#include "wav_memory.h"

namespace kokoro_tts {

namespace {

void append_bytes(std::vector<uint8_t>& out, const void* src, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(src);
    out.insert(out.end(), p, p + n);
}

} // namespace

bool wav_encode(const std::vector<float>& samples, int sample_rate,
                std::vector<uint8_t>& out) {
    if (sample_rate <= 0) {
        return false;
    }

    const uint16_t num_channels    = 1;
    const uint16_t bits_per_sample = 16;
    const uint32_t byte_rate       = (uint32_t) sample_rate * num_channels * bits_per_sample / 8;
    const uint16_t block_align     = num_channels * bits_per_sample / 8;
    const uint32_t data_size       = (uint32_t) (samples.size() * block_align);
    const uint32_t file_size       = 36 + data_size;

    out.clear();
    out.reserve(44 + data_size);

    append_bytes(out, "RIFF", 4);
    append_bytes(out, &file_size, 4);
    append_bytes(out, "WAVE", 4);

    append_bytes(out, "fmt ", 4);
    const uint32_t fmt_size     = 16;
    const uint16_t audio_format = 1; // PCM
    const uint32_t sr           = (uint32_t) sample_rate;
    append_bytes(out, &fmt_size, 4);
    append_bytes(out, &audio_format, 2);
    append_bytes(out, &num_channels, 2);
    append_bytes(out, &sr, 4);
    append_bytes(out, &byte_rate, 4);
    append_bytes(out, &block_align, 2);
    append_bytes(out, &bits_per_sample, 2);

    append_bytes(out, "data", 4);
    append_bytes(out, &data_size, 4);

    for (float s : samples) {
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t pcm = (int16_t) (s * 32767.0f);
        append_bytes(out, &pcm, 2);
    }

    return true;
}

} // namespace kokoro_tts
