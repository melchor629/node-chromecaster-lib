#include "AudioInput.hpp"

#include <pulse/context.h>
#include <pulse/thread-mainloop.h>
#include <pulse/stream.h>
#include <pulse/sample.h>
#include <pulse/error.h>
#include <thread>
#include <cstring>

#define NUM_OF_BUFFERS 4

struct private_data {
    bool open = false;
    bool paused = false;

    pa_threaded_mainloop* mloop;
    pa_proplist* list;
    pa_context* ctx;
    pa_stream* stream;

    pa_context_state contextState = PA_CONTEXT_UNCONNECTED;

    void* buffers[NUM_OF_BUFFERS];
    int currentBuffer = 0;
};

static void paStreamNotify(pa_stream *p, void *userdata);
static void paStreamRequest(pa_stream *p, size_t bytes, void* userdata);
static void paContextNotify(pa_context *c, void *userdata);


pa_sample_format_t bpsToFormat(uint32_t bps) {
    switch(bps) {
        case 8: return PA_SAMPLE_U8;
        case 16: return (PA_SAMPLE_S16LE);
        case 24: return (PA_SAMPLE_S24LE);
        case 32: return (PA_SAMPLE_FLOAT32LE);
        default: return PA_SAMPLE_INVALID;
    }
}

void AudioInput::selfInit() {
    self = new private_data;

    self->mloop = pa_threaded_mainloop_new();
    pa_threaded_mainloop_start(self->mloop);
    self->ctx = pa_context_new(pa_threaded_mainloop_get_api(self->mloop), "Chromecaster Library");
    pa_context_connect(self->ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    pa_context_set_state_callback(self->ctx, paContextNotify, this);

    while(self->contextState != PA_CONTEXT_READY && self->contextState != PA_CONTEXT_FAILED)
        std::this_thread::yield();

    self->list = pa_proplist_new();
    pa_proplist_sets(self->list, PA_PROP_APPLICATION_ID, "com.melchor629.chromecaster-lib");
    pa_proplist_sets(self->list, PA_PROP_APPLICATION_NAME, "Node Chromecaster Library");
}

int AudioInput::open() {
    if(self->open) return 0;

    pa_sample_spec spec = { bpsToFormat(options.bitsPerSample), options.sampleRate, options.channels };
    pa_channel_map map = { options.channels, { PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT }};
    self->stream = pa_stream_new_with_proplist(self->ctx, "AudioInputNative", &spec, &map, self->list);

    if(self->stream == nullptr) {
        return pa_context_errno(self->ctx);
    }

    double samplesPerFrame = double(options.sampleRate) * double(options.frameDuration) / 1000.0 * double(options.channels);
    pa_buffer_attr bufferAttr = { uint32_t(samplesPerFrame), 0, 0, 0, uint32_t(-1) };
    int status = pa_stream_connect_record(self->stream, options.devName, &bufferAttr, PA_STREAM_ADJUST_LATENCY);

    if(status < 0) {
        return pa_context_errno(self->ctx);
    }

    for(int i = 0; i < NUM_OF_BUFFERS; i++) {
        self->buffers[i] = malloc(size_t(samplesPerFrame));
    }

    pa_stream_set_state_callback(self->stream, &paStreamNotify, this);
    pa_stream_set_read_callback(self->stream, &paStreamRequest, this);
    self->open = true;

    return 0;
}

void AudioInput::pause() {
    if(self->open) {
        self->paused = !self->paused;
        pa_stream_cork(self->stream, self->paused, nullptr, nullptr);
    }
}

void AudioInput::close() {
    if(self->open) {
        pa_stream_disconnect(self->stream);
        self->open = false;
        for(int i = 0; i < NUM_OF_BUFFERS; i++) {
            free(self->buffers[i]);
            self->buffers[i] = nullptr;
        }
    }
}

bool AudioInput::isOpen() {
    return self->open;
}

bool AudioInput::isPaused() {
    return !self->open || self->paused;
}

AudioInput::~AudioInput() {
    this->close();
    pa_proplist_free(self->list);
    pa_stream_unref(self->stream);
    pa_context_disconnect(self->ctx);
    pa_context_unref(self->ctx);
    pa_threaded_mainloop_stop(self->mloop);
    pa_threaded_mainloop_free(self->mloop);
    delete self;
}


static void paStreamRequest(pa_stream *p, size_t bytes, void* userdata) {
    AudioInput* ai = (AudioInput*) userdata;

    const void* pcm;
    int status = pa_stream_peek(p, &pcm, &bytes);
    if(pcm != nullptr && bytes != 0 && ai->self->isOpen) {
        memcpy(ai->self->buffers[ai->self->currentBuffer], pcm, bytes);
        ai->callCallback(bytes, ai->self->buffers[ai->self->currentBuffer]);
        pa_stream_drop(p);
        ai->self->currentBuffer = (ai->self->currentBuffer + 1) % NUM_OF_BUFFERS;
    }
}

static void paStreamNotify(pa_stream *p, void *userdata) {
    AudioInput* ai = (AudioInput*) userdata;
}

static void paContextNotify(pa_context *c, void *userdata) {
    AudioInput* ai = (AudioInput*) userdata;
    ai->self->contextState = pa_context_get_state(c);
}

const char* AudioInput::errorCodeToString(int err) {
    return pa_strerror(err);
}
