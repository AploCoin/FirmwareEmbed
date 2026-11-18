#pragma once
#include <Arduino.h>
#include <uint256/uint256_t.h>
#include <vector>
extern std::vector<std::string> events;
class Web3 {
public:
    Web3(const char*, const char*) {
        events.push_back("web3");
    }
    void setCertificate(const char*) {
        events.push_back("custom-ca");
    }
    void setAutoCertificate() {
        events.push_back("auto-ca");
    }
    std::string AploGetAploBalanceString(const std::string*) {
        return "0";
    }
    std::string AploGetGasBalanceString(const std::string*) {
        return "0";
    }
    uint256_t AploGetStake(const std::string*, const std::string*) {
        return uint256_t(0);
    }
    uint256_t AploGetStakeMultiplier(const std::string*, const std::string*) {
        return uint256_t(0);
    }
};
