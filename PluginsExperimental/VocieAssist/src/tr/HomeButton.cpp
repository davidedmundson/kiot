#include "tr/HomeButton.h"

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <chrono>
#include <cstring>

#include "protocol/MessageRegistry.h"
#include "state/ServerState.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::tr {

namespace {

constexpr const char* kTag           = "home_btn";
constexpr int    kKeyHome            = 102;     // Linux input KEY_HOME
constexpr int    kEvKey              = 1;       // EV_KEY
constexpr int    kClickWindowMs      = 500;     // multi-click grouping
constexpr int    kPollTimeoutMs      = 100;

}  // namespace

HomeButton::HomeButton(const Options& opts, lva::state::ServerState& state)
    : opts_(opts), state_(state) {}

HomeButton::~HomeButton() { Stop(); }

int HomeButton::Start() {
    if (event_fd_ >= 0) return event_fd_;
    event_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd_ < 0) {
        LVA_LOGE(kTag, "eventfd failed: %s", std::strerror(errno));
        return -1;
    }
    stop_requested_.store(false, std::memory_order_relaxed);
    thread_ = std::thread([this] { ThreadLoop(); });
    LVA_LOGI(kTag, "started (input=%s, entity_key=%u)",
             opts_.input_device.c_str(), opts_.entity_key);
    return event_fd_;
}

void HomeButton::Stop() {
    if (!thread_.joinable()) return;
    stop_requested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) thread_.join();
    if (event_fd_ >= 0) {
        ::close(event_fd_);
        event_fd_ = -1;
    }
}

void HomeButton::OnMainLoopWake() {
    // Drain eventfd.
    std::uint64_t expirations = 0;
    while (::read(event_fd_, &expirations, sizeof(expirations)) > 0) {}

    const int clicks = pending_clicks_.exchange(0, std::memory_order_acq_rel);
    if (clicks <= 0) return;

    const char* event_type = nullptr;
    if      (clicks == 1) event_type = "single_press";
    else if (clicks == 2) event_type = "double_press";
    else                   event_type = "triple_press";  // ≥3

    if (!state_.broadcast) {
        LVA_LOGW(kTag, "no broadcast hook; click event dropped");
        return;
    }
    LVA_LOGI(kTag, "%d click(s) -> %s", clicks, event_type);

    ::EventResponse resp;
    resp.set_key(opts_.entity_key);
    resp.set_event_type(event_type);
    state_.broadcast(lva::proto::kIdEventResponse, resp);
}

void HomeButton::ThreadLoop() {
    const int fd = ::open(opts_.input_device.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LVA_LOGE(kTag, "open(%s) failed: %s",
                 opts_.input_device.c_str(), std::strerror(errno));
        return;
    }

    int    click_count           = 0;
    auto   last_release_time     = std::chrono::steady_clock::time_point{};
    bool   release_pending_flush = false;

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        const auto now = std::chrono::steady_clock::now();
        int poll_timeout = kPollTimeoutMs;
        if (release_pending_flush) {
            const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_release_time).count();
            if (elapsed_ms >= kClickWindowMs) {
                pending_clicks_.store(click_count,
                                      std::memory_order_release);
                std::uint64_t one = 1;
                ::write(event_fd_, &one, sizeof(one));
                click_count           = 0;
                release_pending_flush = false;
                continue;
            }
            poll_timeout = static_cast<int>(kClickWindowMs - elapsed_ms);
            if (poll_timeout < 1) poll_timeout = 1;
        }

        struct pollfd pfd{};
        pfd.fd      = fd;
        pfd.events  = POLLIN;
        const int pr = ::poll(&pfd, 1, poll_timeout);
        if (pr < 0) {
            if (errno == EINTR) continue;
            LVA_LOGE(kTag, "poll failed: %s", std::strerror(errno));
            break;
        }
        if (pr == 0) continue;  // timeout
        if (!(pfd.revents & POLLIN)) continue;

        struct input_event ev;
        const ssize_t n = ::read(fd, &ev, sizeof(ev));
        if (n < static_cast<ssize_t>(sizeof(ev))) continue;

        // Only key release of KEY_HOME counts as a click.
        if (ev.type != kEvKey) continue;
        if (ev.code != kKeyHome) continue;
        if (ev.value != 0) continue;  // 0 = release

        const auto now2 = std::chrono::steady_clock::now();
        if (release_pending_flush) {
            const auto since_last =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now2 - last_release_time).count();
            if (since_last < kClickWindowMs) {
                ++click_count;
            } else {
                pending_clicks_.store(click_count,
                                      std::memory_order_release);
                std::uint64_t one = 1;
                ::write(event_fd_, &one, sizeof(one));
                click_count = 1;
            }
        } else {
            click_count = 1;
        }
        last_release_time     = now2;
        release_pending_flush = true;
    }

    // Flush any pending click count on shutdown.
    if (click_count > 0) {
        pending_clicks_.store(click_count, std::memory_order_release);
        std::uint64_t one = 1;
        ::write(event_fd_, &one, sizeof(one));
    }

    ::close(fd);
    LVA_LOGD(kTag, "thread exiting");
}

}  // namespace lva::tr
