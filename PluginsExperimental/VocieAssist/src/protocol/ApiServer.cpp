#include "protocol/ApiServer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <utility>

#include "state/ServerState.h"
#include "util/Log.h"

namespace lva::proto {

namespace {

constexpr const char* kTag = "api";

bool SetNonBlocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return false;
    return true;
}

}  // namespace

ApiServer::ApiServer(lva::state::ServerState& state, Options options)
    : options_(std::move(options)), state_(state) {}

ApiServer::~ApiServer() {
    state_.broadcast = nullptr;
    if (listen_fd_ >= 0) ::close(listen_fd_);
    if (epoll_fd_  >= 0) ::close(epoll_fd_);
    if (wake_fd_   >= 0) ::close(wake_fd_);
}

void ApiServer::Stop() noexcept {
    stop_requested_.store(true, std::memory_order_relaxed);
    const int fd = wake_fd_;
    if (fd >= 0) {
        const std::uint64_t value = 1;
        ssize_t r;
        do {
            r = ::write(fd, &value, sizeof(value));
        } while (r < 0 && errno == EINTR);
        (void)r;
    }
}

void ApiServer::AddAuxFd(int fd, std::function<void()> on_readable) {
    if (epoll_fd_ < 0) {
        LVA_LOGE(kTag, "AddAuxFd called before Start()");
        return;
    }
    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LVA_LOGE(kTag, "epoll_ctl(aux fd=%d) failed: %s", fd,
                 std::strerror(errno));
        return;
    }
    aux_handlers_.emplace(fd, std::move(on_readable));
    LVA_LOGD(kTag, "registered aux fd=%d", fd);
}

bool ApiServer::Start() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listen_fd_ < 0) {
        LVA_LOGE(kTag, "socket() failed: %s", std::strerror(errno));
        return false;
    }

    int one = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        LVA_LOGW(kTag, "setsockopt(SO_REUSEADDR) failed: %s", std::strerror(errno));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(options_.port);
    if (options_.bind_address == "0.0.0.0" || options_.bind_address.empty()) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (::inet_pton(AF_INET, options_.bind_address.c_str(),
                           &addr.sin_addr) != 1) {
        LVA_LOGE(kTag, "inet_pton(%s) failed", options_.bind_address.c_str());
        return false;
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LVA_LOGE(kTag, "bind(%s:%u) failed: %s", options_.bind_address.c_str(),
                 options_.port, std::strerror(errno));
        return false;
    }

    if (::listen(listen_fd_, options_.listen_backesphome_log) < 0) {
        LVA_LOGE(kTag, "listen() failed: %s", std::strerror(errno));
        return false;
    }

    if (!SetNonBlocking(listen_fd_)) {
        LVA_LOGE(kTag, "fcntl(O_NONBLOCK) on listen fd failed: %s",
                 std::strerror(errno));
        return false;
    }

    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        LVA_LOGE(kTag, "epoll_create1 failed: %s", std::strerror(errno));
        return false;
    }

    wake_fd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd_ < 0) {
        LVA_LOGE(kTag, "eventfd failed: %s", std::strerror(errno));
        return false;
    }

    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = listen_fd_;
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
            LVA_LOGE(kTag, "epoll_ctl(listen) failed: %s", std::strerror(errno));
            return false;
        }
    }

    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = wake_fd_;
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wake_fd_, &ev) < 0) {
            LVA_LOGE(kTag, "epoll_ctl(wakefd) failed: %s", std::strerror(errno));
            return false;
        }
    }

    LVA_LOGI(kTag, "listening on %s:%u (device_name=%s)",
             options_.bind_address.c_str(), options_.port,
             state_.name.c_str());

    state_.broadcast = [this](std::uint32_t msg_type_id,
                              const ::google::protobuf::MessageLite& msg) {
        for (auto& [fd, conn] : connections_) {
            (void)fd;
            if (conn) {
                conn->SendSerializedMessage(msg_type_id, msg);
            }
        }
    };

    return true;
}

bool ApiServer::AcceptOne() {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    const int fd = ::accept4(listen_fd_, reinterpret_cast<sockaddr*>(&peer),
                             &peer_len, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
        if (errno == EINTR) return true;
        LVA_LOGE(kTag, "accept4 failed: %s", std::strerror(errno));
        return false;
    }

    int one = 1;
    if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0) {
        LVA_LOGW(kTag, "setsockopt(TCP_NODELAY) failed: %s", std::strerror(errno));
    }

    char ip_str[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &peer.sin_addr, ip_str, sizeof(ip_str));
    char peer_label[64];
    std::snprintf(peer_label, sizeof(peer_label), "%s:%u", ip_str,
                  ntohs(peer.sin_port));

    auto conn = std::make_unique<Connection>(fd, state_,
                                             std::string(peer_label));

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = fd;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LVA_LOGE(kTag, "epoll_ctl(client) failed: %s", std::strerror(errno));
        // conn destructor closes fd
        return true;
    }

    connections_.emplace(fd, std::move(conn));
    return true;
}

void ApiServer::EraseConnection(int fd) {
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        connections_.erase(it);
    } else {
        ::close(fd);
    }
    // Notify satellite of disconnect so it can clean up pipeline state.
    if (connections_.empty() && state_.on_client_disconnected) {
        state_.on_client_disconnected();
    }
}

int ApiServer::Run() {
    constexpr int kMaxEvents = 16;
    epoll_event events[kMaxEvents];

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        const int n = ::epoll_wait(epoll_fd_, events, kMaxEvents, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            LVA_LOGE(kTag, "epoll_wait failed: %s", std::strerror(errno));
            return 1;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;

            if (fd == wake_fd_) {
                std::uint64_t v = 0;
                while (::read(wake_fd_, &v, sizeof(v)) > 0) {
                    // drain
                }
                // Loop will re-check stop_requested_ at top.
                continue;
            }

            if (fd == listen_fd_) {
                while (AcceptOne()) {
                    // drain accept queue
                }
                continue;
            }

            // Auxiliary fd registered via AddAuxFd?
            if (auto it = aux_handlers_.find(fd); it != aux_handlers_.end()) {
                try {
                    it->second();
                } catch (...) {
                    LVA_LOGW(kTag, "aux fd %d handler threw", fd);
                }
                continue;
            }

            // Otherwise it's a client connection.
            auto it = connections_.find(fd);
            if (it == connections_.end()) {
                // Stray event — ignore.
                continue;
            }
            Connection& conn = *it->second;

            const std::uint32_t mask = events[i].events;
            bool keep_alive = true;
            if (mask & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                keep_alive = false;
            } else {
                if (mask & EPOLLIN) {
                    keep_alive = conn.OnReadable();
                }
                if (keep_alive && (mask & EPOLLOUT)) {
                    keep_alive = conn.OnWritable();
                }
                if (keep_alive && conn.closed()) {
                    keep_alive = false;
                }
            }
            if (!keep_alive) {
                EraseConnection(fd);
            }
        }
    }

    LVA_LOGI(kTag, "stopping (cleaning up %zu connection(s))",
             connections_.size());
    // Close all client connections cleanly.
    while (!connections_.empty()) {
        EraseConnection(connections_.begin()->first);
    }
    return 0;
}

}  // namespace lva::proto
