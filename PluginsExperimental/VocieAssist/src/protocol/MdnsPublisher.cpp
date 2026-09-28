#include "protocol/MdnsPublisher.h"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>

#include <atomic>
#include <thread>

#include "util/Log.h"

namespace lva::proto {

namespace {
constexpr const char* kTag = "mdns";
}

struct MdnsPublisher::Impl {
    AvahiSimplePoll* poll   = nullptr;
    AvahiClient*     client = nullptr;
    AvahiEntryGroup* group  = nullptr;
    std::thread      thread;
    std::atomic<bool> running{false};
    Options opts;

    static void GroupCallback(AvahiEntryGroup* g,
                              AvahiEntryGroupState state,
                              void* userdata) {
        (void)g;
        (void)userdata;
        if (state == AVAHI_ENTRY_GROUP_COLLISION) {
            LVA_LOGW(kTag, "service name collision");
        } else if (state == AVAHI_ENTRY_GROUP_FAILURE) {
            LVA_LOGE(kTag, "entry group failure");
        } else if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
            LVA_LOGI(kTag, "service registered");
        }
    }

    static void ClientCallback(AvahiClient* c,
                               AvahiClientState state,
                               void* userdata) {
        auto* self = static_cast<Impl*>(userdata);
        if (state == AVAHI_CLIENT_S_RUNNING) {
            self->RegisterService(c);
        } else if (state == AVAHI_CLIENT_FAILURE) {
            LVA_LOGE(kTag, "client failure: %s",
                     avahi_strerror(avahi_client_errno(c)));
            avahi_simple_poll_quit(self->poll);
        }
    }

    void RegisterService(AvahiClient* c) {
        if (!group) {
            group = avahi_entry_group_new(c, GroupCallback, this);
            if (!group) {
                LVA_LOGE(kTag, "avahi_entry_group_new failed");
                return;
            }
        }
        if (avahi_entry_group_is_empty(group)) {
            const std::string mac_txt   = "mac=" + opts.mac;
            const std::string ver_txt   = "version=" + opts.version;
            const char* board_txt       = "board=aarch64";
            const char* platform_txt    = "platform=ThirdReality";
            const char* network_txt     = "network=wifi";

            int ret = avahi_entry_group_add_service(
                group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                static_cast<AvahiPublishFlags>(0),
                opts.name.c_str(),
                "_esphomelib._tcp", nullptr, nullptr,
                opts.port,
                mac_txt.c_str(),
                ver_txt.c_str(),
                board_txt,
                platform_txt,
                network_txt,
                nullptr);
            if (ret < 0) {
                LVA_LOGE(kTag, "add_service failed: %s",
                         avahi_strerror(ret));
                return;
            }
            ret = avahi_entry_group_commit(group);
            if (ret < 0) {
                LVA_LOGE(kTag, "commit failed: %s",
                         avahi_strerror(ret));
            }
        }
    }
};

MdnsPublisher::~MdnsPublisher() {
    Stop();
}

bool MdnsPublisher::Start(const Options& opts) {
    if (impl_) return true;  // already running

    auto* p = new Impl();
    p->opts = opts;

    p->poll = avahi_simple_poll_new();
    if (!p->poll) {
        LVA_LOGE(kTag, "avahi_simple_poll_new failed");
        delete p;
        return false;
    }

    int error = 0;
    p->client = avahi_client_new(
        avahi_simple_poll_get(p->poll),
        AVAHI_CLIENT_NO_FAIL,
        Impl::ClientCallback, p, &error);
    if (!p->client) {
        LVA_LOGE(kTag, "avahi_client_new failed: %s",
                 avahi_strerror(error));
        avahi_simple_poll_free(p->poll);
        delete p;
        return false;
    }

    p->running = true;
    p->thread = std::thread([p] {
        LVA_LOGD(kTag, "poll loop started");
        avahi_simple_poll_loop(p->poll);
        p->running = false;
        LVA_LOGD(kTag, "poll loop exited");
    });

    impl_ = p;
    LVA_LOGI(kTag, "started (_esphomelib._tcp, name=%s, port=%u)",
             opts.name.c_str(), static_cast<unsigned>(opts.port));
    return true;
}

void MdnsPublisher::Stop() {
    if (!impl_) return;
    avahi_simple_poll_quit(impl_->poll);
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
    if (impl_->client) avahi_client_free(impl_->client);
    if (impl_->poll)   avahi_simple_poll_free(impl_->poll);
    delete impl_;
    impl_ = nullptr;
}

}  // namespace lva::proto
