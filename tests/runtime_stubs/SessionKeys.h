#pragma once
#include <string>
#include <stdint.h>
namespace aplo {
struct SessionResult {
    int code = 1;
    std::string message = "synthetic failure";
};
class SessionKey {
public:
    unsigned imports = 0;
    unsigned clears = 0;
    uint8_t firstByte = 0;
    bool Import(const uint8_t* secret) {
        ++imports;
        firstByte = secret[0];
        return firstByte != 0;
    }
    void Clear() {
        ++clears;
        firstByte = 0;
    }
    std::string Address() const {
        return firstByte == 0xbb ? "0x1111111111111111111111111111111111111111"
                                 : "0x2222222222222222222222222222222222222222";
    }
};

class SessionKeys {
public:
    template <class Client> explicit SessionKeys(Client&) {}
};
} // namespace aplo
