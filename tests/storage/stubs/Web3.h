#pragma once
#include <cstring>

// Only network/TLS boundaries are replaced. Production uses the actual X.509 parser.
class Web3 {
public:
    static bool isHttpsUrl(const char* s) {
        return s && !strncmp(s, "https://", 8) && strlen(s) > 8 && !strchr(s, ' ');
    }

    static bool isValidCertificate(const char* s) {
        return s && !strncmp(s, "-----BEGIN CERTIFICATE-----\n", 28) &&
               strstr(s, "-----END CERTIFICATE-----\n");
    }
};
