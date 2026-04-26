#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace kokoro_tts {

// Encodes mono float audio (sample range expected to be [-1.0, 1.0]) into a
// canonical 16-bit PCM WAV byte stream. Returns false if sample_rate <= 0.
bool wav_encode(const std::vector<float>& samples, int sample_rate,
                std::vector<uint8_t>& out);

} // namespace kokoro_tts
