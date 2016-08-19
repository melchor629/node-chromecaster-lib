#include "AudioInput.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <comdef.h>
#include <thread>
#include <avrt.h>
#include <atomic>
#include <mutex>
#include <locale>
#include <codecvt>
#include <string>

// https://msdn.microsoft.com/es-es/library/windows/desktop/dd370800(v=vs.85).aspx
// https://msdn.microsoft.com/es-es/library/windows/desktop/dd370812(v=vs.85).aspx
// https://msdn.microsoft.com/es-es/library/windows/desktop/dd371455(v=vs.85).aspx
// https://msdn.microsoft.com/es-es/library/windows/desktop/dd316556(v=vs.85).aspx
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

#define IF_FAILED(hr, message, ...) \
    if(FAILED(hr)) { \
        fprintf(stderr, "%d: " message "\n", __LINE__, __VA_ARGS__);\
        return; \
    }

#define IF_FAILED2(hr, message, ...) \
    if(FAILED(hr)) { \
        fprintf(stderr, "%d: " message "\n", __LINE__, __VA_ARGS__);\
        return nullptr; \
    }

static IMMDevice* getDeviceWithName(const char* name);
static IMMDevice* getDefaultDevice();
static char* toUtf8(const wchar_t* wstr);
static wchar_t* toUnicode(const char* str);

//Needed to avoid memory leaks on AudioInput::errorCodeToString()
char* errorStringTmp = nullptr;

struct private_data {
    //Device & Audio capture stuff
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;

    //OnSamplesAvailable and OnStopCalled events
    HANDLE eventHandle = 0, shutdownHandle = 0;

    //About sound thread and its protection
    std::thread audioThread;
    std::mutex mutex;
    uint8_t* buffers[4] = { nullptr, nullptr, nullptr, nullptr };
    size_t sbuffers[4] = { 0, 0, 0, 0 };
    uint8_t pos = 0;

    //the standard
    bool open = false;
    bool paused = false;
};

void AudioInput::selfInit() {
    self = new private_data;
}

int AudioInput::open() {
    if(self->open) return 1;

    HRESULT hr;
    WAVEFORMATEXTENSIBLE pwfx;
    IMMDevice* device;
    if(options.devName != nullptr) {
        device = getDeviceWithName(options.devName);
    } else {
        device = getDefaultDevice();
    }
    if(device == nullptr) return -1;

    hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**) &self->client);
    if(FAILED(hr)) { close(); return hr; }

    self->device = device;

    pwfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    pwfx.Format.nChannels = options.channels;
    pwfx.Format.nSamplesPerSec = options.sampleRate;
    pwfx.Format.nBlockAlign = (options.channels * options.bitsPerSample) / 8;
    pwfx.Format.nAvgBytesPerSec = options.sampleRate * pwfx.Format.nBlockAlign;
    pwfx.Format.wBitsPerSample = options.bitsPerSample;
    pwfx.Samples.wValidBitsPerSample = options.bitsPerSample;
    pwfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    pwfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    pwfx.Format.cbSize = 22;

    hr = self->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                  0,
                                  0,
                                  (WAVEFORMATEX*) &pwfx,
                                  nullptr);
    if(FAILED(hr)) { close(); return hr; }

    hr = self->client->GetService(IID_IAudioCaptureClient, (void**) &self->capture);
    if(FAILED(hr)) { close(); return hr; }

    self->eventHandle = CreateEventEx(0, L"Audio ready", 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if(self->eventHandle == NULL) { close(); return -2; }

    self->shutdownHandle = CreateEventEx(0, L"Shutdown audio service", 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if(self->shutdownHandle == NULL) { close(); return -2; }

    hr = self->client->SetEventHandle(self->eventHandle);
    if(FAILED(hr)) { close(); return hr; }

    self->open = true;
    self->paused = false;
    self->audioThread = std::thread([this, pwfx] {
        DWORD taskIndex = 0;
        HANDLE hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
        DWORD flags = 0;
        uint32_t framesAvailable = 0, packetLength = 0;
        HANDLE handles[] = { self->eventHandle, self->shutdownHandle };
        bool JustDoIt = true;

        AudioInput::staticInit();

        while(JustDoIt) {
            uint8_t* data = nullptr;
            DWORD rv = WaitForMultipleObjects(2, handles, false, 2000);
            if(rv == WAIT_TIMEOUT) {
                //Timeout has been reached, stop thread
                fprintf(stderr, "Timeout for Audio Event Handle\n");
                JustDoIt = false;
            } else if(rv == WAIT_OBJECT_0 + 1) {
                //AudioInput::close() called, stop thread
                JustDoIt = false;
            } else {
                //We got some audio, lets retrieve it
                self->mutex.lock();
                self->capture->GetNextPacketSize(&packetLength);

                //Retrieve all sound stored in the CaptureClient before waiting again
                while(packetLength != 0) {
                    self->capture->GetBuffer(&data, &framesAvailable, &flags, NULL, NULL);

                    if(framesAvailable == 0) continue;
                    size_t size = pwfx.Format.nBlockAlign * framesAvailable;
                    if(self->sbuffers[self->pos] < size) {
                        delete[] self->buffers[self->pos];
                        self->buffers[self->pos] = new uint8_t[size];
                        self->sbuffers[self->pos] = size;
                    }

                    if(data == NULL || flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        memset(self->buffers[self->pos], 0, size);
                        printf("!! ");
                    } else {
                        memcpy(self->buffers[self->pos], data, size);
                    }

                    if(!self->paused) {
                        //WASAPI has not pause method, so will "force" some kind of pause
                        //not sending the data to the callback
                        this->callCallback(framesAvailable, self->buffers[self->pos]);
                    }
                    self->capture->ReleaseBuffer(framesAvailable);
                    self->pos = (self->pos + 1) % 4;

                    self->capture->GetNextPacketSize(&packetLength);
                }

                self->mutex.unlock();
            }
        }

        if(hTask != NULL) {
            AvRevertMmThreadCharacteristics(hTask);
        }

        CoUninitialize();
    });

    hr = self->client->Start();
    if(FAILED(hr)) { close(); return hr; }

    return 0;
}

void AudioInput::pause() {
    if(self->open) {
        self->mutex.lock();
        self->paused = !self->paused;
        self->mutex.unlock();
    }
}

bool AudioInput::isOpen() {
    return self->open;
}

bool AudioInput::isPaused() {
    return self->open && self->paused;
}

void AudioInput::close() {
    if(self->open) {
        SetEvent(self->shutdownHandle);
        self->mutex.lock();
        self->client->Stop();
        self->mutex.unlock();
        self->open = false;
        self->audioThread.join();
    }

    if(self->capture) {
        self->capture->Release();
        self->capture = nullptr;
    }

    if(self->client) {
        self->client->Release();
        self->client = nullptr;
    }

    if(self->device) {
        self->device->Release();
        self->device = nullptr;
    }

    if(self->eventHandle) {
        CloseHandle(self->eventHandle);
        self->eventHandle = NULL;
    }

    if(self->shutdownHandle) {
        CloseHandle(self->shutdownHandle);
        self->shutdownHandle = NULL;
    }

    memset(self->sbuffers, 0, 4 * sizeof(size_t));
}

AudioInput::~AudioInput() {
    close();
    for(unsigned i = 0; i < 4; i++) {
        if(self->buffers[i]) delete[] self->buffers[i];
    }
    delete self;
}

const char* AudioInput::errorCodeToString(int errorCode) {
    switch(errorCode) {
        case 1: return "Already open";
        case -1: return "Audio Device not found";
        case -2: return "Cannot create audio event handle";
        case AUDCLNT_E_WRONG_ENDPOINT_TYPE: return "Wrong device type";
        case AUDCLNT_E_UNSUPPORTED_FORMAT: return "Unsupported audio format";
        case AUDCLNT_E_SERVICE_NOT_RUNNING: return "Windows Audio Service is unloaded";
        case AUDCLNT_E_NOT_INITIALIZED: return "AUDCLNT_E_NOT_INITIALIZED";
        case AUDCLNT_E_ALREADY_INITIALIZED: return "AUDCLNT_E_ALREADY_INITIALIZED";
        case AUDCLNT_E_DEVICE_INVALIDATED: return "AUDCLNT_E_DEVICE_INVALIDATED";
        case AUDCLNT_E_NOT_STOPPED: return "AUDCLNT_E_NOT_STOPPED";
        case AUDCLNT_E_BUFFER_TOO_LARGE: return "AUDCLNT_E_BUFFER_TOO_LARGE";
        case AUDCLNT_E_OUT_OF_ORDER: return "AUDCLNT_E_OUT_OF_ORDER";
        case AUDCLNT_E_INVALID_SIZE: return "AUDCLNT_E_INVALID_SIZE";
        case AUDCLNT_E_DEVICE_IN_USE: return "AUDCLNT_E_DEVICE_IN_USE";
        case AUDCLNT_E_BUFFER_OPERATION_PENDING: return "AUDCLNT_E_BUFFER_OPERATION_PENDING";
        case AUDCLNT_E_THREAD_NOT_REGISTERED: return "AUDCLNT_E_THREAD_NOT_REGISTERED";
        case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED: return "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
        case AUDCLNT_E_ENDPOINT_CREATE_FAILED: return "AUDCLNT_E_ENDPOINT_CREATE_FAILED";
        case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED: return "AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED";
        case AUDCLNT_E_EXCLUSIVE_MODE_ONLY: return "AUDCLNT_E_EXCLUSIVE_MODE_ONLY";
        case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL: return "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL";
        case AUDCLNT_E_EVENTHANDLE_NOT_SET: return "AUDCLNT_E_EVENTHANDLE_NOT_SET";
        case AUDCLNT_E_INCORRECT_BUFFER_SIZE: return "AUDCLNT_E_INCORRECT_BUFFER_SIZE";
        case AUDCLNT_E_BUFFER_SIZE_ERROR: return "AUDCLNT_E_BUFFER_SIZE_ERROR";
        case AUDCLNT_E_CPUUSAGE_EXCEEDED: return "AUDCLNT_E_CPUUSAGE_EXCEEDED";
        case AUDCLNT_E_BUFFER_ERROR: return "AUDCLNT_E_BUFFER_ERROR";
        case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED: return "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";
        case AUDCLNT_E_INVALID_DEVICE_PERIOD: return "AUDCLNT_E_INVALID_DEVICE_PERIOD";
        case AUDCLNT_E_INVALID_STREAM_FLAG: return "AUDCLNT_E_INVALID_STREAM_FLAG";
        case AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE: return "AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE";
        case AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES: return "AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES";
        case AUDCLNT_E_OFFLOAD_MODE_ONLY: return "AUDCLNT_E_OFFLOAD_MODE_ONLY";
        case AUDCLNT_E_NONOFFLOAD_MODE_ONLY: return "AUDCLNT_E_NONOFFLOAD_MODE_ONLY";
        case AUDCLNT_E_RESOURCES_INVALIDATED: return "AUDCLNT_E_RESOURCES_INVALIDATED";
        case AUDCLNT_E_RAW_MODE_UNSUPPORTED: return "AUDCLNT_E_RAW_MODE_UNSUPPORTED";
        case AUDCLNT_S_BUFFER_EMPTY: return "AUDCLNT_S_BUFFER_EMPTY";
        case AUDCLNT_S_THREAD_ALREADY_REGISTERED: return "AUDCLNT_S_THREAD_ALREADY_REGISTERED";
        case AUDCLNT_S_POSITION_STALLED: return "AUDCLNT_S_POSITION_STALLED";
        default: {
            _com_error err(errorCode);
            const wchar_t* errmsg = err.ErrorMessage();
            if(errorStringTmp) delete[] errorStringTmp;
            errorStringTmp = toUtf8(errmsg);
            return errorStringTmp;
        }
    }
}

void AudioInput::getInputDevices(std::vector<std::string> &list) {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDeviceCollection *collection = NULL;
    IMMDevice *device = NULL;
    IPropertyStore *props = NULL;
    wchar_t* pwszID = NULL;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        (void**) &enumerator);
    IF_FAILED(hr, "Cannot create Audio Device Enumerator");

    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    IF_FAILED(hr, "Cannot create iterator");

    uint32_t count;
    hr = collection->GetCount(&count);
    IF_FAILED(hr, "Cannot get count of collection");

    if(count == 0) return;

    for(uint32_t i = 0; i < count; i++) {
        hr = collection->Item(i, &device);
        IF_FAILED(hr, "Cannot get one device %d", i);

        hr = device->GetId(&pwszID);
        IF_FAILED(hr, "Cannot get device id %d", i);

        hr = device->OpenPropertyStore(STGM_READ, &props);
        IF_FAILED(hr, "Cannot get device properties %d", i);

        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
        IF_FAILED(hr, "Cannot get device name %d", i);

        char* str = toUtf8(varName.pwszVal);
        list.push_back(std::string(str));
        delete[] str;

        CoTaskMemFree(pwszID);
        pwszID = nullptr;
        PropVariantClear(&varName);
        props->Release(); props = nullptr;
        device->Release(); device = nullptr;
    }

    enumerator->Release();
    collection->Release();
}

void AudioInput::staticInit() {
    CoInitializeEx(nullptr, COINIT_SPEED_OVER_MEMORY | COINIT_MULTITHREADED);
}

void AudioInput::staticDeinit() {
    CoUninitialize();
    if(errorStringTmp != nullptr) {
        delete[] errorStringTmp;
    }
}

static IMMDevice* getDeviceWithName(const char* name) {
    wchar_t* wsname = toUnicode(name);

    HRESULT hr = S_OK;
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDeviceCollection *collection = NULL;
    IMMDevice *device = NULL;
    IPropertyStore *props = NULL;
    wchar_t* pwszID = NULL;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        (void**) &enumerator);
    IF_FAILED2(hr, "Cannot create Audio Device Enumerator");

    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    IF_FAILED2(hr, "Cannot create iterator");

    uint32_t count;
    hr = collection->GetCount(&count);
    IF_FAILED2(hr, "Cannot get count of collection");

    if(count == 0) return nullptr;

    for(uint32_t i = 0; i < count; i++) {
        hr = collection->Item(i, &device);
        IF_FAILED2(hr, "Cannot get one device %d", i);

        hr = device->GetId(&pwszID);
        IF_FAILED2(hr, "Cannot get device id %d", i);

        hr = device->OpenPropertyStore(STGM_READ, &props);
        IF_FAILED2(hr, "Cannot get device properties %d", i);

        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
        IF_FAILED2(hr, "Cannot get device name %d", i);

        if(wcscmp(wsname, varName.pwszVal) == 0) {
            CoTaskMemFree(pwszID);
            PropVariantClear(&varName);
            props->Release();
            enumerator->Release();
            collection->Release();
            delete[] wsname;
            return device;
        }

        CoTaskMemFree(pwszID);
        pwszID = nullptr;
        PropVariantClear(&varName);
        props->Release(); props = nullptr;
        device->Release(); device = nullptr;
    }

    enumerator->Release();
    collection->Release();
    delete[] wsname;
    return nullptr;
}

static IMMDevice* getDefaultDevice() {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDevice* device = nullptr;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        (void**) &enumerator);
    IF_FAILED2(hr, "Cannot create Audio Device Enumerator");

    hr = enumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &device);
    IF_FAILED2(hr, "Cannot get default audio device");

    enumerator->Release();
    return device;
}

static char* toUtf8(const wchar_t* wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string sstr = converter.to_bytes(wstr);
    char* str = new char[sstr.length() + 1];
    strcpy_s(str, sstr.length() + 1, sstr.c_str());
    return str;
}

static wchar_t* toUnicode(const char* str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring wsstr = converter.from_bytes(str);
    wchar_t* wstr = new wchar_t[wsstr.length() + 1];
    wcscpy_s(wstr, wsstr.length() + 1, wsstr.c_str());
    return wstr;
}
