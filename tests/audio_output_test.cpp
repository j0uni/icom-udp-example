/**
 * Plays a 440 Hz sine tone for 1 second when built with ICOM_UDP_PORTAUDIO.
 * Verifies PortAudio linkage without a radio.
 */

#include "audio_output.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
#ifndef ICOM_UDP_PORTAUDIO
    std::printf("SKIP: built without ICOM_UDP_PORTAUDIO\n");
    return 0;
#else
    constexpr int kSampleRate = 12000;
    constexpr double kFreqHz = 440.0;
    constexpr int kDurationMs = 1000;

    auto out = AudioOutput::create();
    if (!out->open(kSampleRate, 1)) {
        std::fprintf(stderr, "FAIL: AudioOutput::open\n");
        return 1;
    }

    const int totalSamples = kSampleRate * kDurationMs / 1000;
    std::vector<int16_t> chunk(240);
    for (int pos = 0; pos < totalSamples; pos += static_cast<int>(chunk.size())) {
        const int n = std::min(static_cast<int>(chunk.size()), totalSamples - pos);
        for (int i = 0; i < n; ++i) {
            double t = static_cast<double>(pos + i) / kSampleRate;
            chunk[static_cast<size_t>(i)] =
                static_cast<int16_t>(8000.0 * std::sin(2.0 * M_PI * kFreqHz * t));
        }
        out->pushPcm16(chunk.data(), static_cast<size_t>(n));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    out->close();
    std::printf("OK: PortAudio sine playback test\n");
    return 0;
#endif
}
