#include "AudioInput.hpp"
#include <portaudio.h>
#include <nan.h>

struct private_data {
    PaStream* stream = nullptr;
    bool isPaused = false;
};

int stream_cbk(const void *input,
               void *output,
               unsigned long frameCount,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void *userData);

void AudioInput::staticInit() {
    int err = Pa_Initialize();
    if(err != paNoError) {
        Nan::ThrowError(Pa_GetErrorText(err));
    }
}

void AudioInput::staticDeinit() {
    int err = Pa_Terminate();
    if(err != paNoError) {
        Nan::ThrowError(Pa_GetErrorText(err));
    }
}

void AudioInput::getInputDevices(std::vector<std::string> &list) {
    int numDevices = Pa_GetDeviceCount();
    if(numDevices < 0) {
        Nan::ThrowError(Pa_GetErrorText(numDevices));
        return;
    }

    for(int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* device = Pa_GetDeviceInfo(i);
        if(device->maxInputChannels > 0) {
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
    //void* pcm = malloc(bytes);
    void* pcm = (void*) new char[bytes];
    memcpy(pcm, input, bytes);
    self->callCallback(
        bytes,
        pcm
    );
    return paContinue;
}
