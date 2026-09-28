// WakeWordScanner: reads available wake-word metadata from .json
// files without loading TFLite models. Isolated TU so nlohmann/json
// doesn't collide with api.pb.h's `esphome_log` extension.

#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace lva::audio {

struct WakeWordInfo {
    std::string id;
    std::string wake_word;
    std::vector<std::string> trained_languages;
    // "micro" or "open" (from JSON's "type" field).
    // Empty if missing — caller should default to "micro" for back-compat.
    std::string type;
};

std::vector<WakeWordInfo> ScanAvailableWakeWords(
    const std::filesystem::path& dir);

// Read just the "type" field from <dir>/<id>.json.
// Returns "micro", "open", or "" if the file is missing/invalid.
std::string ReadWakeWordType(const std::filesystem::path& dir,
                             const std::string& id);

}  // namespace lva::audio
