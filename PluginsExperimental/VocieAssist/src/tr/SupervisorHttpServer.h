
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace lva::tr {

class Supervisor;

class SupervisorHttpServer {
   public:
    SupervisorHttpServer(Supervisor& supervisor, std::uint16_t port = 8086);
    ~SupervisorHttpServer();

    SupervisorHttpServer(const SupervisorHttpServer&)            = delete;
    SupervisorHttpServer& operator=(const SupervisorHttpServer&) = delete;

    // Bind + listen + spawn accept loop. Returns false on bind error.
    bool Start();

    // Close listening socket, signal accept loop, join.
    void Stop();

   private:
    void AcceptLoop();
    void HandleConnection(int conn_fd);
    void HandleGet(int conn_fd, const std::string& path);
    void HandlePost(int conn_fd, const std::string& path,
                    const std::string& content_type,
                    const std::string& body);
    void SendJson(int conn_fd, int status, const std::string& json);
    void SendText(int conn_fd, int status, const std::string& body);

    Supervisor&         supervisor_;
    std::uint16_t       port_;
    int                 listen_fd_      = -1;
    std::atomic<bool>   running_{false};
    std::thread         accept_thread_;
};

}  // namespace lva::tr
