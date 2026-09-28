
#pragma once

#include <cstdio>
#include <string>
#include <string_view>

namespace lva::esphome_log {

enum class Level : int {
    kError = 0,
    kWarn  = 1,
    kInfo  = 2,
    kDebug = 3,
};

void SetLevel(Level level) noexcept;
Level GetLevel() noexcept;

void Write(Level level, std::string_view tag, std::string_view message) noexcept;

namespace detail {

template <typename... Args>
std::string Format(const char* fmt, Args... args) {
    char buf[1024];
    const int n = std::snprintf(buf, sizeof(buf), fmt, args...);
    if (n <= 0) return {};
    const std::size_t len = (static_cast<std::size_t>(n) >= sizeof(buf))
                                ? sizeof(buf) - 1
                                : static_cast<std::size_t>(n);
    return std::string(buf, len);
}

}  // namespace detail

}  // namespace lva::esphome_log

#define LVA_LOG(level, tag, ...)                                           \
    do {                                                                   \
        if (static_cast<int>(::lva::esphome_log::GetLevel()) >=                    \
            static_cast<int>(level)) {                                     \
            ::lva::esphome_log::Write((level), (tag),                              \
                              ::lva::esphome_log::detail::Format(__VA_ARGS__));    \
        }                                                                  \
    } while (0)

#define LVA_LOGE(tag, ...) LVA_LOG(::lva::esphome_log::Level::kError, tag, __VA_ARGS__)
#define LVA_LOGW(tag, ...) LVA_LOG(::lva::esphome_log::Level::kWarn,  tag, __VA_ARGS__)
#define LVA_LOGI(tag, ...) LVA_LOG(::lva::esphome_log::Level::kInfo,  tag, __VA_ARGS__)
#define LVA_LOGD(tag, ...) LVA_LOG(::lva::esphome_log::Level::kDebug, tag, __VA_ARGS__)
