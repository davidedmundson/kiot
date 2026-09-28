#include "util/Log.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <mutex>

namespace lva::esphome_log {

namespace {

std::atomic<int> g_level{static_cast<int>(Level::kInfo)};
std::mutex g_write_mtx;

const char* LevelTag(Level level) noexcept {
    switch (level) {
        case Level::kError: return "ERROR";
        case Level::kWarn:  return "WARN ";
        case Level::kInfo:  return "INFO ";
        case Level::kDebug: return "DEBUG";
    }
    return "?    ";
}

}  // namespace

void SetLevel(Level level) noexcept {
    g_level.store(static_cast<int>(level), std::memory_order_relaxed);
}

Level GetLevel() noexcept {
    return static_cast<Level>(g_level.load(std::memory_order_relaxed));
}

void Write(Level level, std::string_view tag, std::string_view message) noexcept {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto t   = clock::to_time_t(now);
    const auto us  = std::chrono::duration_cast<std::chrono::microseconds>(
                         now.time_since_epoch())
                         .count() %
                     1'000'000;

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char ts[32];
    std::snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<long>(us));

    std::lock_guard<std::mutex> lock(g_write_mtx);
    std::fprintf(stderr, "[%s] %s [%.*s] %.*s\n",
                 ts, LevelTag(level),
                 static_cast<int>(tag.size()), tag.data(),
                 static_cast<int>(message.size()), message.data());
    std::fflush(stderr);
}

}  // namespace lva::esphome_log
