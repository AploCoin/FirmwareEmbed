#include "ConfigValues.h"
#include <cstring>

namespace {
bool validAddress(const char* text) {
    if (!text || text[0] != '0' || text[1] != 'x') {
        return false;
    }
    text += 2;
    bool nonzero = false;
    size_t length = 0;
    for (; *text; ++text) {
        ++length;
        if (length > 40 || !((*text >= '0' && *text <= '9') || (*text >= 'a' && *text <= 'f') ||
                             (*text >= 'A' && *text <= 'F'))) {
            return false;
        }
        nonzero = nonzero || *text != '0';
    }
    return length == 40 && nonzero;
}

bool sameAddress(const char* left, const char* right) {
    // Called only after both exact-length hex addresses have been validated.
    for (size_t i = 2; i < 42; ++i) {
        const char a = left[i] >= 'A' && left[i] <= 'F' ? left[i] + ('a' - 'A') : left[i];
        const char b = right[i] >= 'A' && right[i] <= 'F' ? right[i] + ('a' - 'A') : right[i];
        if (a != b) {
            return false;
        }
    }
    return true;
}

bool validHttpsEndpoint(const char* url) {
    if (!url || std::strncmp(url, "https://", 8) != 0) {
        return false;
    }
    const char* host = url + 8;
    const char* cursor = host;
    while ((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= 'A' && *cursor <= 'Z') ||
           (*cursor >= '0' && *cursor <= '9') || *cursor == '-' || *cursor == '.') {
        ++cursor;
    }
    if (cursor == host) {
        return false;
    }
    if (*cursor == ':') {
        ++cursor;
        unsigned port = 0;
        const char* digits = cursor;
        while (*cursor >= '0' && *cursor <= '9') {
            const unsigned digit = static_cast<unsigned>(*cursor - '0');
            port = port * 10 + digit;
            ++cursor;
            if (port > 65535) {
                return false;
            }
        }
        if (cursor == digits || port == 0) {
            return false;
        }
    }
    if (*cursor != '\0' && *cursor != '/') {
        return false;
    }
    for (; *cursor; ++cursor) {
        if (static_cast<unsigned char>(*cursor) <= 32 || *cursor == '#' || *cursor == 127) {
            return false;
        }
    }
    return true;
}
} // namespace

bool FirmwareConfig::parsePositiveDecimal(const char* text, uint256_t& out) {
    out = 0;
    if (!text || *text < '1' || *text > '9') {
        return false;
    }
    const uint256_t maximum = ~uint256_t(0);
    const uint256_t ten(10);
    uint256_t value(0);
    size_t digits = 0;
    for (const char* cursor = text; *cursor; ++cursor) {
        ++digits;
        if (digits > 78 || *cursor < '0' || *cursor > '9') {
            return false;
        }
        const uint256_t digit(static_cast<unsigned>(*cursor - '0'));
        if (value > (maximum - digit) / ten) {
            return false;
        }
        value = value * ten + digit;
    }
    out = value;
    return true;
}

bool FirmwareConfig::parsePublicConfig(const PublicConfig& config, uint256_t& chainId,
                                       uint256_t& gaploBudget) {
    chainId = 0;
    gaploBudget = 0;
    if (!config.ssid || !*config.ssid || !validHttpsEndpoint(config.rpcPrimary) ||
        (config.rpcFallback && *config.rpcFallback && !validHttpsEndpoint(config.rpcFallback)) ||
        !validAddress(config.owner) || !validAddress(config.signer) ||
        sameAddress(config.owner, config.signer) || !config.expiry) {
        return false;
    }
    if (!parsePositiveDecimal(config.chainId, chainId) ||
        !parsePositiveDecimal(config.gaploBudget, gaploBudget) || gaploBudget == ~uint256_t(0)) {
        chainId = 0;
        gaploBudget = 0;
        return false;
    }
    return true;
}
