#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

/** Optional RX audio playback (PortAudio when ICOM_UDP_PORTAUDIO is defined). */
class AudioOutput {
public:
    virtual ~AudioOutput() = default;

    virtual bool open(int sampleRate, int channelCount = 1) = 0;
    virtual void pushPcm16(const int16_t* samples, size_t count) = 0;
    virtual void close() = 0;

    static std::unique_ptr<AudioOutput> create();
};
