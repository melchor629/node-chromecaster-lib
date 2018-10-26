#include "AudioInput.hpp"
#include "portaudio.h"
#include <nan.h>
#include "dl.hpp"

#ifdef _WIN32
extern "C" int PaWasapi_IsLoopback(PaDeviceIndex deviceId);
#endif

struct private_data {
    PaStream* stream = nullptr;
    bool isPaused = false;
};

static Library* portaudio = nullptr;
static bool loadLibrary(std::string path = "");
static void unloadLibrary();

int stream_cbk(const void *input,
               void *output,
               unsigned long frameCount,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void *userData);

void AudioInput::staticInit(std::string path) {
    if(portaudio == nullptr) {
        if(loadLibrary(path)) {
            int err = Pa_Initialize();
            if(err != paNoError) {
                Nan::ThrowError(Pa_GetErrorText(err));
            }
        } else if(!path.empty()) {
            auto errStr = "Could not load native library: " + Library::getLastError() + " - " + path;
            Nan::ThrowError(errStr.c_str());
        }
    } else {
        Nan::ThrowError("Library 'portaudio' has already been loaded");
    }
}

void AudioInput::staticDeinit() {
    int err = Pa_Terminate();
    if(err != paNoError) {
        Nan::ThrowError(Pa_GetErrorText(err));
    }
    unloadLibrary();
}

bool AudioInput::isLoaded() {
    return portaudio != nullptr;
}

void AudioInput::getInputDevices(std::vector<std::string> &list) {
    int numDevices = Pa_GetDeviceCount();
    if(numDevices < 0) {
        Nan::ThrowError(Pa_GetErrorText(numDevices));
        return;
    }

    for(int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* device = Pa_GetDeviceInfo(i);
#ifdef _WIN32
        if(device->maxInputChannels > 0 || PaWasapi_IsLoopback(i)) {
#else
        if(device->maxInputChannels > 0) {
#endif
            const PaHostApiInfo* hostApi = Pa_GetHostApiInfo(device->hostApi);
            list.push_back(std::string("[") + hostApi->name + "] " + device->name);
        }
    }
}

const char* AudioInput::errorCodeToString(int code) {
    return Pa_GetErrorText(code);
}

static inline int searchDeviceId(const char* devName) {
    if(devName != nullptr) {
        int numDevices = Pa_GetDeviceCount();
        std::string name = devName;
        for(int i = 0; i < numDevices; i++) {
            const PaDeviceInfo* device = Pa_GetDeviceInfo(i);
            if(device->maxInputChannels > 0) {
                if(name.find(device->name) != std::string::npos) {
                    return i;
                }
            }
        }

        return -1;
    } else {
        return Pa_GetDefaultInputDevice();
    }
}

static inline int bitsPerSampleToSampleFormat(int bitsPerSample) {
    switch(bitsPerSample) {
        case 8: return paInt8;
        case 16: return paInt16;
        case 24: return paInt24;
        case 32: return paFloat32;
        default: return paInt16;
    }
}

void AudioInput::selfInit() {
    if(portaudio == nullptr) {
        Nan::ThrowError("Native library is not loaded. Load it using AudioInput.loadNativeLibrary(\"pathToLibrary\");");
        return;
    }

    self = new private_data;

    PaStreamParameters params;
    memset(&params, 0, sizeof(params));

    params.channelCount = options.channels;
    params.device = searchDeviceId(options.devName);
    params.sampleFormat = bitsPerSampleToSampleFormat(options.bitsPerSample);
    params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowInputLatency;

    if(Pa_IsFormatSupported(&params, nullptr, options.sampleRate) != paNoError) {
        Nan::ThrowError("Unsupported audio format");
        return;
    }

    int err = Pa_OpenStream(
        &self->stream,
        &params,
        nullptr,
        options.sampleRate,
        options.frameDuration != 0 ? options.sampleRate * options.frameDuration / 1000 : paFramesPerBufferUnspecified,
        paNoFlag,
        stream_cbk,
        (void*) this
    );
    if(err != paNoError) {
        Nan::ThrowError(Pa_GetErrorText(err));
    }
}

int AudioInput::open() {
    return Pa_StartStream(self->stream);
}

void AudioInput::pause() {
    int err;
    if(self->isPaused) {
        err = Pa_StartStream(self->stream);
    } else {
        err = Pa_StopStream(self->stream);
    }

    if(err != paNoError) {
        Nan::ThrowError(Pa_GetErrorText(err));
    } else {
        self->isPaused = !self->isPaused;
    }
}

void AudioInput::close() {
    int err = Pa_AbortStream(self->stream);
    if(err != paNoError) {
        Nan::ThrowError(Pa_GetErrorText(err));
        return;
    }
    err = Pa_CloseStream(self->stream);
    if(err != paNoError) {
        Nan::ThrowError(Pa_GetErrorText(err));
        return;
    }
    self->stream = nullptr;
}

bool AudioInput::isOpen() {
    return self->stream != nullptr;
}

bool AudioInput::isPaused() {
    return self->isPaused;
}

AudioInput::~AudioInput() {
    if(isOpen()) close();
    delete self;
}



int stream_cbk(const void *input,
               void *output,
               unsigned long frameCount,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void *userData) {
    AudioInput* self = (AudioInput*) userData;
    if(statusFlags) {
        printf("[PortAudio]: PaStreamCallbackFlag set:");
        if(statusFlags & paInputOverflow) printf(" InputOverflow");
        if(statusFlags & paInputUnderflow) printf(" InputUnderflow");
        printf("\n");
    }
    size_t bytes = frameCount * self->options.bitsPerSample / 8 * self->options.channels;
    void* pcm = (void*) new char[bytes];
    memcpy(pcm, input, bytes);
    self->callCallback(
        bytes,
        pcm
    );
    return paContinue;
}


static bool loadLibrary(std::string path) {
    if(portaudio == nullptr) {
        if(path.empty()) {
            portaudio = Library::load("libportaudio");
            if(portaudio == nullptr) {
#ifndef WIN32
                portaudio = Library::load("libportaudio", "so.2");
                if(portaudio == nullptr) {
                    portaudio = Library::load("./lib/libportaudio");
                }
#else
                portaudio = Library::load("portaudio_x64", "dll");
                if(portaudio == nullptr) {
                    portaudio = Library::load("./lib/portaudio");
                    if(portaudio == nullptr) {
                        portaudio = Library::load("./lib/portaudio_x64");
                    }
                }
#endif
            }
        } else {
            portaudio = Library::load(path);
        }

        return portaudio != nullptr;
    }

    return true;
}

static void unloadLibrary() {
    delete portaudio;
}

extern "C" PaError Pa_Initialize(void) {
    static PaError(*func)(void) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_Initialize");
    return func();
}

extern "C" PaError Pa_Terminate(void) {
    static PaError(*func)(void) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_Terminate");
    return func();
}

extern "C" const char* Pa_GetErrorText(PaError err) {
    static const char*(*func)(PaError) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_GetErrorText");
    return func(err);
}

extern "C" PaError Pa_GetDeviceCount(void) {
    static PaError(*func)(void) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_GetDeviceCount");
    return func();
}

extern "C" const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex index) {
    static const PaDeviceInfo*(*func)(PaDeviceIndex) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_GetDeviceInfo");
    return func(index);
}

extern "C" const PaHostApiInfo* Pa_GetHostApiInfo(PaHostApiIndex index) {
    static const PaHostApiInfo*(*func)(PaHostApiIndex) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_GetHostApiInfo");
    return func(index);
}

extern "C" PaDeviceIndex Pa_GetDefaultInputDevice(void) {
    static PaDeviceIndex(*func)(void) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_GetDefaultInputDevice");
    return func();
}

extern "C" PaError Pa_IsFormatSupported(const PaStreamParameters* in, const PaStreamParameters* out, double sampleRate) {
    static PaError(*func)(const PaStreamParameters*, const PaStreamParameters*, double) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_IsFormatSupported");
    return func(in, out, sampleRate);
}

extern "C" PaError Pa_OpenStream(PaStream** stream,
                       const PaStreamParameters * in,
                       const PaStreamParameters * out,
                       double sampleRate,
                       unsigned long time,
                       PaStreamFlags flags,
                       PaStreamCallback * cbk,
                       void * userData) {
    static
    PaError(*func)(PaStream**,
                   const PaStreamParameters *,
                   const PaStreamParameters *,
                   double,
                   unsigned long,
                   PaStreamFlags,
                   PaStreamCallback *,
                   void *) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_OpenStream");
    return func(stream, in, out, sampleRate, time, flags, cbk, userData);
}

extern "C" PaError Pa_StartStream(PaStream* stream) {
    static PaError(*func)(PaStream*) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_StartStream");
    return func(stream);
}

extern "C" PaError Pa_StopStream(PaStream* stream) {
    static PaError(*func)(PaStream*) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_StopStream");
    return func(stream);
}

extern "C" PaError Pa_AbortStream(PaStream* stream) {
    static PaError(*func)(PaStream*) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_AbortStream");
    return func(stream);
}

extern "C" PaError Pa_CloseStream(PaStream* stream) {
    static PaError(*func)(PaStream*) = nullptr;
    if(func == nullptr) func = portaudio->getSymbolAddress<decltype(func)>("Pa_CloseStream");
    return func(stream);
}

extern "C" int PaWasapi_IsLoopback(PaDeviceIndex deviceId) {
    //Avoid crashes when portaudio dll has not the Audacity patch
    static bool loaded = false;
    static int(*func)(PaDeviceIndex) = nullptr;
    if(!loaded) {
        func = portaudio->getSymbolAddress<decltype(func)>("PaWasapi_IsLoopback");
        loaded = true;
    }
    if(loaded && func != nullptr) {
        return func(deviceId);
    }
    return false; //Return false by default (it is ok)
}
