#include "audio/ExternalWakeWord.h"

#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include <curl/curl.h>

#include "util/Log.h"

namespace lva::audio {

namespace {

constexpr const char* kTag = "wakeword";

// ---------------------------------------------------------------------------
// Minimal SHA-256 (self-contained, mirrors OtaClient's vendored MD5 so we
// don't pull in an OpenSSL dependency just for one hash).
// ---------------------------------------------------------------------------
class Sha256 {
   public:
    Sha256() { Reset(); }

    void Update(const unsigned char* data, std::size_t len) {
        for (std::size_t i = 0; i < len; ++i) {
            buffer_[buffer_len_++] = data[i];
            if (buffer_len_ == 64) {
                Transform(buffer_);
                bit_len_ += 512;
                buffer_len_ = 0;
            }
        }
    }

    void Final(unsigned char digest[32]) {
        std::size_t i = buffer_len_;
        bit_len_ += static_cast<std::uint64_t>(buffer_len_) * 8;

        // Pad.
        buffer_[i++] = 0x80;
        if (i > 56) {
            while (i < 64) buffer_[i++] = 0x00;
            Transform(buffer_);
            i = 0;
        }
        while (i < 56) buffer_[i++] = 0x00;

        // Append length (big-endian).
        for (int j = 7; j >= 0; --j) {
            buffer_[i++] = static_cast<unsigned char>(
                (bit_len_ >> (j * 8)) & 0xFF);
        }
        Transform(buffer_);

        for (int j = 0; j < 8; ++j) {
            digest[j * 4 + 0] = static_cast<unsigned char>((state_[j] >> 24) & 0xFF);
            digest[j * 4 + 1] = static_cast<unsigned char>((state_[j] >> 16) & 0xFF);
            digest[j * 4 + 2] = static_cast<unsigned char>((state_[j] >> 8) & 0xFF);
            digest[j * 4 + 3] = static_cast<unsigned char>(state_[j] & 0xFF);
        }
    }

   private:
    void Reset() {
        state_[0] = 0x6a09e667; state_[1] = 0xbb67ae85;
        state_[2] = 0x3c6ef372; state_[3] = 0xa54ff53a;
        state_[4] = 0x510e527f; state_[5] = 0x9b05688c;
        state_[6] = 0x1f83d9ab; state_[7] = 0x5be0cd19;
        bit_len_ = 0;
        buffer_len_ = 0;
    }

    static std::uint32_t Rotr(std::uint32_t x, std::uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    void Transform(const unsigned char block[64]) {
        static const std::uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4 + 0]) << 24) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 =
                Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 =
                Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 =
                Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 =
                Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;

            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::uint32_t state_[8];
    std::uint64_t bit_len_;
    unsigned char buffer_[64];
    std::size_t buffer_len_;
};

std::string Sha256HexLower(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    Sha256 sha;
    std::array<char, 64 * 1024> buf;
    while (f) {
        f.read(buf.data(), buf.size());
        const std::streamsize n = f.gcount();
        if (n > 0) {
            sha.Update(reinterpret_cast<const unsigned char*>(buf.data()),
                       static_cast<std::size_t>(n));
        }
    }
    unsigned char digest[32];
    sha.Final(digest);

    static const char kHex[] = "0123456789abcdef";
    std::string out(64, '\0');
    for (int i = 0; i < 32; ++i) {
        out[2 * i + 0] = kHex[(digest[i] >> 4) & 0xF];
        out[2 * i + 1] = kHex[digest[i] & 0xF];
    }
    return out;
}

// Replace the last path segment of a URL with `filename`, mirroring the
// Python build's posixpath.join(dirname(path), model_name) trick. The
// query string and fragment, if any, are dropped (the Python code only
// rewrites the path component, but HA's model URLs carry neither).
std::string ReplaceUrlFilename(const std::string& url,
                               const std::string& filename) {
    // Find the start of the path (after scheme://host).
    std::size_t path_start = 0;
    const std::size_t scheme = url.find("://");
    if (scheme != std::string::npos) {
        path_start = url.find('/', scheme + 3);
        if (path_start == std::string::npos) {
            return url + "/" + filename;
        }
    }
    // Strip query/fragment before locating the last slash.
    std::size_t end = url.find_first_of("?#", path_start);
    const std::string base =
        (end == std::string::npos) ? url : url.substr(0, end);

    const std::size_t last_slash = base.find_last_of('/');
    if (last_slash == std::string::npos || last_slash < path_start) {
        return base + "/" + filename;
    }
    return base.substr(0, last_slash + 1) + filename;
}

std::size_t WriteToFileCb(char* ptr, std::size_t size, std::size_t nmemb,
                          void* userdata) {
    auto* fp = static_cast<std::FILE*>(userdata);
    const std::size_t n = size * nmemb;
    if (n == 0) return 0;
    return std::fwrite(ptr, 1, n, fp);
}

// GETs `url` into `dest`. Returns true on HTTP 200 with the body written.
bool DownloadToFile(const std::string& url,
                    const std::filesystem::path& dest) {
    std::FILE* fp = std::fopen(dest.c_str(), "wb");
    if (fp == nullptr) {
        LVA_LOGE(kTag, "cannot create %s", dest.c_str());
        return false;
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        std::fclose(fp);
        ::unlink(dest.c_str());
        LVA_LOGE(kTag, "curl_easy_init failed");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteToFileCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CAINFO,
                     "/etc/ssl/certs/ca-certificates.crt");

    const CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    std::fclose(fp);

    if (rc != CURLE_OK) {
        LVA_LOGW(kTag, "download failed: %s (%s)", url.c_str(),
                 curl_easy_strerror(rc));
        ::unlink(dest.c_str());
        return false;
    }
    if (http_code != 200) {
        LVA_LOGW(kTag, "download failed: %s, status=%ld", url.c_str(),
                 http_code);
        ::unlink(dest.c_str());
        return false;
    }
    return true;
}

}  // namespace

std::filesystem::path DownloadExternalWakeWord(
    const ExternalWakeWordInfo& info,
    const std::filesystem::path& dest_dir) {
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::create_directories(dest_dir, ec);
    if (ec) {
        LVA_LOGE(kTag, "cannot create %s: %s", dest_dir.c_str(),
                 ec.message().c_str());
        return {};
    }

    const fs::path config_path = dest_dir / (info.id + ".json");
    const fs::path model_path  = dest_dir / (info.id + ".tflite");

    const bool config_missing = !fs::exists(config_path);

    // Decide whether the cached model is still valid (size + hash match).
    bool need_model = true;
    if (fs::exists(model_path)) {
        const auto cached_size = fs::file_size(model_path, ec);
        if (!ec && cached_size == info.model_size) {
            const std::string cached_hash = Sha256HexLower(model_path);
            if (!cached_hash.empty() && cached_hash == info.model_hash) {
                need_model = false;
                LVA_LOGD(kTag,
                         "model size and hash match for %s; skipping download",
                         info.id.c_str());
            }
        }
    }

    if (config_missing || need_model) {
        LVA_LOGI(kTag, "downloading config %s -> %s", info.url.c_str(),
                 config_path.c_str());
        if (!DownloadToFile(info.url, config_path)) {
            return {};
        }
    }

    if (need_model) {
        const std::string model_url =
            ReplaceUrlFilename(info.url, model_path.filename().string());
        LVA_LOGI(kTag, "downloading model %s -> %s", model_url.c_str(),
                 model_path.c_str());
        if (!DownloadToFile(model_url, model_path)) {
            return {};
        }
    }

    return config_path;
}

}  // namespace lva::audio
