#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace redis_dc {

inline std::string encodeLengthPrefixedFrame(std::string_view payload) {
    if (payload.size() > 0xFFFFFFFFu) {
        throw std::length_error("socket payload is too large");
    }
    const auto length = static_cast<std::uint32_t>(payload.size());
    std::string frame;
    frame.resize(4);
    frame[0] = static_cast<char>((length >> 24U) & 0xFFU);
    frame[1] = static_cast<char>((length >> 16U) & 0xFFU);
    frame[2] = static_cast<char>((length >> 8U) & 0xFFU);
    frame[3] = static_cast<char>(length & 0xFFU);
    frame.append(payload.data(), payload.size());
    return frame;
}

inline std::uint32_t decodeLengthPrefix(const unsigned char bytes[4]) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
           (static_cast<std::uint32_t>(bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[2]) << 8U) |
           static_cast<std::uint32_t>(bytes[3]);
}

}  // namespace redis_dc
