#include "audio/WakeWordScanner.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace lva::audio {

std::vector<WakeWordInfo> ScanAvailableWakeWords(
    const std::filesystem::path& dir) {
    std::vector<WakeWordInfo> result;
    namespace fs = std::filesystem;
    if (!fs::is_directory(dir)) return result;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        if (!f) continue;
        nlohmann::json j;
        try { f >> j; } catch (...) { continue; }

        WakeWordInfo info;
        info.id = entry.path().stem().string();
        if (info.id == "stop") continue;  // exclude stop model
        info.wake_word = j.value("wake_word", info.id);
        info.type = j.value("type", "");
        if (j.contains("trained_languages") &&
            j["trained_languages"].is_array()) {
            for (const auto& lang : j["trained_languages"]) {
                if (lang.is_string())
                    info.trained_languages.push_back(lang.get<std::string>());
            }
        }
        result.push_back(std::move(info));
    }
    return result;
}

std::string ReadWakeWordType(const std::filesystem::path& dir,
                             const std::string& id) {
    namespace fs = std::filesystem;
    const fs::path p = dir / (id + ".json");
    std::ifstream f(p);
    if (!f) return "";
    nlohmann::json j;
    try { f >> j; } catch (...) { return ""; }
    return j.value("type", "");
}

}  // namespace lva::audio
