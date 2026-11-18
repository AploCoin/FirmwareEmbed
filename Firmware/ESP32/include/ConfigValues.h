#pragma once

#include <uint256/uint256_t.h>

namespace FirmwareConfig {
struct PublicConfig {
    const char* ssid;
    const char* rpcPrimary;
    const char* rpcFallback;
    const char* owner;
    const char* signer;
    const char* chainId;
    const char* gaploBudget;
    uint64_t expiry;
};

// Canonical positive decimal configuration; uint256_t(string) parses HEX.
bool parsePositiveDecimal(const char* text, uint256_t& out);
// No partially parsed authorization is returned on failure.
bool parsePublicConfig(const PublicConfig& config, uint256_t& chainId, uint256_t& gaploBudget);
} // namespace FirmwareConfig
