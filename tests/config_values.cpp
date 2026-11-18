#include <Arduino.h>
#include <ConfigValues.h>
#include <assert.h>
#include <iostream>

TestSerial Serial;
unsigned yields = 0;
unsigned delays = 0;
int main() {
    uint256_t out;
    assert(FirmwareConfig::parsePositiveDecimal("1337", out) && out == uint256_t(1337));
    assert(FirmwareConfig::parsePositiveDecimal("424242", out) && out == uint256_t(424242));
    assert(FirmwareConfig::parsePositiveDecimal("1000000000000000000", out) &&
           out == uint256_t(UINT64_C(1000000000000000000)));
    const char* max =
        "115792089237316195423570985008687907853269984665640564039457584007913129639935";
    assert(FirmwareConfig::parsePositiveDecimal(max, out) && out == ~uint256_t(0));
    for (const char* invalid :
         {"", "0", "01", "-1", "1.0", "0x10", "10a",
          "115792089237316195423570985008687907853269984665640564039457584007913129639936"}) {
        out = 9;
        assert(!FirmwareConfig::parsePositiveDecimal(invalid, out) && out == uint256_t(0));
    }
    assert(!FirmwareConfig::parsePositiveDecimal(nullptr, out));
    FirmwareConfig::PublicConfig config = {"test-network",
                                           "https://rpc.example:443/api",
                                           "",
                                           "0x1111111111111111111111111111111111111111",
                                           "0x2222222222222222222222222222222222222222",
                                           "1337",
                                           "1000",
                                           42};
    uint256_t chain, budget;
    auto valid = [&]() { return FirmwareConfig::parsePublicConfig(config, chain, budget); };
    assert(valid() && chain == uint256_t(1337) && budget == uint256_t(1000));
    for (const char* url : {"", "http://rpc.example", "https://", "https://host:0",
                            "https://host:65536", "https://user@host", "https://host?query",
                            "https://host/path\r\nInjected: header", "https://host/#fragment"}) {
        config.rpcPrimary = url;
        assert(!valid() && chain == uint256_t(0) && budget == uint256_t(0));
    }
    config.rpcPrimary = "https://rpc.example";
    config.rpcFallback = "http://fallback.example";
    assert(!valid());
    config.rpcFallback = "https://fallback.example/path?api=value";
    assert(valid());
    config.ssid = "";
    assert(!valid());
    config.ssid = "test-network";
    config.owner = "0x0000000000000000000000000000000000000000";
    assert(!valid());
    config.owner = "0x1111111111111111111111111111111111111111";
    config.signer = "bad";
    assert(!valid());
    config.signer = "0x2222222222222222222222222222222222222222";
    config.owner = "1111111111111111111111111111111111111111";
    assert(!valid());
    config.owner = "0x1111111111111111111111111111111111111111";
    config.signer = "2222222222222222222222222222222222222222";
    assert(!valid());
    config.signer = "0x2222222222222222222222222222222222222222";
    config.owner = "0xabcdefabcdefabcdefabcdefabcdefabcdefabcd";
    config.signer = "0xABCDEFABCDEFABCDEFABCDEFABCDEFABCDEFABCD";
    assert(!valid());
    config.owner = "0x1111111111111111111111111111111111111111";
    config.signer = "0x2222222222222222222222222222222222222222";
    config.gaploBudget = max;
    assert(!valid() && chain == uint256_t(0) && budget == uint256_t(0));
    config.gaploBudget = "1000";
    config.expiry = 0;
    assert(!valid());
    config.expiry = 42;
    config.gaploBudget = "01";
    assert(!valid() && chain == uint256_t(0) && budget == uint256_t(0));
    std::cout
        << "Decimal/public config: exact integers, incomplete policy and unsafe RPC URLs PASS.\n";
}
