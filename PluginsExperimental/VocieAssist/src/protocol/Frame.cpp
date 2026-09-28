#include "protocol/Frame.h"

#include <cstring>

namespace lva::proto {

namespace {

constexpr std::uint8_t kPreamble = 0x00;
constexpr std::size_t kMaxVarintBytes = 5;

}  // namespace

void AppendVarUint32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<std::uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(value));
}

VarUintResult ReadVarUint32(std::span<const std::uint8_t> input,
                            std::size_t pos) noexcept {
    VarUintResult result{};
    result.invalid = false;

    std::uint32_t value = 0;
    std::size_t bytes = 0;

    while (true) {
        if (pos + bytes >= input.size()) {
            // Need more input; not a hard failure.
            result.value = std::nullopt;
            result.bytes_consumed = 0;
            result.invalid = false;
            return result;
        }
        if (bytes >= kMaxVarintBytes) {
            result.value = std::nullopt;
            result.bytes_consumed = bytes;
            result.invalid = true;
            return result;
        }

        const std::uint8_t byte = input[pos + bytes];
        value |= static_cast<std::uint32_t>(byte & 0x7F) << (7 * bytes);
        ++bytes;

        if ((byte & 0x80) == 0) {
            // Last byte of the varuint.
            result.value = value;
            result.bytes_consumed = bytes;
            result.invalid = false;
            return result;
        }
    }
}

ParseStatus ParseFrame(std::span<const std::uint8_t> input,
                       InboundFrame& out,
                       std::size_t& bytes_consumed) noexcept {
    bytes_consumed = 0;

    // Need at least preamble + 1 byte length + 1 byte type to begin.
    if (input.size() < 3) {
        return ParseStatus::kNeedMore;
    }

    if (input[0] != kPreamble) {
        return ParseStatus::kBadPreamble;
    }

    // Parse the length varuint immediately after the preamble.
    auto length_res = ReadVarUint32(input, 1);
    if (length_res.invalid) {
        return ParseStatus::kBadVarint;
    }
    if (!length_res.value.has_value()) {
        return ParseStatus::kNeedMore;
    }
    const std::uint32_t payload_len = *length_res.value;
    const std::size_t after_length = 1 + length_res.bytes_consumed;

    // Parse the type-id varuint.
    auto type_res = ReadVarUint32(input, after_length);
    if (type_res.invalid) {
        return ParseStatus::kBadVarint;
    }
    if (!type_res.value.has_value()) {
        return ParseStatus::kNeedMore;
    }
    const std::uint32_t msg_type_id = *type_res.value;
    const std::size_t after_type = after_length + type_res.bytes_consumed;

    // Need the full payload still in the buffer.
    const std::size_t end = after_type + payload_len;
    if (input.size() < end) {
        return ParseStatus::kNeedMore;
    }

    out.msg_type_id = msg_type_id;
    out.payload.assign(input.begin() + static_cast<std::ptrdiff_t>(after_type),
                       input.begin() + static_cast<std::ptrdiff_t>(end));
    bytes_consumed = end;
    return ParseStatus::kOk;
}

std::vector<std::uint8_t> EncodeFrame(std::uint32_t msg_type_id,
                                      std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> out;
    // Preamble + 2 varuints (worst case 5 bytes each) + payload.
    out.reserve(1 + kMaxVarintBytes + kMaxVarintBytes + payload.size());

    out.push_back(kPreamble);
    AppendVarUint32(out, static_cast<std::uint32_t>(payload.size()));
    AppendVarUint32(out, msg_type_id);
    out.insert(out.end(), payload.begin(), payload.end());

    return out;
}

std::vector<std::uint8_t> EncodeFrame(std::uint32_t msg_type_id,
                                      std::string_view payload) {
    return EncodeFrame(
        msg_type_id,
        std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()));
}

}  // namespace lva::proto
