#pragma once
#include <Arduino.h>
#include <stdint.h>

// Copy this file to DeviceConfig.private.h, replace values locally, and build.
// Private headers and personal binaries contain secrets: never publish them.
// Both modes are retained in the public image by volatile runtime gates.
namespace device {
enum class Mode { Session, Direct };
inline const volatile bool kConfigured = false;
inline const volatile Mode kMode = Mode::Session;
inline const char kSsid[] = "configure-me";
inline const char kPassword[] = "";
inline const char kPrimaryUrl[] = ""; // HTTPS DNS hostname; no implicit fallback
inline const char kFallbackUrl[] = "";
inline const char kPrimaryCa[] PROGMEM = ""; // CA PEM, maximum 4096 bytes
inline const char kFallbackCa[] PROGMEM = ""; // required separately if fallback URL is set
inline const uint8_t kPrivateKey[32] = {}; // Session signer OR Direct owner, never both
inline const char kChainId[] = ""; // positive canonical decimal
inline const char kOwner[] = ""; // Session only
inline const char kSigner[] = ""; // Session only
inline const char kGaploBudget[] = ""; // Session only, canonical decimal base units
inline constexpr uint64_t kExpiry = 0; // Session only, owner-approved absolute block height
inline const char kNtpPrimary[] = "pool.ntp.org";
inline const char kNtpFallback[] = "time.cloudflare.com";
inline constexpr uint32_t kHashesPerStep = 32;
inline constexpr uint32_t kRetryMs = 5000;
} // namespace device
