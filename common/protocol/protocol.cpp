#include "protocol.h"
#include <arpa/inet.h>  // htonl, ntohl
#include <cstring>

namespace Protocol {

std::vector<uint8_t> frame(const nlohmann::json& j) {
    std::string body = j.dump();
    uint32_t len = htonl(static_cast<uint32_t>(body.size()));

    std::vector<uint8_t> result(4 + body.size());
    std::memcpy(result.data(), &len, 4);
    std::memcpy(result.data() + 4, body.data(), body.size());
    return result;
}

bool extractFrame(std::vector<uint8_t>& buffer, nlohmann::json& out) {
    if (buffer.size() < 4) return false;

    uint32_t netLen;
    std::memcpy(&netLen, buffer.data(), 4);
    uint32_t bodyLen = ntohl(netLen);

    if (bodyLen > 4 * 1024 * 1024) return false;  // sanity limit: 4 MB (screen frames)
    if (buffer.size() < 4 + bodyLen) return false;

    std::string body(buffer.begin() + 4, buffer.begin() + 4 + bodyLen);
    try {
        out = nlohmann::json::parse(body);
    } catch (const nlohmann::json::parse_error&) {
        // Discard malformed frame and continue
        buffer.erase(buffer.begin(), buffer.begin() + 4 + bodyLen);
        return false;
    }

    buffer.erase(buffer.begin(), buffer.begin() + 4 + bodyLen);
    return true;
}

} // namespace Protocol
