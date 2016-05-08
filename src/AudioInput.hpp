#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include <stdint.h>
#include <functional>

class AudioInput {
public:
    typedef void (*AudioInputCallback)(uint32_t, const void*, void*);

    struct Options {
        uint32_t sampleRate;
        uint8_t bitsPerSample;
        uint8_t channels;
        uint16_t frameDuration;
        const char* devName;
    };

    static const char* errorCodeToString(int);

    AudioInput(const Options &opt) : options(opt) {
        selfInit();
    }
    ~AudioInput();

    void setInputCallback(AudioInputCallback cbk, void* userData = nullptr) {
        this->cbk = cbk;
        this->userData = userData;
    }

    int open();
    void close();
    void pause();
    bool isOpen();
    bool isPaused();

    Options options;

    void callCallback(uint32_t size, const void* pcm) {
        if(cbk) {
            cbk(size, pcm, userData);
        }
    }

    void selfInit();

    AudioInputCallback cbk = nullptr;
    void* userData;
    struct private_data* self;
};

#endif
