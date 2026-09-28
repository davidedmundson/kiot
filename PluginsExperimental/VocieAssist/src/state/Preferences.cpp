#include "state/Preferences.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "util/Log.h"

namespace lva::state {

namespace {

constexpr const char* kTag = "prefs";
constexpr const char* kSoundFileName = "sound.json";

double Clamp01(double v) noexcept {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

int CoerceInt(const nlohmann::json& v, int fallback) noexcept {
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_number_float())   return static_cast<int>(v.get<double>());
    if (v.is_boolean())        return v.get<bool>() ? 1 : 0;
    if (v.is_string()) {
        try {
            return std::stoi(v.get<std::string>());
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

bool DetectSoundFormat(const nlohmann::json& obj,
                       const std::filesystem::path& path) noexcept {
    if (obj.contains("mic_gain") || obj.contains("mic_mute")) return true;
    if (path.filename() == kSoundFileName) return true;
    return false;
}

void PopulateFromJson(Preferences& out,
                      const nlohmann::json& obj,
                      const std::filesystem::path& storage_path) {
    if (!obj.is_object()) return;

    out = Preferences::ForPath(storage_path);

    const bool sound_format = DetectSoundFormat(obj, storage_path);
    if (sound_format && out.format() != Preferences::Format::kSound) {
        out = Preferences::ForPath(
            storage_path.parent_path() / kSoundFileName);
    }

    if (auto it = obj.find("active_wake_words"); it != obj.end() && it->is_array()) {
        for (const auto& item : *it) {
            if (item.is_null()) {
                out.active_wake_words.emplace_back(std::nullopt);
            } else if (item.is_string()) {
                out.active_wake_words.emplace_back(item.get<std::string>());
            } else {
                out.active_wake_words.emplace_back(std::nullopt);
            }
        }
    }

    if (auto it = obj.find("volume"); it != obj.end() && !it->is_null()
                                       && it->is_number()) {
        const double raw = it->get<double>();
        out.volume = (out.format() == Preferences::Format::kSound)
                         ? Clamp01(raw / 100.0)
                         : Clamp01(raw);
    }

    out.thinking_sound        = CoerceInt(obj.value("thinking_sound", 0), 0) ? 1 : 0;
    out.mic_gain              = CoerceInt(obj.value("mic_gain", 30), 30);
    out.mic_mute              = CoerceInt(obj.value("mic_mute", 1), 1) == 0 ? 0 : 1;
    out.mic_auto_gain         = CoerceInt(obj.value("mic_auto_gain", 0), 0);
    out.mic_noise_suppression = CoerceInt(obj.value("mic_noise_suppression", 0), 0);
    out.mic_volume = std::clamp(
        CoerceInt(obj.value("mic_volume", 1600), 1600), 1, 4000);

    auto load_optional_double = [&](const char* key) -> std::optional<double> {
        if (auto it = obj.find(key);
            it != obj.end() && !it->is_null() && it->is_number()) {
            return it->get<double>();
        }
        return std::nullopt;
    };
    out.wake_word_1_sensitivity = load_optional_double("wake_word_1_sensitivity");
    out.wake_word_2_sensitivity = load_optional_double("wake_word_2_sensitivity");
    out.stop_word_sensitivity   = load_optional_double("stop_word_sensitivity");
    out.continue_conversation_delay =
        load_optional_double("continue_conversation_delay");
}

void ApplyToJson(const Preferences& prefs, nlohmann::json& obj) {
    if (!obj.is_object()) {
        obj = nlohmann::json::object();
    }

    {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& slot : prefs.active_wake_words) {
            if (slot.has_value()) arr.push_back(*slot);
            else                  arr.push_back(nullptr);
        }
        obj["active_wake_words"] = std::move(arr);
    }

    obj["thinking_sound"] = prefs.thinking_sound ? 1 : 0;

    if (prefs.format() == Preferences::Format::kSound) {
        const double v = prefs.volume.has_value() ? *prefs.volume : 1.0;
        obj["volume"] = static_cast<int>(std::lround(Clamp01(v) * 100.0));
        obj["mic_gain"] = prefs.mic_gain;
        obj["mic_mute"] = prefs.is_mic_muted() ? 0 : 1;
    } else {
        if (prefs.volume.has_value()) {
            obj["volume"] = Clamp01(*prefs.volume);
        } else {
            obj["volume"] = nullptr;
        }
    }

    obj["mic_auto_gain"]         = prefs.mic_auto_gain;
    obj["mic_noise_suppression"] = prefs.mic_noise_suppression;
    obj["mic_volume"]            = prefs.mic_volume;

    auto store_optional_double = [&](const char* key,
                                     const std::optional<double>& v) {
        if (v.has_value()) {
            obj[key] = *v;
        } else {
            obj.erase(key);
        }
    };
    store_optional_double("wake_word_1_sensitivity", prefs.wake_word_1_sensitivity);
    store_optional_double("wake_word_2_sensitivity", prefs.wake_word_2_sensitivity);
    store_optional_double("stop_word_sensitivity",   prefs.stop_word_sensitivity);
    store_optional_double("continue_conversation_delay",
                          prefs.continue_conversation_delay);
}

}  // namespace


Preferences Preferences::ForPath(const std::filesystem::path& storage_path) {
    Preferences p;
    p.format_ = (storage_path.filename() == kSoundFileName)
                    ? Format::kSound
                    : Format::kPreferences;
    return p;
}

Preferences Preferences::LoadFromFile(const std::filesystem::path& storage_path) {
    if (!std::filesystem::exists(storage_path)) {
        LVA_LOGI(kTag, "no preferences file at %s — using defaults",
                 storage_path.c_str());
        return ForPath(storage_path);
    }

    std::ifstream file(storage_path);
    if (!file) {
        LVA_LOGW(kTag, "failed to open %s for read: %s",
                 storage_path.c_str(), std::strerror(errno));
        return ForPath(storage_path);
    }

    std::ostringstream buf;
    buf << file.rdbuf();
    std::string text = buf.str();

    nlohmann::json obj;
    try {
        obj = nlohmann::json::parse(text);
    } catch (const std::exception& e) {
        LVA_LOGW(kTag, "failed to parse %s as JSON: %s",
                 storage_path.c_str(), e.what());
        return ForPath(storage_path);
    }

    if (!obj.is_object()) {
        LVA_LOGW(kTag, "preferences file %s is not a JSON object",
                 storage_path.c_str());
        return ForPath(storage_path);
    }

    Preferences p;
    PopulateFromJson(p, obj, storage_path);
    p.raw_data_text_ = std::move(text);
    LVA_LOGI(kTag, "loaded preferences from %s (format=%s)",
             storage_path.c_str(),
             p.format_ == Format::kSound ? "sound" : "preferences");
    return p;
}

bool Preferences::SaveToFile(const std::filesystem::path& storage_path) const {
    std::error_code ec;
    if (storage_path.has_parent_path()) {
        std::filesystem::create_directories(storage_path.parent_path(), ec);
        if (ec) {
            LVA_LOGW(kTag, "failed to create parent of %s: %s",
                     storage_path.c_str(), ec.message().c_str());
            return false;
        }
    }

    nlohmann::json obj;
    if (!raw_data_text_.empty()) {
        try {
            obj = nlohmann::json::parse(raw_data_text_);
        } catch (...) {
            obj = nlohmann::json::object();
        }
        if (!obj.is_object()) {
            obj = nlohmann::json::object();
        }
    } else {
        obj = nlohmann::json::object();
    }

    ApplyToJson(*this, obj);

    const auto tmp_path = storage_path.string() + ".tmp";
    const std::string payload = obj.dump(4) + '\n';

    // Write + fsync the temp file, atomically rename, then fsync the
    // parent directory. Without the fsyncs the data can sit in the page
    // cache and be lost if power is cut right after saving.
    const int fd = ::open(tmp_path.c_str(),
                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LVA_LOGW(kTag, "failed to open %s for write: %s",
                 tmp_path.c_str(), std::strerror(errno));
        return false;
    }
    {
        std::size_t off = 0;
        bool write_ok = true;
        while (off < payload.size()) {
            const ssize_t n = ::write(fd, payload.data() + off,
                                      payload.size() - off);
            if (n < 0) {
                if (errno == EINTR) continue;
                LVA_LOGW(kTag, "write to %s failed: %s",
                         tmp_path.c_str(), std::strerror(errno));
                write_ok = false;
                break;
            }
            off += static_cast<std::size_t>(n);
        }
        if (write_ok && ::fsync(fd) != 0) {
            LVA_LOGW(kTag, "fsync %s failed: %s",
                     tmp_path.c_str(), std::strerror(errno));
            write_ok = false;
        }
        if (::close(fd) != 0 && write_ok) {
            LVA_LOGW(kTag, "close %s failed: %s",
                     tmp_path.c_str(), std::strerror(errno));
            write_ok = false;
        }
        if (!write_ok) {
            std::filesystem::remove(tmp_path, ec);
            return false;
        }
    }

    std::filesystem::rename(tmp_path, storage_path, ec);
    if (ec) {
        LVA_LOGW(kTag, "rename %s -> %s failed: %s",
                 tmp_path.c_str(), storage_path.c_str(),
                 ec.message().c_str());
        std::filesystem::remove(tmp_path, ec);
        return false;
    }

    // Persist the directory entry (the rename) to disk as well.
    const auto dir = storage_path.has_parent_path()
                         ? storage_path.parent_path()
                         : std::filesystem::path(".");
    const int dfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) {
        if (::fsync(dfd) != 0) {
            LVA_LOGW(kTag, "fsync dir %s failed: %s",
                     dir.c_str(), std::strerror(errno));
        }
        ::close(dfd);
    }
    return true;
}

}  // namespace lva::state
