
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace lva::state { class ServerState; }

namespace lva::tr {

class HomeButton {
   public:
    struct Options {
        std::string input_device  = "/dev/input/event0";
        std::uint32_t entity_key  = 0;
    };

    HomeButton(const Options& opts, lva::state::ServerState& state);
    ~HomeButton();

    HomeButton(const HomeButton&)            = delete;
    HomeButton& operator=(const HomeButton&) = delete;

    int Start();

    void OnMainLoopWake();

    void Stop();

   private:
    void ThreadLoop();

    Options                 opts_;
    lva::state::ServerState& state_;

    int                     event_fd_       = -1;   // worker → main
    std::thread             thread_;
    std::atomic<bool>       stop_requested_{false};
    std::atomic<int>        pending_clicks_{0};      // 1/2/3
};

}  // namespace lva::tr
