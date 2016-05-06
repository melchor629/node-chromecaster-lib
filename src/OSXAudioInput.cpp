#include "AudioInput.hpp"

#include <AudioToolbox/AudioQueue.h>
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

struct private_data {
    AudioStreamBasicDescription format;
    AudioQueueRef inQueue;
    AudioQueueBufferRef inBuffer[NUM_OF_BUFFERS];
    bool open = false;
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
}

int AudioInput::open() {
    if(self->open) return 1;

    OSStatus status;
    double samplesPerFrame = double(options.sampleRate) * double(options.frameDuration) / 1000.0 * double(options.channels);
    status = AudioQueueNewInput(&self->format, input_callback, this, NULL, kCFRunLoopCommonModes, 0, &self->inQueue);
    if(status == 0) return status;

    for(int i = 0; i < NUM_OF_BUFFERS; i++) {
        status = AudioQueueAllocateBuffer(self->inQueue, samplesPerFrame, &self->inBuffer[i]);
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
    return 0;
}

bool AudioInput::isOpen() {
    return self->open;
}

void AudioInput::close() {
    if(self->open) {
        self->open = false;
        AudioQueueDispose(self->inQueue, true);
    }
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
        ai->callCallback(num_packets, buffer->mAudioData);
    }
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

const char* AudioInput::errorCodeToString(int errorCode) {
    //See https://developer.apple.com/library/ios/documentation/MusicAudio/Reference/AudioQueueReference/#//apple_ref/c/econst/kAudioQueueErr_BufferEmpty
    switch(errorCode) {
        case kAudioQueueErr_InvalidBuffer:
            return "InvalidBuffer";
        case kAudioQueueErr_BufferEmpty:
            return "BufferEmpty";
        case kAudioQueueErr_DisposalPending:
            return "DisposalPending";
        case kAudioQueueErr_InvalidProperty:
            return "InvalidProperty";
        case kAudioQueueErr_InvalidPropertySize:
            return "InvalidPropertySize";
        case kAudioQueueErr_InvalidParameter:
            return "InvalidParameter";
        case kAudioQueueErr_CannotStart:
            return "CannotStart";
        case kAudioQueueErr_InvalidDevice:
            return "InvalidDevice";
        case kAudioQueueErr_BufferInQueue:
            return "BufferInQueue";
        case kAudioQueueErr_InvalidRunState:
            return "InvalidRunState";
        case kAudioQueueErr_InvalidQueueType:
            return "InvalidQueueType";
        case kAudioQueueErr_Permissions:
            return "Permissions";
        case kAudioQueueErr_InvalidPropertyValue:
            return "InvalidPropertyValue";
        case kAudioQueueErr_PrimeTimedOut:
            return "PrimeTimedOut";
        case kAudioQueueErr_CodecNotFound:
            return "CodecNotFound";
        case kAudioQueueErr_InvalidCodecAccess:
            return "InvalidCodecAccess";
        case kAudioQueueErr_QueueInvalidated:
            return "QueueInvalidated";
        case kAudioQueueErr_RecordUnderrun:
            return "RecordUnderrun";
        case kAudioQueueErr_EnqueueDuringReset:
            return "EnqueueDuringReset";
        case kAudioQueueErr_InvalidOfflineMode:
            return "InvalidOfflineMode";
        default:
            return nullptr;
    }
}
