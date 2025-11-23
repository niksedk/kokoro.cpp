# Kokoro C++ Inference

A lightweight, high-performance C++ inference implementation for the [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) TTS model, based on ONNX Runtime. This project currently supports **Chinese and English** mixed synthesis.

## Features

- 🚀 **Fast Inference**: Powered by ONNX Runtime.
- 🌏 **Bilingual Support**: Native support for Chinese and English.


## Prerequisites

- **CMake** (3.14+)
- **C++ Compiler** (C++17 support required)
- **Python 3** (for data preparation scripts)

## Data Preparation

Before compiling, you need to prepare the model and voice files.

### 1. Download  Voices

The project uses a compact binary format for voice styles. You need to download the voice data.

Download `voices-v1.1-zh.bin` from [here](https://github.com/koth/kokoro.cpp/releases/download/voices_model_files/voices-v1.1-zh.bin) 

### 2. Download Model

Download the ONNX model file.

Download `kokoro-v1.1-zh.onnx` from [here](https://github.com/koth/kokoro.cpp/releases/download/voices_model_files/kokoro-v1.1-zh.onnx) 

## Build

```bash
mkdir build
cmake -B build .
cmake --build build
```

## Usage

Run the `kokoro_demo` executable with the model, voices file, and input text.

```bash
./kokoro_demo <path/to/model.onnx> <path/to/voices.bin> <"Text to speak">
```

### Example

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "你好啊，这是一个测试。Hello world"
```

The output audio will be saved as `output.wav` in the current directory.

## Project Structure

- `Kokoro.cpp/h`: Main TTS class.
- `ZHFrontend.cpp/h`: Chinese frontend (G2P, Tone Sandhi).
- `scripts/`: Helper scripts for data processing.
- `dict/`: Dictionary files for G2P (Jieba, Pinyin).

## License

MIT
