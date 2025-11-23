#include "Kokoro.h"
#include <iostream>
#include <vector>
#include <fstream>

void save_audio(const std::string& filename, const std::vector<float>& audio, int sample_rate) {
    // Simple WAV header writing
    std::ofstream file(filename, std::ios::binary);
    
    int channels = 1;
    int bits_per_sample = 32; // Float
    int byte_rate = sample_rate * channels * bits_per_sample / 8;
    int block_align = channels * bits_per_sample / 8;
    int data_size = audio.size() * sizeof(float);
    int chunk_size = 36 + data_size;

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunk_size), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    
    int subchunk1_size = 16;
    short audio_format = 3; // IEEE Float
    short num_channels = channels;
    int sample_rate_int = sample_rate;
    
    file.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    file.write(reinterpret_cast<const char*>(&audio_format), 2);
    file.write(reinterpret_cast<const char*>(&num_channels), 2);
    file.write(reinterpret_cast<const char*>(&sample_rate_int), 4);
    file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    file.write(reinterpret_cast<const char*>(&block_align), 2);
    short bits = bits_per_sample;
    file.write(reinterpret_cast<const char*>(&bits), 2);
    
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&data_size), 4);
    file.write(reinterpret_cast<const char*>(audio.data()), data_size);
    
    std::cout << "Saved audio to " << filename << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <voices.bin> <text> [vocab_path]" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::string voices_path = argv[2];
    std::string text = argv[3];
    std::string vocab_path = "dict/vocab.txt";
    if (argc > 4) {
        vocab_path = argv[4];
    }

    try {
        Kokoro tts(model_path, voices_path, vocab_path);
        
        auto voice = tts.get_voice_style("zf_001"); // Default voice
        auto result = tts.create(text, voice, 1.0f);
        
        save_audio("output.wav", result.first, result.second);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
