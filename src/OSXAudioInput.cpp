#include "AudioInput.hpp"

#include <AudioToolbox/AudioQueue.h>
#include <AudioToolbox/AudioServices.h>
#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CFRunLoop.h>

#define NUM_OF_BUFFERS 4

static void input_callback(
    void *custom_data,
    AudioQueueRef queue,
    AudioQueueBufferRef buffer,
    const AudioTimeStamp *start_time,
    UInt32 num_packets,
    const AudioStreamPacketDescription *packet_desc);

static OSStatus setInputDevice(const char* name);
static char* getDefaultInputDeviceName();

struct private_data {
    AudioStreamBasicDescription format;
    AudioQueueRef inQueue;
    AudioQueueBufferRef inBuffer[NUM_OF_BUFFERS];
    char* defaultDevice;
    bool open = false;
    bool paused = false;
};

void AudioInput::selfInit() {
    this->self = new private_data;
    self->format.mSampleRate = options.sampleRate;
    self->format.mFormatID = kAudioFormatLinearPCM;
    self->format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    self->format.mBitsPerChannel = options.bitsPerSample;
    self->format.mChannelsPerFrame = options.channels;
    self->format.mBytesPerFrame = options.bitsPerSample / 8 * options.channels;
    self->format.mFramesPerPacket = 1;
    self->format.mBytesPerPacket = self->format.mBytesPerFrame * self->format.mFramesPerPacket;
    self->format.mReserved = 0;
    self->open = false;
    self->defaultDevice = getDefaultInputDeviceName();
}

int AudioInput::open() {
    if(self->open) return 1;

    OSStatus status;
    double samplesPerFrame = double(options.sampleRate) * double(options.frameDuration) / 1000.0 * double(options.channels) * options.bitsPerSample / 8;
    status = AudioQueueNewInput(&self->format, input_callback, this, NULL, kCFRunLoopCommonModes, 0, &self->inQueue);
    if(status != 0) return status;

    for(int i = 0; i < NUM_OF_BUFFERS; i++) {
        status = AudioQueueAllocateBuffer(self->inQueue, uint32_t(samplesPerFrame), &self->inBuffer[i]);
        if(status != 0) {
            AudioQueueDispose(self->inQueue, true);
            return status;
        }

        status = AudioQueueEnqueueBuffer(self->inQueue, self->inBuffer[i], 0, NULL);
        if(status != 0) {
            AudioQueueDispose(self->inQueue, true);
            return status;
        }
    }

    status = AudioQueueStart(self->inQueue, NULL);
    if(status != 0) {
        AudioQueueDispose(self->inQueue, true);
        return status;
    }

    self->open = true;
    if(options.devName)
        return setInputDevice(options.devName);
    else return 0;
}

void AudioInput::pause() {
    if(self->paused) {
        AudioQueueStart(self->inQueue, NULL);
        self->paused = false;
    } else {
        AudioQueuePause(self->inQueue);
        self->paused = true;
    }
}

bool AudioInput::isOpen() {
    return self->open;
}

bool AudioInput::isPaused() {
    return !self->open || self->paused;
}

void AudioInput::close() {
    if(self->open) {
        self->open = false;
        AudioQueueDispose(self->inQueue, true);
        setInputDevice(self->defaultDevice);
    }
}

AudioInput::~AudioInput() {
    close();
    delete[] self->defaultDevice;
}

static void input_callback(
    void *custom_data,
    AudioQueueRef queue,
    AudioQueueBufferRef buffer,
    const AudioTimeStamp *start_time,
    UInt32 num_packets,
    const AudioStreamPacketDescription *packet_desc)
{
    AudioInput* ai = (AudioInput*) custom_data;

    if(ai->self->open) {
        ai->callCallback(num_packets * ai->self->format.mBytesPerPacket, buffer->mAudioData);
    }
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

const char* AudioInput::errorCodeToString(int errorCode) {
    //See https://developer.apple.com/library/ios/documentation/MusicAudio/Reference/AudioQueueReference/#//apple_ref/c/econst/kAudioQueueErr_BufferEmpty
    switch(errorCode) {
        case 1: return "Stream was started yet";
        case 2: return "Could not change input device";
        case kAudioQueueErr_InvalidBuffer: return "InvalidBuffer";
        case kAudioQueueErr_BufferEmpty: return "BufferEmpty";
        case kAudioQueueErr_DisposalPending: return "DisposalPending";
        case kAudioQueueErr_InvalidProperty: return "InvalidProperty";
        case kAudioQueueErr_InvalidPropertySize: return "InvalidPropertySize";
        case kAudioQueueErr_InvalidParameter: return "InvalidParameter";
        case kAudioQueueErr_CannotStart: return "CannotStart";
        case kAudioQueueErr_InvalidDevice: return "InvalidDevice";
        case kAudioQueueErr_BufferInQueue: return "BufferInQueue";
        case kAudioQueueErr_InvalidRunState: return "InvalidRunState";
        case kAudioQueueErr_InvalidQueueType: return "InvalidQueueType";
        case kAudioQueueErr_Permissions: return "Permissions";
        case kAudioQueueErr_InvalidPropertyValue: return "InvalidPropertyValue";
        case kAudioQueueErr_PrimeTimedOut: return "PrimeTimedOut";
        case kAudioQueueErr_CodecNotFound: return "CodecNotFound";
        case kAudioQueueErr_InvalidCodecAccess: return "InvalidCodecAccess";
        case kAudioQueueErr_QueueInvalidated: return "QueueInvalidated";
        case kAudioQueueErr_RecordUnderrun: return "RecordUnderrun";
        case kAudioQueueErr_EnqueueDuringReset: return "EnqueueDuringReset";
        case kAudioQueueErr_InvalidOfflineMode: return "InvalidOfflineMode";
        default: return nullptr;
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
// http://stackoverflow.com/questions/4575408/audioobjectgetpropertydata-to-get-a-list-of-input-devices
static void getInputDevicesId(AudioDeviceID* &devices, uint32_t &count) {
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    uint32_t dataSize = 0;
    OSStatus status = AudioHardwareServiceGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize);
    if(kAudioHardwareNoError != status) {
        fprintf(stderr, "AudioObjectGetPropertyDataSize (kAudioHardwarePropertyDevices) failed: %i\n", status);
        devices = nullptr;
        count = 0;
        return;
    }

    count = uint32_t(dataSize / sizeof(AudioDeviceID));
    devices = new AudioDeviceID[count];

    status = AudioHardwareServiceGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize, devices);
    if(kAudioHardwareNoError != status) {
        fprintf(stderr, "AudioObjectGetPropertyData (kAudioHardwarePropertyDevices) failed: %i\n", status);
        delete[] devices;
        devices = nullptr;
        count = 0;
    }
}

static OSStatus setInputDevice(const char* name) {
    AudioDeviceID* devices;
    uint32_t deviceCount, dataSize;
    OSStatus ret = 2;
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMaster
    };

    getInputDevicesId(devices, deviceCount);
    if(deviceCount == 0) return 2;

    CFStringRef desiredDeviceName = CFStringCreateWithBytesNoCopy(
        nullptr,
        (uint8_t*) name,
        strlen(name),
        kCFStringEncodingUTF8,
        false,
        nullptr);

    for(uint32_t i = 0; i < deviceCount; i++) {
        CFStringRef deviceName = nullptr;
        propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
        AudioHardwareServiceGetPropertyData(devices[i], &propertyAddress, 0, nullptr, &dataSize, &deviceName);

        if(CFStringCompare(desiredDeviceName, deviceName, 0) == kCFCompareEqualTo) {
            propertyAddress.mSelector = kAudioHardwarePropertyDefaultInputDevice;
            propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
            propertyAddress.mElement = kAudioObjectPropertyElementMaster;
            AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, sizeof(AudioDeviceID), &devices[i]);
            CFRelease(deviceName);
            ret = 0;
            break;
        }

        CFRelease(deviceName);
    }

    delete[] devices;
    CFRelease(desiredDeviceName);
    return ret;
}

static char* getDefaultInputDeviceName() {
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultInputDevice;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;
    AudioDeviceID device = 0;
    uint32_t size = sizeof(AudioDeviceID);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &size, &device);

    CFStringRef deviceName = nullptr;
    propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
    propertyAddress.mScope = kAudioDevicePropertyScopeInput;
    size = sizeof(CFStringRef);
    AudioHardwareServiceGetPropertyData(device, &propertyAddress, 0, nullptr, &size, &deviceName);

    char* mutableStrDeviceName = new char[CFStringGetLength(deviceName)+1];
    if(!CFStringGetCString(deviceName, mutableStrDeviceName, CFStringGetLength(deviceName)+1, kCFStringEncodingUTF8)) {
        delete[] mutableStrDeviceName;
        fprintf(stderr, "Cannot obtain const char* from CFString correctly %ld\n", CFStringGetLength(deviceName));
    }

    CFRelease(deviceName);
    return mutableStrDeviceName;
}

void AudioInput::getInputDevices(std::vector<std::string> &list) {
    AudioDeviceID* devices;
    uint32_t deviceCount;
    OSStatus status;

    getInputDevicesId(devices, deviceCount);

    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMaster
    };
    for(uint32_t i = 0; i < deviceCount; i++) {
        CFStringRef deviceName = NULL;
        uint32_t dataSize = sizeof(deviceName);
        propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
        status = AudioHardwareServiceGetPropertyData(devices[i], &propertyAddress, 0, NULL, &dataSize, &deviceName);
        if(kAudioHardwareNoError != status) {
            fprintf(stderr, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceNameCFString) failed: %i\n", status);
            continue;
        }

        dataSize = 0;
        propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
        status = AudioHardwareServiceGetPropertyDataSize(devices[i], &propertyAddress, 0, NULL, &dataSize);
        if(kAudioHardwareNoError != status) {
            fprintf(stderr, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration) failed: %i\n", status);
            CFRelease(deviceName);
            continue;
        }

        AudioBufferList *bufferList = (AudioBufferList *) malloc(dataSize);
        status = AudioHardwareServiceGetPropertyData(devices[i], &propertyAddress, 0, NULL, &dataSize, bufferList);
        if(kAudioHardwareNoError != status || 0 == bufferList->mNumberBuffers) {
            if(kAudioHardwareNoError != status)
                fprintf(stderr, "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration) failed: %i\n", status);
            free(bufferList);
            CFRelease(deviceName);
            continue;
        }

        const char* strDeviceName = CFStringGetCStringPtr(deviceName, kCFStringEncodingUTF8);
        if(strDeviceName == NULL) {
            char* mutableStrDeviceName = new char[CFStringGetLength(deviceName)+1];
            if(!CFStringGetCString(deviceName, mutableStrDeviceName, CFStringGetLength(deviceName)+1, kCFStringEncodingUTF8)) {
                delete[] mutableStrDeviceName;
                fprintf(stderr, "Cannot obtain const char* from CFString correctly\n");
            }

            list.push_back(std::string(mutableStrDeviceName));
            delete[] mutableStrDeviceName;
        } else {
            list.push_back(std::string(strDeviceName));
        }

        CFRelease(deviceName);
    }

    delete[] devices;
}
#pragma clang diagnostic pop

void AudioInput::staticInit() {

}

void AudioInput::staticDeinit() {

}
