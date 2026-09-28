
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lva::proto {

// One fully-parsed inbound frame.
struct InboundFrame {
    std::uint32_t msg_type_id;
    std::vector<std::uint8_t> payload;
};

enum class ParseStatus {
    kOk,
    kNeedMore,
    kBadPreamble,
    kBadVarint,
};

ParseStatus ParseFrame(std::span<const std::uint8_t> input,
                       InboundFrame& out,
                       std::size_t& bytes_consumed) noexcept;

std::vector<std::uint8_t> EncodeFrame(std::uint32_t msg_type_id,
                                      std::span<const std::uint8_t> payload);

std::vector<std::uint8_t> EncodeFrame(std::uint32_t msg_type_id,
                                      std::string_view payload);

void AppendVarUint32(std::vector<std::uint8_t>& out, std::uint32_t value);

struct VarUintResult {
    std::optional<std::uint32_t> value;
    std::size_t bytes_consumed;
    bool invalid;  // true => unrecoverable (>5 bytes), close connection.
};

VarUintResult ReadVarUint32(std::span<const std::uint8_t> input,
                            std::size_t pos) noexcept;

}  // namespace lva::proto
