
#pragma once

#include <atomic>
#include <chrono>
#include <string>

namespace lva::state { class ServerState; }

namespace lva::tr {

class MicMuteGpio {
   public:
    explicit MicMuteGpio(lva::state::ServerState& state,
                         std::string gpio_path = "/sys/class/gpio/gpio438/value");

    void Poll();

    bool ReadAndApplyOnce();

    void SyncToHardware(bool muted);

    bool Available() const noexcept { return available_; }

   private:
    bool ReadRaw(int* out_value);

    lva::state::ServerState&  state_;
    std::string               gpio_path_;
    bool                      available_  = false;
    int                       last_value_ = -1;  // last GPIO digit seen
    std::chrono::steady_clock::time_point last_poll_{};
};

}  // namespace lva::tr
