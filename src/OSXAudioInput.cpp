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

    double samplesPerFrame = double(options.sampleRate) * double(options.frameDuration) / 1000.0 * double(options.channels);
    AudioQueueNewInput(&self->format, input_callback, this, NULL, kCFRunLoopCommonModes, 0, &self->inQueue);
    for(int i = 0; i < NUM_OF_BUFFERS; i++) {
        AudioQueueAllocateBuffer(self->inQueue, samplesPerFrame, &self->inBuffer[i]);
        AudioQueueEnqueueBuffer(self->inQueue, self->inBuffer[i], 0, NULL);
    }

    AudioQueueStart(self->inQueue, NULL);
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
