#include "audio_output.hpp"

#include <portaudio.h>

#include <cstring>
#include <iostream>
#include <mutex>
#include <vector>

namespace {

class PcmRingBuffer {
public:
    void reset(size_t capacitySamples) {
        std::lock_guard<std::mutex> lock(mu_);
        cap_ = capacitySamples;
        buf_.assign(cap_, 0);
        readPos_ = writePos_ = count_ = 0;
    }

    void write(const int16_t* samples, size_t n) {
        std::lock_guard<std::mutex> lock(mu_);
        if (cap_ == 0) return;
        for (size_t i = 0; i < n; ++i) {
            if (count_ >= cap_) {
                readPos_ = (readPos_ + 1) % cap_;
                count_--;
            }
            buf_[writePos_] = samples[i];
            writePos_ = (writePos_ + 1) % cap_;
            count_++;
        }
    }

    void read(int16_t* out, size_t n) {
        std::lock_guard<std::mutex> lock(mu_);
        size_t i = 0;
        for (; i < n && count_ > 0; ++i) {
            out[i] = buf_[readPos_];
            readPos_ = (readPos_ + 1) % cap_;
            count_--;
        }
        if (i < n) std::memset(out + i, 0, (n - i) * sizeof(int16_t));
    }

private:
    std::mutex mu_;
    std::vector<int16_t> buf_;
    size_t cap_{0};
    size_t readPos_{0};
    size_t writePos_{0};
    size_t count_{0};
};

class AudioOutputPortAudio final : public AudioOutput {
public:
    ~AudioOutputPortAudio() override { close(); }

    bool open(int sampleRate, int channelCount) override {
        if (channelCount != 1) {
            std::cerr << "[audio] PortAudio: only mono supported\n";
            return false;
        }
        sampleRate_ = sampleRate;

        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "[audio] Pa_Initialize: " << Pa_GetErrorText(err) << "\n";
            return false;
        }
        paInitialized_ = true;

        ring_.reset(static_cast<size_t>(sampleRate) * 2);

        err = Pa_OpenDefaultStream(&stream_, 0, 1, paInt16, sampleRate, 480, callback, this);
        if (err != paNoError) {
            std::cerr << "[audio] Pa_OpenDefaultStream: " << Pa_GetErrorText(err) << "\n";
            close();
            return false;
        }

        err = Pa_StartStream(stream_);
        if (err != paNoError) {
            std::cerr << "[audio] Pa_StartStream: " << Pa_GetErrorText(err) << "\n";
            close();
            return false;
        }
        return true;
    }

    void pushPcm16(const int16_t* samples, size_t count) override {
        ring_.write(samples, count);
    }

    void close() override {
        if (stream_) {
            Pa_StopStream(stream_);
            Pa_CloseStream(stream_);
            stream_ = nullptr;
        }
        if (paInitialized_) {
            Pa_Terminate();
            paInitialized_ = false;
        }
    }

private:
    static int callback(const void*, void* output, unsigned long frameCount,
                        const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData) {
        auto* self = static_cast<AudioOutputPortAudio*>(userData);
        self->ring_.read(static_cast<int16_t*>(output), frameCount);
        return paContinue;
    }

    PcmRingBuffer ring_;
    PaStream* stream_{nullptr};
    int sampleRate_{0};
    bool paInitialized_{false};
};

}  // namespace

std::unique_ptr<AudioOutput> AudioOutput::create() {
    return std::make_unique<AudioOutputPortAudio>();
}
