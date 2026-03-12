#pragma once
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace Protocol {

// Wrap a JSON object into a length-prefixed binary frame:
//   [4 bytes: body length, network byte order][JSON bytes]
std::vector<uint8_t> frame(const nlohmann::json& j);

// Try to extract one complete frame from a receive buffer.
// On success: fills `out`, removes consumed bytes from `buffer`, returns true.
// On failure (incomplete data): leaves buffer unchanged, returns false.
bool extractFrame(std::vector<uint8_t>& buffer, nlohmann::json& out);

} // namespace Protocol
