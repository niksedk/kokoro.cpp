#include "Kokoro.h"
#include "wav_memory.h"

#include "httplib.h"
#include "json.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace {

struct server_options {
    std::string model_path;
    std::string voices_path;
    std::string vocab_path = "dict/vocab.txt";
    std::string host       = "127.0.0.1";
    int         port       = 8080;
};

void print_usage(const char* program) {
    fprintf(stderr,
            "Usage: %s -m <model_path> -V <voices_path> [options]\n"
            "\n"
            "Options:\n"
            "  -m, --model <path>       ONNX model file (required)\n"
            "  -V, --voices <path>      voices.bin file (required)\n"
            "      --vocab <path>       vocab.txt file (default: dict/vocab.txt)\n"
            "      --host <addr>        Bind address (default: 127.0.0.1)\n"
            "      --port <n>           Port (default: 8080)\n"
            "  -h, --help               Show this help\n",
            program);
}

bool parse_args(int argc, char** argv, server_options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (++i >= argc) {
                fprintf(stderr, "Error: missing value for %s\n", name);
                std::exit(1);
            }
            return argv[i];
        };
        if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (a == "-m" || a == "--model") {
            opt.model_path = need("--model");
        } else if (a == "-V" || a == "--voices") {
            opt.voices_path = need("--voices");
        } else if (a == "--vocab") {
            opt.vocab_path = need("--vocab");
        } else if (a == "--host") {
            opt.host = need("--host");
        } else if (a == "--port") {
            opt.port = std::atoi(need("--port"));
        } else {
            fprintf(stderr, "Error: unknown argument: %s\n", a.c_str());
            print_usage(argv[0]);
            return false;
        }
    }
    if (opt.model_path.empty() || opt.voices_path.empty()) {
        fprintf(stderr, "Error: --model and --voices are required\n");
        print_usage(argv[0]);
        return false;
    }
    return true;
}

void send_json(httplib::Response& res, int status, const json& j) {
    res.status = status;
    res.set_content(j.dump(), "application/json");
}

void send_error(httplib::Response& res, int status, const std::string& msg) {
    send_json(res, status, json{{"error", msg}});
}

void send_wav(httplib::Response& res, const std::vector<float>& audio, int sample_rate,
              long long synth_total_ms) {
    std::vector<uint8_t> wav;
    if (!kokoro_tts::wav_encode(audio, sample_rate, wav)) {
        send_error(res, 500, "wav_encode failed");
        return;
    }
    char dur[32];
    std::snprintf(dur, sizeof(dur), "%.3f",
                  sample_rate > 0 ? (double) audio.size() / sample_rate : 0.0);
    res.set_header("X-Audio-Duration-Seconds", dur);
    res.set_header("X-Synth-Total-Ms", std::to_string(synth_total_ms));
    res.status = 200;
    res.set_content(std::string(wav.begin(), wav.end()), "audio/wav");
}

struct synth_context {
    Kokoro&     tts;
    std::mutex& mu;
    std::string default_voice;
};

void handle_synthesize(synth_context& ctx, const httplib::Request& req,
                       httplib::Response& res) {
    json body;
    try {
        body = json::parse(req.body);
    } catch (const std::exception& e) {
        send_error(res, 400, std::string("invalid JSON: ") + e.what());
        return;
    }
    if (!body.contains("text") || !body["text"].is_string()) {
        send_error(res, 400, "missing 'text' (string)");
        return;
    }
    std::string text  = body["text"].get<std::string>();
    std::string voice = ctx.default_voice;
    if (body.contains("voice") && body["voice"].is_string()) {
        voice = body["voice"].get<std::string>();
    }
    float speed = 1.0f;
    if (body.contains("speed") && body["speed"].is_number()) {
        speed = body["speed"].get<float>();
    }

    if (!voice.empty() && !ctx.tts.has_voice(voice)) {
        send_error(res, 400, "unknown voice: " + voice);
        return;
    }

    auto t0 = std::chrono::steady_clock::now();
    std::pair<std::vector<float>, int> result;
    try {
        std::lock_guard<std::mutex> lock(ctx.mu);
        result = ctx.tts.create(text, voice, speed);
    } catch (const std::exception& e) {
        send_error(res, 500, std::string("synthesis failed: ") + e.what());
        return;
    }
    auto t1 = std::chrono::steady_clock::now();
    long long total_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (result.first.empty()) {
        send_error(res, 500, "synthesis returned empty audio");
        return;
    }
    send_wav(res, result.first, result.second, total_ms);
}

void handle_capabilities(Kokoro& tts, httplib::Response& res) {
    auto names = tts.get_voice_names();
    json out = {
        {"loaded",                  true},
        {"model_type",              "kokoro"},
        {"supports_voice_clone",    false},
        {"supports_named_speakers", true},
        {"supports_instruction",    false},
        {"speaker_embedding_dim",   256},
        {"speaker_count",           (int) names.size()},
    };
    send_json(res, 200, out);
}

void handle_speakers(Kokoro& tts, httplib::Response& res) {
    send_json(res, 200, json{{"speakers", tts.get_voice_names()}});
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    server_options opt;
    if (!parse_args(argc, argv, opt)) return 1;

    fprintf(stderr, "Loading Kokoro model: %s\n", opt.model_path.c_str());
    fprintf(stderr, "Loading voices:       %s\n", opt.voices_path.c_str());
    fprintf(stderr, "Vocab:                %s\n", opt.vocab_path.c_str());

    auto t0 = std::chrono::steady_clock::now();
    std::unique_ptr<Kokoro> tts;
    try {
        tts = std::make_unique<Kokoro>(opt.model_path, opt.voices_path, opt.vocab_path);
    } catch (const std::exception& e) {
        fprintf(stderr, "Error: failed to construct Kokoro: %s\n", e.what());
        return 1;
    }
    auto t1 = std::chrono::steady_clock::now();
    fprintf(stderr, "Model loaded in %lld ms (%d voices)\n",
            (long long) std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count(),
            (int) tts->get_voice_names().size());

    auto voice_names = tts->get_voice_names();
    if (voice_names.empty()) {
        fprintf(stderr, "Error: no voices loaded from %s\n", opt.voices_path.c_str());
        return 1;
    }
    std::string default_voice = voice_names.front();
    fprintf(stderr, "Default voice: %s\n", default_voice.c_str());

    std::mutex synth_mu;
    synth_context ctx{*tts, synth_mu, default_voice};

    httplib::Server svr;
    svr.set_payload_max_length(16 * 1024 * 1024); // 16 MiB JSON cap

    svr.Get("/health", [&](const httplib::Request&, httplib::Response& res) {
        json models = json::object();
        models["model"]  = fs::path(opt.model_path).filename().string();
        models["voices"] = fs::path(opt.voices_path).filename().string();
        send_json(res, 200, json{
            {"status",       "ok"},
            {"model_loaded", true},
            {"models",       models},
        });
    });
    svr.Get("/v1/capabilities", [&](const httplib::Request&, httplib::Response& res) {
        handle_capabilities(*tts, res);
    });
    svr.Get("/v1/speakers", [&](const httplib::Request&, httplib::Response& res) {
        handle_speakers(*tts, res);
    });
    svr.Post("/v1/synthesize", [&](const httplib::Request& req, httplib::Response& res) {
        handle_synthesize(ctx, req, res);
    });

    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res,
                                 std::exception_ptr ep) {
        std::string msg = "internal error";
        try { if (ep) std::rethrow_exception(ep); }
        catch (const std::exception& e) { msg = e.what(); }
        catch (...) {}
        send_error(res, 500, msg);
    });

    fprintf(stderr, "kokoro-tts-server listening on http://%s:%d\n",
            opt.host.c_str(), opt.port);
    if (!svr.listen(opt.host, opt.port)) {
        fprintf(stderr, "Error: failed to bind %s:%d\n",
                opt.host.c_str(), opt.port);
        return 1;
    }
    return 0;
}
