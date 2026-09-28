
#pragma once

#include <chrono>
#include <filesystem>
#include <sys/stat.h>

namespace lva::state { class ServerState; }

namespace lva::tr {

class SoundConfWatcher {
   public:
    SoundConfWatcher(lva::state::ServerState& state,
                     std::filesystem::path path);

    void Poll();

   private:
    void ReloadAndApply();

    lva::state::ServerState& state_;
    std::filesystem::path    path_;
    struct timespec          last_mtime_{0, 0};
    std::chrono::steady_clock::time_point last_check_{};
};

}  // namespace lva::tr
