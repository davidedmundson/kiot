
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

#include "audio/MicroFeatures.h"
#include "audio/MicroWakeWord.h"

namespace {

struct WavData {
    std::vector<std::int16_t> samples;
    unsigned int channels    = 0;
    unsigned int sample_rate = 0;
    unsigned int bits        = 0;
};

bool ReadLE16(std::istream& in, std::uint16_t& v) {
    std::uint8_t b[2];
    in.read(reinterpret_cast<char*>(b), 2);
    if (in.gcount() != 2) return false;
    v = static_cast<std::uint16_t>(b[0] | (b[1] << 8));
    return true;
}

bool ReadLE32(std::istream& in, std::uint32_t& v) {
    std::uint8_t b[4];
    in.read(reinterpret_cast<char*>(b), 4);
    if (in.gcount() != 4) return false;
    v = static_cast<std::uint32_t>(b[0] | (b[1] << 8)
                                 | (b[2] << 16) | (b[3] << 24));
    return true;
}

bool LoadWav(std::istream& in, WavData& out, std::string& err) {
    char riff[4]{};
    in.read(riff, 4);
    if (in.gcount() != 4 || std::memcmp(riff, "RIFF", 4) != 0) {
        err = "not a RIFF file"; return false;
    }
    std::uint32_t riff_size{};
    if (!ReadLE32(in, riff_size)) { err = "short RIFF"; return false; }
    char wave[4]{};
    in.read(wave, 4);
    if (in.gcount() != 4 || std::memcmp(wave, "WAVE", 4) != 0) {
        err = "not WAVE"; return false;
    }
    bool got_fmt = false, got_data = false;
    while (in && !got_data) {
        char id[4]{}; std::uint32_t sz{};
        in.read(id, 4); if (in.gcount() != 4) break;
        if (!ReadLE32(in, sz)) break;
        if (std::memcmp(id, "fmt ", 4) == 0) {
            std::uint16_t fmt, ch, ba, bits; std::uint32_t sr, br;
            if (!ReadLE16(in, fmt) || !ReadLE16(in, ch) ||
                !ReadLE32(in, sr) || !ReadLE32(in, br) ||
                !ReadLE16(in, ba) || !ReadLE16(in, bits)) {
                err = "short fmt"; return false;
            }
            if (fmt != 1) { err = "non-PCM"; return false; }
            if (sz > 16) in.ignore(sz - 16);
            out.channels = ch; out.sample_rate = sr; out.bits = bits;
            got_fmt = true;
        } else if (std::memcmp(id, "data", 4) == 0) {
            if (!got_fmt) { err = "data before fmt"; return false; }
            out.samples.resize(sz / (out.bits / 8));
            in.read(reinterpret_cast<char*>(out.samples.data()), sz);
            if (in.gcount() != static_cast<std::streamsize>(sz)) {
                err = "short data"; return false;
            }
            got_data = true;
        } else {
            in.ignore(sz);
        }
    }
    if (!got_fmt || !got_data) { err = "missing chunks"; return false; }
    if (out.channels != 1 || out.sample_rate != 16000 || out.bits != 16) {
        err = "expected 16 kHz / mono / 16-bit"; return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr,
                     "usage: %s <model.json> <input.wav>\n", argv[0]);
        return 2;
    }

    auto ww = lva::audio::MicroWakeWord::FromConfig(argv[1]);
    if (!ww || !ww->Ok()) {
        std::fprintf(stderr, "FromConfig failed for %s\n", argv[1]);
        return 1;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[2]); return 1; }
    WavData wav;
    std::string err;
    if (!LoadWav(f, wav, err)) {
        std::fprintf(stderr, "WAV: %s\n", err.c_str()); return 1;
    }

    // Extract all features up front ( verified bit-exact).
    lva::audio::MicroFeatures mf;
    if (!mf.Ok()) {
        std::fprintf(stderr, "MicroFeatures init failed\n"); return 1;
    }
    std::vector<float> features;
    mf.Process(wav.samples.data(), wav.samples.size(), features);
    const std::size_t total_frames =
        features.size() / lva::audio::MicroFeatures::kFeatureSize;

    const auto& cfg = ww->config();

    const std::size_t feature_size = lva::audio::MicroFeatures::kFeatureSize;

    int stride = 3;
    {
        std::ifstream cj(argv[1]);
        std::string s((std::istreambuf_iterator<char>(cj)),
                       std::istreambuf_iterator<char>());
        auto pos = s.find("\"stride\"");
        if (pos != std::string::npos) {
            auto colon = s.find(':', pos);
            if (colon != std::string::npos) {
                int parsed = std::atoi(s.c_str() + colon + 1);
                if (parsed > 0) stride = parsed;
            }
        }
    }

    int invoke_idx = 0;
    std::deque<float> window;

    for (std::size_t f = 0; f < total_frames; ++f) {
        float lp = 0.0f;
        ww->Process(features.data() + f * feature_size, 1, &lp);
        if (((f + 1) % stride) == 0) {
            window.push_back(lp);
            if (static_cast<int>(window.size()) > cfg.sliding_window_size) {
                window.pop_front();
            }
            float mean = -1.0f;
            if (static_cast<int>(window.size()) == cfg.sliding_window_size) {
                mean = std::accumulate(window.begin(), window.end(), 0.0f)
                       / static_cast<float>(window.size());
            }
            std::printf("%d %.6f %.6f\n", invoke_idx, lp, mean);
            ++invoke_idx;
        }
    }
    return 0;
}
