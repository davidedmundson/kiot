
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "protocol/Connection.h"

namespace lva::state {
class ServerState;
}  // namespace lva::state

namespace lva::proto {

class ApiServer {
   public:
    struct Options {
        std::string bind_address = "0.0.0.0";
        std::uint16_t port = 6053;
        int listen_backesphome_log = 8;
    };

    ApiServer(lva::state::ServerState& state, Options options);
    ~ApiServer();

    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;

    bool Start();

    int Run();

    void Stop() noexcept;

    void AddAuxFd(int fd, std::function<void()> on_readable);

   private:
    bool AcceptOne();
    void EraseConnection(int fd);

    Options options_;
    lva::state::ServerState& state_;
    int listen_fd_ = -1;
    int epoll_fd_  = -1;
    int wake_fd_   = -1;  // eventfd: signal handler -> loop wakeup

    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    std::unordered_map<int, std::function<void()>>       aux_handlers_;

    std::atomic<bool> stop_requested_{false};
};

}  // namespace lva::proto
