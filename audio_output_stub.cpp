#include "audio_output.hpp"

class AudioOutputNull final : public AudioOutput {
public:
    bool open(int, int) override { return true; }
    void pushPcm16(const int16_t*, size_t) override {}
    void close() override {}
};

std::unique_ptr<AudioOutput> AudioOutput::create() {
    return std::make_unique<AudioOutputNull>();
}
