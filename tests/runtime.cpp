#include <Arduino.h>
#include <BoardRuntime.h>
#include <MiningEmbed.h>
#include <SessionProvisioner.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

void setup();
void loop();
TestSerial Serial;
unsigned long clockMs = 0;
std::vector<std::string> events;
miningembed::TestState miningembed::testState;
const char* miningembed::statusName(Status) {
    return "synthetic-status";
}
bool wifiAvailable = true;
unsigned wifiAttempts = 0;
unsigned timeAttempts = 0;
namespace BoardRuntime {
void beginSerial() {
    events.push_back("serial");
}
void beginWifi(const char*, const char*) {
    events.push_back("wifi");
}
bool connectWifi(uint8_t, unsigned long) {
    events.push_back("connect");
    return ++wifiAttempts > 1;
}
bool ensureWifiConnected(uint8_t, unsigned long, unsigned long) {
    return wifiAvailable;
}
bool synchronizeTime(const char* primary, const char* fallback) {
    assert(std::string(primary) == "pool.ntp.org");
    assert(std::string(fallback) == "time.cloudflare.com");
    events.push_back("time");
    return ++timeAttempts > 1;
}
void cooperate() {}
void reportMemory(const char*) {}
} // namespace BoardRuntime
void SessionProvisioner::importApprovedSession(aplo::SessionKey&, const char*) {
    events.push_back("provision");
}

int main() {
    setup();
#ifndef APLO_BUILD_VALIDATION
    loop();
    assert(events == std::vector<std::string>{"serial"});
    assert(Serial.output.find("Configuration missing/invalid") != std::string::npos);
    assert(miningembed::testState.reads == 0);
    std::cout << "Empty device config: no network, TLS, provisioning or mining PASS.\n";
#else
    using namespace miningembed;
    auto& state = testState;
    assert(events == (std::vector<std::string>{"serial", "wifi", "connect", "connect", "web3",
#ifdef APLO_RPC_ROOT_CA
                                               "custom-ca",
#else
                                               "auto-ca",
#endif
                                               "time", "connect", "time", "provision"}));
    wifiAvailable = false;
    loop();
    assert(state.reads == 0 && state.submits == 0);
    wifiAvailable = true;
    for (Status status : {Status::RpcUnavailable, Status::SessionInvalid, Status::Expired,
                          Status::BudgetExceeded}) {
        state.readStatus = status;
        loop();
        assert(state.submits == 0);
    }
    state.readStatus = Status::Ok;
    state.findStatus = Status::NoCandidate;
    loop();
    assert(state.submits == 0);
    state.findStatus = Status::Ok;
    state.submitStatus = Status::BroadcastUncertain;
    loop();
    assert(state.submits == 1 && state.outstanding);
    const unsigned reads = state.reads;
    state.receiptStatus = Status::RpcUnavailable;
    loop();
    assert(state.outstanding && state.submits == 1 && state.reads == reads);
    state.receiptStatus = Status::Ok;
    for (SubmissionState receipt : {SubmissionState::Uncertain, SubmissionState::Pending}) {
        state.receiptState = receipt;
        loop();
        assert(state.outstanding && state.submits == 1 && state.reads == reads);
    }
    state.receiptState = SubmissionState::Confirmed;
    loop();
    assert(!state.outstanding && state.submits == 1);
    loop();
    assert(state.submits == 2);
    state.receiptState = SubmissionState::Reverted;
    loop();
    assert(!state.outstanding);
    std::cout << "Firmware orchestration: DHCP/TLS retry order, network pause, revoke/expiry, "
                 "uncertain send and receipt lock PASS (scripted boundary).\n";
#endif
}
