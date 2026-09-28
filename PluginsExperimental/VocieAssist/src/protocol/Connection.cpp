#include "protocol/Connection.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>

#include "entities/Entity.h"
#include "protocol/MessageRegistry.h"
#include "satellite/Satellite.h"
#include "tr/TimeZoneSync.h"
#include "state/ServerState.h"
#include "util/Log.h"

#include "api.pb.h"

namespace lva::proto {

namespace {

constexpr const char* kTag = "conn";

constexpr std::uint32_t kApiVersionMajor = 1;
constexpr std::uint32_t kApiVersionMinor = 10;

constexpr std::size_t kMaxReadBufferBytes = 1u * 1024u * 1024u;

constexpr std::size_t kReadChunkBytes = 4096;

}  // namespace

Connection::Connection(int fd,
                       lva::state::ServerState& state,
                       std::string peer_label)
    : fd_(fd), state_(state), peer_label_(std::move(peer_label)) {
    LVA_LOGI(kTag, "[%s] connected (fd=%d)", peer_label_.c_str(), fd_);
}

Connection::~Connection() {
    Close();
}

void Connection::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    closed_ = true;
}

bool Connection::OnReadable() {
    if (closed_) return false;

    const std::size_t old_size = read_buf_.size();
    if (old_size + kReadChunkBytes > kMaxReadBufferBytes) {
        LVA_LOGE(kTag, "[%s] read buffer overflow (%zu bytes), closing",
                 peer_label_.c_str(), old_size);
        return false;
    }
    read_buf_.resize(old_size + kReadChunkBytes);

    const ssize_t n = ::recv(fd_, read_buf_.data() + old_size,
                             kReadChunkBytes, 0);
    if (n == 0) {
        LVA_LOGI(kTag, "[%s] peer closed", peer_label_.c_str());
        read_buf_.resize(old_size);
        return false;
    }
    if (n < 0) {
        if (errno == EINTR) {
            read_buf_.resize(old_size);
            return true;  // try again later
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            read_buf_.resize(old_size);
            return true;
        }
        LVA_LOGE(kTag, "[%s] recv failed: %s", peer_label_.c_str(),
                 std::strerror(errno));
        return false;
    }
    read_buf_.resize(old_size + static_cast<std::size_t>(n));

    return ProcessBuffer();
}

bool Connection::ProcessBuffer() {
    while (!read_buf_.empty()) {
        InboundFrame frame;
        std::size_t consumed = 0;
        const ParseStatus status =
            ParseFrame(std::span<const std::uint8_t>(read_buf_.data(),
                                                     read_buf_.size()),
                       frame, consumed);
        if (status == ParseStatus::kNeedMore) {
            return true;  // wait for more bytes
        }
        if (status != ParseStatus::kOk) {
            LVA_LOGE(kTag, "[%s] frame parse failed (%d), closing",
                     peer_label_.c_str(), static_cast<int>(status));
            return false;
        }

        if (consumed >= read_buf_.size()) {
            read_buf_.clear();
        } else {
            read_buf_.erase(read_buf_.begin(),
                            read_buf_.begin() + static_cast<std::ptrdiff_t>(consumed));
        }

        if (!DispatchFrame(frame)) {
            return false;
        }
        if (closed_) {
            return false;
        }
    }
    return true;
}

bool Connection::DispatchFrame(const InboundFrame& frame) {
    LVA_LOGD(kTag, "[%s] <- type=%u (%.*s) len=%zu",
             peer_label_.c_str(), frame.msg_type_id,
             static_cast<int>(MessageName(frame.msg_type_id).size()),
             MessageName(frame.msg_type_id).data(),
             frame.payload.size());

    switch (frame.msg_type_id) {
        case kIdHelloRequest: {
            ::HelloRequest req;
            if (!req.ParseFromArray(frame.payload.data(),
                                    static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] HelloRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            ::HelloResponse resp;
            resp.set_api_version_major(kApiVersionMajor);
            resp.set_api_version_minor(kApiVersionMinor);
            resp.set_name(state_.name);
            return Send(resp);
        }

        case kIdAuthenticationRequest: {
            ::AuthenticationResponse resp;
            const bool ok = Send(resp);
            if (ok) {
                // Ask HA for its timezone.
                ::GetTimeRequest gt;
                Send(gt);

                const auto sink = MakeSink();
                for (const auto& entity : state_.entities) {
                    entity->OnSubscribeStates(sink);
                }
            }
            if (ok && state_.on_client_authenticated) {
                state_.on_client_authenticated();
            }
            return ok;
        }

        case kIdGetTimeResponse: {
            ::GetTimeResponse gtr;
            if (!gtr.ParseFromArray(
                    frame.payload.data(),
                    static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] GetTimeResponse parse failed",
                         peer_label_.c_str());
                return false;
            }
            const std::string tz = gtr.timezone();
            LVA_LOGI(kTag, "[%s] HA timezone=%s epoch=%u",
                     peer_label_.c_str(),
                     tz.empty() ? "(empty)" : tz.c_str(),
                     gtr.epoch_seconds());
            if (!tz.empty()) {
                lva::tr::ApplyTimezone(tz);
            }
            return true;
        }

        case kIdPingRequest: {
            ::PingResponse resp;
            return Send(resp);
        }

        case kIdDisconnectRequest: {
            ::DisconnectResponse resp;
            (void)Send(resp);
            // After answering, tear the connection down ourselves.
            LVA_LOGI(kTag, "[%s] disconnect requested", peer_label_.c_str());
            closed_ = true;
            return true;
        }

        case kIdDeviceInfoRequest: {
            ::DeviceInfoResponse resp;
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            resp.set_uses_password(false);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
            resp.set_name(state_.name);
            resp.set_friendly_name(
                state_.friendly_name.empty() ? state_.name
                                             : state_.friendly_name);
            if (!state_.mac_address.empty()) {
                resp.set_mac_address(state_.mac_address);
            }
            resp.set_project_name("ThirdReality.Linux Voice Assistant (C++)");
            resp.set_project_version(state_.version);
            resp.set_esphome_version(state_.esphome_version);
            resp.set_manufacturer("ThirdReality");
            resp.set_model("Linux Voice Assistant");

            constexpr std::uint32_t kVoiceFeatureFlags =
                (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5);
            resp.set_voice_assistant_feature_flags(kVoiceFeatureFlags);

            return Send(resp);
        }

        case kIdListEntitiesRequest: {
            const auto sink = MakeSink();
            for (const auto& entity : state_.entities) {
                entity->OnListEntities(sink);
            }
            ::ListEntitiesDoneResponse done;
            return Send(done);
        }

        case kIdSubscribeStatesRequest:
        case kIdSubscribeHomeAssistantStatesRequest: {
            const auto sink = MakeSink();
            for (const auto& entity : state_.entities) {
                entity->OnSubscribeStates(sink);
            }
            return true;
        }

        case kIdSwitchCommandRequest: {
            ::SwitchCommandRequest cmd;
            if (!cmd.ParseFromArray(
                    frame.payload.data(),
                    static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] SwitchCommandRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            const auto sink = MakeSink();
            for (const auto& entity : state_.entities) {
                entity->OnCommand(cmd, kIdSwitchCommandRequest, sink);
            }
            return true;
        }

        case kIdButtonCommandRequest: {
            ::ButtonCommandRequest cmd;
            if (!cmd.ParseFromArray(
                    frame.payload.data(),
                    static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] ButtonCommandRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            const auto sink = MakeSink();
            for (const auto& entity : state_.entities) {
                entity->OnCommand(cmd, kIdButtonCommandRequest, sink);
            }
            return true;
        }

        case kIdNumberCommandRequest: {
            ::NumberCommandRequest cmd;
            if (!cmd.ParseFromArray(
                    frame.payload.data(),
                    static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] NumberCommandRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            const auto sink = MakeSink();
            for (const auto& entity : state_.entities) {
                entity->OnCommand(cmd, kIdNumberCommandRequest, sink);
            }
            return true;
        }

        case kIdSelectCommandRequest: {
            ::SelectCommandRequest cmd;
            if (!cmd.ParseFromArray(
                    frame.payload.data(),
                    static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] SelectCommandRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            const auto sink = MakeSink();
            for (const auto& entity : state_.entities) {
                entity->OnCommand(cmd, kIdSelectCommandRequest, sink);
            }
            return true;
        }

        case kIdMediaPlayerCommandRequest: {
            ::MediaPlayerCommandRequest cmd;
            if (!cmd.ParseFromArray(
                    frame.payload.data(),
                    static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] MediaPlayerCommandRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            const auto sink = MakeSink();
            for (const auto& entity : state_.entities) {
                entity->OnCommand(cmd, kIdMediaPlayerCommandRequest, sink);
            }
            return true;
        }

        case kIdUpdateCommandRequest: {
            ::UpdateCommandRequest cmd;
            if (!cmd.ParseFromArray(
                    frame.payload.data(),
                    static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] UpdateCommandRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            const auto sink = MakeSink();
            for (const auto& entity : state_.entities) {
                entity->OnCommand(cmd, kIdUpdateCommandRequest, sink);
            }
            return true;
        }

        // ---- Voice assistant ----
        case kIdSubscribeVoiceAssistantRequest: {
            ::SubscribeVoiceAssistantRequest m;
            if (!m.ParseFromArray(frame.payload.data(),
                                  static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] SubscribeVoiceAssistantRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            LVA_LOGI(kTag, "[%s] VoiceAssistant subscribe=%d flags=%u",
                     peer_label_.c_str(), m.subscribe() ? 1 : 0,
                     m.flags());
            return true;
        }
        case kIdVoiceAssistantConfigurationRequest: {
            ::VoiceAssistantConfigurationRequest m;
            if (!m.ParseFromArray(frame.payload.data(),
                                  static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] VoiceAssistantConfigurationRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            if (state_.satellite) {
                state_.satellite->HandleMessage(
                    kIdVoiceAssistantConfigurationRequest, m);
            }
            return true;
        }
        case kIdVoiceAssistantSetConfiguration: {
            ::VoiceAssistantSetConfiguration m;
            if (!m.ParseFromArray(frame.payload.data(),
                                  static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] VoiceAssistantSetConfiguration parse failed",
                         peer_label_.c_str());
                return false;
            }
            if (state_.satellite) {
                state_.satellite->HandleMessage(
                    kIdVoiceAssistantSetConfiguration, m);
            }
            return true;
        }
        case kIdVoiceAssistantEventResponse: {
            ::VoiceAssistantEventResponse m;
            if (!m.ParseFromArray(frame.payload.data(),
                                  static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] VoiceAssistantEventResponse parse failed",
                         peer_label_.c_str());
                return false;
            }
            if (state_.satellite) {
                state_.satellite->HandleMessage(
                    kIdVoiceAssistantEventResponse, m);
            }
            return true;
        }
        case kIdVoiceAssistantTimerEventResponse: {
            ::VoiceAssistantTimerEventResponse m;
            if (!m.ParseFromArray(frame.payload.data(),
                                  static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] VoiceAssistantTimerEventResponse parse failed",
                         peer_label_.c_str());
                return false;
            }
            if (state_.satellite) {
                state_.satellite->HandleMessage(
                    kIdVoiceAssistantTimerEventResponse, m);
            }
            return true;
        }
        case kIdVoiceAssistantAnnounceRequest: {
            ::VoiceAssistantAnnounceRequest m;
            if (!m.ParseFromArray(frame.payload.data(),
                                  static_cast<int>(frame.payload.size()))) {
                LVA_LOGE(kTag, "[%s] VoiceAssistantAnnounceRequest parse failed",
                         peer_label_.c_str());
                return false;
            }
            if (state_.satellite) {
                state_.satellite->HandleMessage(
                    kIdVoiceAssistantAnnounceRequest, m);
            }
            return true;
        }

        default:
            LVA_LOGD(kTag, "[%s] ignoring unhandled message type %u",
                     peer_label_.c_str(), frame.msg_type_id);
            return true;
    }
}

bool Connection::OnWritable() {
    if (closed_ || write_buf_.empty()) {
        return true;
    }
    while (!write_buf_.empty()) {
        const ssize_t n = ::send(fd_, write_buf_.data(), write_buf_.size(),
                                 MSG_NOSIGNAL);
        if (n > 0) {
            const std::size_t sent = static_cast<std::size_t>(n);
            if (sent >= write_buf_.size()) {
                write_buf_.clear();
            } else {
                write_buf_.erase(
                    write_buf_.begin(),
                    write_buf_.begin() + static_cast<std::ptrdiff_t>(sent));
            }
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return true;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        LVA_LOGE(kTag, "[%s] send failed: %s", peer_label_.c_str(),
                 std::strerror(errno));
        return false;
    }
    return true;
}

bool Connection::SendRaw(std::span<const std::uint8_t> bytes) {
    if (closed_) return false;

    if (!write_buf_.empty()) {
        write_buf_.insert(write_buf_.end(), bytes.begin(), bytes.end());
        return true;
    }

    std::size_t sent_total = 0;
    while (sent_total < bytes.size()) {
        const ssize_t n = ::send(fd_, bytes.data() + sent_total,
                                 bytes.size() - sent_total, MSG_NOSIGNAL);
        if (n > 0) {
            sent_total += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // Buffer the remainder for OnWritable to drain.
            write_buf_.assign(bytes.begin() + static_cast<std::ptrdiff_t>(sent_total),
                              bytes.end());
            return true;
        }
        LVA_LOGE(kTag, "[%s] send failed: %s", peer_label_.c_str(),
                 std::strerror(errno));
        return false;
    }
    return true;
}

bool Connection::SendSerializedMessage(std::uint32_t msg_type_id,
                                       const ::google::protobuf::MessageLite& msg) {
    std::string payload;
    if (!msg.SerializeToString(&payload)) {
        LVA_LOGE(kTag, "[%s] failed to serialize msg type %u",
                 peer_label_.c_str(), msg_type_id);
        return false;
    }
    LVA_LOGD(kTag, "[%s] -> type=%u (%.*s) len=%zu",
             peer_label_.c_str(), msg_type_id,
             static_cast<int>(MessageName(msg_type_id).size()),
             MessageName(msg_type_id).data(),
             payload.size());
    auto frame = EncodeFrame(msg_type_id, payload);
    return SendRaw(std::span<const std::uint8_t>(frame.data(), frame.size()));
}

std::function<void(std::uint32_t,
                   const ::google::protobuf::MessageLite&)>
Connection::MakeSink() {
    return [this](std::uint32_t msg_type_id,
                  const ::google::protobuf::MessageLite& msg) {
        SendSerializedMessage(msg_type_id, msg);
    };
}

}  // namespace lva::proto
