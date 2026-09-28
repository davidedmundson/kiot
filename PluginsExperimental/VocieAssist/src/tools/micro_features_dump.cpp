
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "audio/MicroFeatures.h"

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
        err = "not a RIFF file";
        return false;
    }
    std::uint32_t riff_size{};
    if (!ReadLE32(in, riff_size)) {
        err = "short RIFF header";
        return false;
    }
    char wave[4]{};
    in.read(wave, 4);
    if (in.gcount() != 4 || std::memcmp(wave, "WAVE", 4) != 0) {
        err = "not a WAVE file";
        return false;
    }

    bool got_fmt  = false;
    bool got_data = false;
    while (in && !got_data) {
        char chunk_id[4]{};
        in.read(chunk_id, 4);
        if (in.gcount() != 4) break;
        std::uint32_t chunk_size{};
        if (!ReadLE32(in, chunk_size)) break;

        if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
            std::uint16_t audio_format{};
            std::uint16_t channels{};
            std::uint32_t sample_rate{};
            std::uint32_t byte_rate{};
            std::uint16_t block_align{};
            std::uint16_t bits{};
            if (!ReadLE16(in, audio_format) || !ReadLE16(in, channels) ||
                !ReadLE32(in, sample_rate) || !ReadLE32(in, byte_rate) ||
                !ReadLE16(in, block_align) || !ReadLE16(in, bits)) {
                err = "short fmt chunk";
                return false;
            }
            if (audio_format != 1) {
                err = "only PCM (audio_format=1) is supported";
                return false;
            }
            if (chunk_size > 16) {
                in.ignore(chunk_size - 16);
            }
            out.channels    = channels;
            out.sample_rate = sample_rate;
            out.bits        = bits;
            got_fmt = true;
        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
            if (!got_fmt) {
                err = "data chunk before fmt";
                return false;
            }
            const std::size_t total_samples =
                chunk_size / (out.bits / 8);
            out.samples.resize(total_samples);
            in.read(reinterpret_cast<char*>(out.samples.data()),
                    chunk_size);
            if (in.gcount() != static_cast<std::streamsize>(chunk_size)) {
                err = "short data chunk";
                return false;
            }
            got_data = true;
        } else {
            // Skip unknown chunks (LIST, fact, ...).
            in.ignore(chunk_size);
        }
    }

    if (!got_fmt || !got_data) {
        err = "missing fmt or data chunk";
        return false;
    }
    if (out.channels != 1) {
        err = "expected mono, got " + std::to_string(out.channels) + " channels";
        return false;
    }
    if (out.sample_rate != 16'000) {
        err = "expected 16000 Hz, got " + std::to_string(out.sample_rate);
        return false;
    }
    if (out.bits != 16) {
        err = "expected 16-bit, got " + std::to_string(out.bits);
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr,
                     "usage: %s <path-to-16khz-mono-s16le.wav>\n",
                     argv[0]);
        return 2;
    }
    std::ifstream wav(argv[1], std::ios::binary);
    if (!wav) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    WavData data;
    std::string err;
    if (!LoadWav(wav, data, err)) {
        std::fprintf(stderr, "WAV parse failed: %s\n", err.c_str());
        return 1;
    }

    lva::audio::MicroFeatures mf;
    if (!mf.Ok()) {
        std::fprintf(stderr, "MicroFeatures init failed\n");
        return 1;
    }

    std::vector<float> features;
    features.reserve(
        (data.samples.size() / lva::audio::MicroFeatures::kSamplesPerChunk + 1)
        * lva::audio::MicroFeatures::kFeatureSize);
    mf.Process(data.samples.data(), data.samples.size(), features);

    const std::size_t kFeatureSize = lva::audio::MicroFeatures::kFeatureSize;
    const std::size_t num_frames   = features.size() / kFeatureSize;

    for (std::size_t f = 0; f < num_frames; ++f) {
        for (std::size_t i = 0; i < kFeatureSize; ++i) {
            if (i > 0) std::printf(" ");
            std::printf("%.17g", features[f * kFeatureSize + i]);
        }
        std::printf("\n");
    }
    return 0;
}
