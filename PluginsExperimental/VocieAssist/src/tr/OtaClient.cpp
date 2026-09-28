#include "tr/OtaClient.h"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "util/Log.h"

extern char** environ;

namespace lva::tr {

namespace {

constexpr const char* kTag = "ota";
constexpr off_t kCacheDropWindow = 1 << 20;  // 1 MiB

void DropFileCache(int fd, off_t offset, off_t length) {
    if (length <= 0) return;
#ifdef POSIX_FADV_DONTNEED
    posix_fadvise(fd, offset, length, POSIX_FADV_DONTNEED);
#else
    (void)fd; (void)offset; (void)length;
#endif
}

std::string Md5HexLower(const unsigned char digest[16]) {
    static const char kHex[] = "0123456789abcdef";
    std::string out(32, '\0');
    for (int i = 0; i < 16; ++i) {
        out[2 * i + 0] = kHex[(digest[i] >> 4) & 0xF];
        out[2 * i + 1] = kHex[digest[i] & 0xF];
    }
    return out;
}

}  // namespace

}  // namespace lva::tr

namespace lva::tr {
namespace {

class Md5 {
   public:
    Md5() {
        state_[0] = 0x67452301;
        state_[1] = 0xEFCDAB89;
        state_[2] = 0x98BADCFE;
        state_[3] = 0x10325476;
        count_   = 0;
    }
    void Update(const unsigned char* data, std::size_t n) {
        std::size_t idx = static_cast<std::size_t>((count_ >> 3) & 0x3F);
        count_ += static_cast<std::uint64_t>(n) << 3;
        std::size_t part = 64 - idx;
        std::size_t i = 0;
        if (n >= part) {
            std::memcpy(buffer_ + idx, data, part);
            Transform(buffer_);
            for (i = part; i + 63 < n; i += 64) Transform(data + i);
            idx = 0;
        }
        std::memcpy(buffer_ + idx, data + i, n - i);
    }
    void Final(unsigned char digest[16]) {
        unsigned char bits[8];
        for (int i = 0; i < 8; ++i) {
            bits[i] = static_cast<unsigned char>((count_ >> (i * 8)) & 0xFF);
        }
        std::size_t idx = static_cast<std::size_t>((count_ >> 3) & 0x3F);
        std::size_t pad_len = (idx < 56) ? (56 - idx) : (120 - idx);
        static const unsigned char kPad[64] = {0x80};
        Update(kPad, pad_len);
        Update(bits, 8);
        for (int i = 0; i < 4; ++i) {
            digest[i * 4 + 0] = static_cast<unsigned char>(state_[i] & 0xFF);
            digest[i * 4 + 1] = static_cast<unsigned char>((state_[i] >> 8) & 0xFF);
            digest[i * 4 + 2] = static_cast<unsigned char>((state_[i] >> 16) & 0xFF);
            digest[i * 4 + 3] = static_cast<unsigned char>((state_[i] >> 24) & 0xFF);
        }
    }

   private:
    static std::uint32_t F(std::uint32_t x, std::uint32_t y, std::uint32_t z)
        { return (x & y) | (~x & z); }
    static std::uint32_t G(std::uint32_t x, std::uint32_t y, std::uint32_t z)
        { return (x & z) | (y & ~z); }
    static std::uint32_t H(std::uint32_t x, std::uint32_t y, std::uint32_t z)
        { return x ^ y ^ z; }
    static std::uint32_t I(std::uint32_t x, std::uint32_t y, std::uint32_t z)
        { return y ^ (x | ~z); }
    static std::uint32_t Rot(std::uint32_t x, int n)
        { return (x << n) | (x >> (32 - n)); }
    static void Step(std::uint32_t (*f)(std::uint32_t,std::uint32_t,std::uint32_t),
                     std::uint32_t& a, std::uint32_t b, std::uint32_t c,
                     std::uint32_t d, std::uint32_t x, int s, std::uint32_t t) {
        a = b + Rot(a + f(b, c, d) + x + t, s);
    }

    void Transform(const unsigned char block[64]) {
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t x[16];
        for (int i = 0; i < 16; ++i) {
            x[i] = static_cast<std::uint32_t>(block[i * 4 + 0])
                 | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 8)
                 | (static_cast<std::uint32_t>(block[i * 4 + 2]) << 16)
                 | (static_cast<std::uint32_t>(block[i * 4 + 3]) << 24);
        }
        // Round 1
        Step(F, a, b, c, d, x[0],  7,  0xD76AA478);
        Step(F, d, a, b, c, x[1],  12, 0xE8C7B756);
        Step(F, c, d, a, b, x[2],  17, 0x242070DB);
        Step(F, b, c, d, a, x[3],  22, 0xC1BDCEEE);
        Step(F, a, b, c, d, x[4],  7,  0xF57C0FAF);
        Step(F, d, a, b, c, x[5],  12, 0x4787C62A);
        Step(F, c, d, a, b, x[6],  17, 0xA8304613);
        Step(F, b, c, d, a, x[7],  22, 0xFD469501);
        Step(F, a, b, c, d, x[8],  7,  0x698098D8);
        Step(F, d, a, b, c, x[9],  12, 0x8B44F7AF);
        Step(F, c, d, a, b, x[10], 17, 0xFFFF5BB1);
        Step(F, b, c, d, a, x[11], 22, 0x895CD7BE);
        Step(F, a, b, c, d, x[12], 7,  0x6B901122);
        Step(F, d, a, b, c, x[13], 12, 0xFD987193);
        Step(F, c, d, a, b, x[14], 17, 0xA679438E);
        Step(F, b, c, d, a, x[15], 22, 0x49B40821);
        // Round 2
        Step(G, a, b, c, d, x[1],  5,  0xF61E2562);
        Step(G, d, a, b, c, x[6],  9,  0xC040B340);
        Step(G, c, d, a, b, x[11], 14, 0x265E5A51);
        Step(G, b, c, d, a, x[0],  20, 0xE9B6C7AA);
        Step(G, a, b, c, d, x[5],  5,  0xD62F105D);
        Step(G, d, a, b, c, x[10], 9,  0x02441453);
        Step(G, c, d, a, b, x[15], 14, 0xD8A1E681);
        Step(G, b, c, d, a, x[4],  20, 0xE7D3FBC8);
        Step(G, a, b, c, d, x[9],  5,  0x21E1CDE6);
        Step(G, d, a, b, c, x[14], 9,  0xC33707D6);
        Step(G, c, d, a, b, x[3],  14, 0xF4D50D87);
        Step(G, b, c, d, a, x[8],  20, 0x455A14ED);
        Step(G, a, b, c, d, x[13], 5,  0xA9E3E905);
        Step(G, d, a, b, c, x[2],  9,  0xFCEFA3F8);
        Step(G, c, d, a, b, x[7],  14, 0x676F02D9);
        Step(G, b, c, d, a, x[12], 20, 0x8D2A4C8A);
        // Round 3
        Step(H, a, b, c, d, x[5],  4,  0xFFFA3942);
        Step(H, d, a, b, c, x[8],  11, 0x8771F681);
        Step(H, c, d, a, b, x[11], 16, 0x6D9D6122);
        Step(H, b, c, d, a, x[14], 23, 0xFDE5380C);
        Step(H, a, b, c, d, x[1],  4,  0xA4BEEA44);
        Step(H, d, a, b, c, x[4],  11, 0x4BDECFA9);
        Step(H, c, d, a, b, x[7],  16, 0xF6BB4B60);
        Step(H, b, c, d, a, x[10], 23, 0xBEBFBC70);
        Step(H, a, b, c, d, x[13], 4,  0x289B7EC6);
        Step(H, d, a, b, c, x[0],  11, 0xEAA127FA);
        Step(H, c, d, a, b, x[3],  16, 0xD4EF3085);
        Step(H, b, c, d, a, x[6],  23, 0x04881D05);
        Step(H, a, b, c, d, x[9],  4,  0xD9D4D039);
        Step(H, d, a, b, c, x[12], 11, 0xE6DB99E5);
        Step(H, c, d, a, b, x[15], 16, 0x1FA27CF8);
        Step(H, b, c, d, a, x[2],  23, 0xC4AC5665);
        // Round 4
        Step(I, a, b, c, d, x[0],  6,  0xF4292244);
        Step(I, d, a, b, c, x[7],  10, 0x432AFF97);
        Step(I, c, d, a, b, x[14], 15, 0xAB9423A7);
        Step(I, b, c, d, a, x[5],  21, 0xFC93A039);
        Step(I, a, b, c, d, x[12], 6,  0x655B59C3);
        Step(I, d, a, b, c, x[3],  10, 0x8F0CCC92);
        Step(I, c, d, a, b, x[10], 15, 0xFFEFF47D);
        Step(I, b, c, d, a, x[1],  21, 0x85845DD1);
        Step(I, a, b, c, d, x[8],  6,  0x6FA87E4F);
        Step(I, d, a, b, c, x[15], 10, 0xFE2CE6E0);
        Step(I, c, d, a, b, x[6],  15, 0xA3014314);
        Step(I, b, c, d, a, x[13], 21, 0x4E0811A1);
        Step(I, a, b, c, d, x[4],  6,  0xF7537E82);
        Step(I, d, a, b, c, x[11], 10, 0xBD3AF235);
        Step(I, c, d, a, b, x[2],  15, 0x2AD7D2BB);
        Step(I, b, c, d, a, x[9],  21, 0xEB86D391);
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    }

    std::uint32_t state_[4];
    std::uint64_t count_;
    unsigned char buffer_[64];
};

}  // namespace
}  // namespace lva::tr

namespace lva::tr {

std::string OtaClient::CalculateFileMd5(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error("md5: cannot open " + path);
    }
    Md5 md5;
    std::vector<unsigned char> buf(64 * 1024);
    int fd = -1;
    {
        fd = ::open(path.c_str(), O_RDONLY);
    }
    off_t processed = 0, dropped = 0;
    while (f) {
        f.read(reinterpret_cast<char*>(buf.data()),
               static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = f.gcount();
        if (got <= 0) break;
        md5.Update(buf.data(), static_cast<std::size_t>(got));
        processed += got;
        if (fd >= 0 && processed - dropped >= kCacheDropWindow) {
            DropFileCache(fd, dropped, processed - dropped);
            dropped = processed;
        }
    }
    if (fd >= 0) ::close(fd);
    unsigned char digest[16];
    md5.Final(digest);
    return Md5HexLower(digest);
}

namespace {

struct DownloadCtx {
    std::FILE*                              fp = nullptr;
    Md5                                     md5;
    OtaClient::ProgressCallback             progress;
    bool                                    has_progress = false;
    curl_off_t                              total_bytes = 0;
    curl_off_t                              written = 0;
    int                                     fd = -1;
    off_t                                   dropped = 0;
    const std::atomic<bool>*                cancel = nullptr;
};

std::size_t WriteCb(char* ptr, std::size_t size, std::size_t nmemb,
                    void* userdata) {
    auto* ctx = static_cast<DownloadCtx*>(userdata);
    if (ctx->cancel != nullptr && ctx->cancel->load()) {
        return 0;  // tell curl to abort
    }
    const std::size_t n = size * nmemb;
    if (n == 0) return 0;
    if (std::fwrite(ptr, 1, n, ctx->fp) != n) return 0;
    ctx->md5.Update(reinterpret_cast<const unsigned char*>(ptr), n);
    ctx->written += static_cast<curl_off_t>(n);
    if (ctx->fd >= 0 &&
        ctx->written - ctx->dropped >= kCacheDropWindow) {
        std::fflush(ctx->fp);
        ::fsync(ctx->fd);
        DropFileCache(ctx->fd, ctx->dropped, ctx->written - ctx->dropped);
        ctx->dropped = ctx->written;
    }
    return n;
}

int ProgressCb(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
               curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* ctx = static_cast<DownloadCtx*>(clientp);
    if (ctx->cancel != nullptr && ctx->cancel->load()) {
        return 1;  // abort
    }
    if (dltotal > 0) {
        ctx->total_bytes  = dltotal;
        ctx->has_progress = true;
    }
    if (ctx->progress) {
        const float pct = ctx->total_bytes > 0
            ? std::min(100.0f,
                       static_cast<float>(dlnow) /
                           static_cast<float>(ctx->total_bytes) * 100.0f)
            : 0.0f;
        ctx->progress(pct, ctx->has_progress);
    }
    return 0;
}

}  // namespace

void OtaClient::DownloadFirmware(const OtaRelease& release,
                                 ProgressCallback progress,
                                 const std::atomic<bool>* cancel) {
    if (release.url.empty()) {
        throw std::runtime_error("OTA: empty download URL");
    }
    if (release.expected_md5.empty()) {
        throw std::runtime_error("OTA: empty expected MD5");
    }

    const std::string final_path = download_path_;
    const std::string temp_path  = final_path + ".part";

    // If an existing complete .swu matches MD5, reuse it.
    struct stat st{};
    if (::stat(final_path.c_str(), &st) == 0) {
        try {
            const std::string md5 = CalculateFileMd5(final_path);
            std::string want = release.expected_md5;
            std::transform(want.begin(), want.end(), want.begin(),
                           [](unsigned char c) {
                               return static_cast<char>(std::tolower(c));
                           });
            if (md5 == want) {
                LVA_LOGI(kTag,
                         "reusing already-downloaded matching firmware: %s",
                         final_path.c_str());
                if (progress) progress(100.0f, true);
                return;
            }
            LVA_LOGI(kTag, "removing stale firmware (md5 mismatch)");
            ::unlink(final_path.c_str());
        } catch (...) {
            ::unlink(final_path.c_str());
        }
    }
    ::unlink(temp_path.c_str());

    // Make sure the parent dir exists.
    {
        const auto slash = final_path.find_last_of('/');
        if (slash != std::string::npos) {
            const std::string dir = final_path.substr(0, slash);
            ::mkdir(dir.c_str(), 0755);  // best-effort
        }
    }

    DownloadCtx ctx;
    ctx.fp = std::fopen(temp_path.c_str(), "wb");
    if (ctx.fp == nullptr) {
        throw std::runtime_error("OTA: cannot create " + temp_path +
                                 ": " + std::strerror(errno));
    }
    ctx.fd       = ::fileno(ctx.fp);
    ctx.progress = std::move(progress);
    ctx.cancel   = cancel;

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        std::fclose(ctx.fp);
        ::unlink(temp_path.c_str());
        throw std::runtime_error("OTA: curl_easy_init failed");
    }

    curl_easy_setopt(curl, CURLOPT_URL,            release.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  &WriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,     0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &ProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,   &ctx);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);   // 1 KB/s
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,  60L);     // for 60 s
    curl_easy_setopt(curl, CURLOPT_CAINFO,         kCaCertPath);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE,     32L * 1024L);

    LVA_LOGI(kTag, "downloading %s -> %s", release.url.c_str(),
             temp_path.c_str());
    if (ctx.progress) ctx.progress(0.0f, false);

    const CURLcode rc = curl_easy_perform(curl);
    long http_status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
    curl_easy_cleanup(curl);

    std::fflush(ctx.fp);
    ::fsync(ctx.fd);
    if (ctx.written > ctx.dropped) {
        DropFileCache(ctx.fd, ctx.dropped, ctx.written - ctx.dropped);
    }
    std::fclose(ctx.fp);

    if (rc != CURLE_OK) {
        ::unlink(temp_path.c_str());
        throw std::runtime_error(std::string("OTA: curl failed: ") +
                                 curl_easy_strerror(rc));
    }
    if (http_status != 200) {
        ::unlink(temp_path.c_str());
        throw std::runtime_error("OTA: HTTP status " +
                                 std::to_string(http_status));
    }

    // MD5 verify
    unsigned char digest[16];
    ctx.md5.Final(digest);
    const std::string actual = Md5HexLower(digest);
    std::string want = release.expected_md5;
    std::transform(want.begin(), want.end(), want.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    if (actual != want) {
        ::unlink(temp_path.c_str());
        throw std::runtime_error("OTA: MD5 mismatch (expected=" + want +
                                 ", got=" + actual + ")");
    }

    if (::rename(temp_path.c_str(), final_path.c_str()) != 0) {
        ::unlink(temp_path.c_str());
        throw std::runtime_error("OTA: rename to final path failed: " +
                                 std::string(std::strerror(errno)));
    }
    LVA_LOGI(kTag, "download OK (%lld bytes), MD5 verified",
             static_cast<long long>(ctx.written));
    if (ctx.progress) ctx.progress(100.0f, true);
}

void OtaClient::InstallFirmware() {
    struct stat st{};
    if (::stat(download_path_.c_str(), &st) != 0) {
        throw std::runtime_error("OTA: firmware file missing: " +
                                 download_path_);
    }

    char prog[]  = "/usr/bin/swupdate";
    char a1[]    = "-G";
    char a2[]    = "-k";
    char pubkey[] = "/etc/swupdate-public.pem";
    char a3[]    = "-H";
    char hwver[] = "S420:1.0";
    char* argv[] = { prog, a1, a2, pubkey, a3, hwver, nullptr };

    LVA_LOGI(kTag, "starting swupdate");

    // Block SIGCHLD so the global auto-reap handler cannot steal our
    // child before we waitpid. Restored after waitpid returns.
    sigset_t block_chld, old_mask;
    sigemptyset(&block_chld);
    sigaddset(&block_chld, SIGCHLD);
    ::sigprocmask(SIG_BLOCK, &block_chld, &old_mask);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO,
                                     "/dev/null", O_RDONLY, 0);

    pid_t pid = 0;
    const int rc = ::posix_spawn(&pid, prog, &actions, nullptr,
                                 argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0) {
        ::sigprocmask(SIG_SETMASK, &old_mask, nullptr);
        throw std::runtime_error("OTA: posix_spawn(swupdate) failed: " +
                                 std::string(std::strerror(rc)));
    }

    int status = 0;
    while (true) {
        const pid_t r = ::waitpid(pid, &status, 0);
        if (r == pid) break;
        if (r < 0 && errno != EINTR) {
            ::sigprocmask(SIG_SETMASK, &old_mask, nullptr);
            throw std::runtime_error("OTA: waitpid failed: " +
                                     std::string(std::strerror(errno)));
        }
    }
    ::sigprocmask(SIG_SETMASK, &old_mask, nullptr);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error("OTA: swupdate exited with status " +
                                 std::to_string(WEXITSTATUS(status)));
    }
    LVA_LOGI(kTag, "swupdate completed; expecting reboot");
}

}  // namespace lva::tr
