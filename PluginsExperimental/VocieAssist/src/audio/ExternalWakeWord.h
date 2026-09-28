// ExternalWakeWord: downloads custom wake-word models advertised by
// Home Assistant during the VoiceAssistantConfigurationRequest
// handshake. Mirrors the Python build's
// VoiceSatelliteProtocol._download_external_wake_word: the model's
// JSON config and .tflite are fetched, with a size + SHA-256 cache
// check so an unchanged model is not re-downloaded.
//
// On this device the rootfs is an overlayfs (lowerdir=/rom, writable
// upperdir on the persistent /data partition), so downloads go into the
// same directory as the bundled wake words. They survive reboot and are
// picked up by ScanAvailableWakeWords / startup model loading without
// any special handling.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lva::audio {

// Metadata for one external wake word, as sent by Home Assistant in a
// VoiceAssistantExternalWakeWord proto message.
struct ExternalWakeWordInfo {
    std::string id;
    std::string wake_word;
    std::vector<std::string> trained_languages;
    std::string model_type;       // "micro" (only type we support)
    std::uint32_t model_size = 0; // expected .tflite size in bytes
    std::string model_hash;       // expected .tflite SHA-256 (hex, lower)
    std::string url;              // URL of the JSON config file
};

// Downloads the external wake word's JSON config and .tflite model into
// `dest_dir` (the same directory as the bundled wake words). If a cached
// .tflite already matches both model_size and model_hash, the download
// is skipped.
//
// Returns the path to the downloaded JSON config on success, or an
// empty path on failure.
std::filesystem::path DownloadExternalWakeWord(
    const ExternalWakeWordInfo& info,
    const std::filesystem::path& dest_dir);

}  // namespace lva::audio
