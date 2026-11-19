#include <Arduino.h>
#include <Web3.h>
#include <new>

#include "BoardRuntime.h"
#include "DeviceConfig.h"
#include "SerialConfig.h"

namespace {
firmware::DeviceConfig store;
firmware::SerialConfig serialConfig(store);
firmware::BoardRuntime network;
aplo::SessionKey key;

Web3* web3 = nullptr;
miningembed::MiningEmbed* miner = nullptr;
const char* state = "unconfigured";
const char* lastMiningState = "mining";
bool trustPaused = false;
bool miningBlocked = false;
uint32_t lastStepAt = 0;

void setTrust(const firmware::Config& config) {
    web3->setCertificate(
        config.primaryCa.c_str(),
        config.fallbackCa.empty() ? nullptr : config.fallbackCa.c_str()
    );
}

bool startMiner(const firmware::Config& config) {
    if (!key.Import(config.key)) {
        return false;
    }

    // On ESP8266 the constructor reserves the BearSSL secondary stack.
    // Wait until Wi-Fi has associated and DHCP completed.
    static Web3 transport(config.primaryUrl.c_str(), config.fallbackUrl.c_str());
    static aplo::SessionKeys rpc(transport);
    web3 = &transport;
    setTrust(config);

    if (config.mode == firmware::Mode::Direct) {
        uint256_t chainId;
        if (!miningembed::parseMiningChainId(config.chainId.c_str(), chainId)) {
            return false;
        }
        miner = new (
            std::nothrow
        ) miningembed::MiningEmbed(rpc, key, chainId, firmware::DeviceConfig::writePending, &store);
    } else {
        miningembed::MiningSession session;
        if (!miningembed::parseMiningSession(
                config.owner.c_str(),
                config.signer.c_str(),
                config.chainId.c_str(),
                config.gaploBudget.c_str(),
                config.expiry,
                session
            )) {
            return false;
        }
        miner = new (
            std::nothrow
        ) miningembed::MiningEmbed(rpc, key, session, firmware::DeviceConfig::writePending, &store);
    }
    const auto* pending = store.pending();
    return miner && (!pending || miner->restore(*pending));
}

const char* miningState(miningembed::Status status, bool pending) {
    using miningembed::Status;
    switch (status) {
    case Status::JournalFailed:
        return "journal_error";
    case Status::RecoveryRequired:
        return "recovery_required";
    case Status::SignFailed:
        return "sign_failed";
    case Status::RandomUnavailable:
        return "random_unavailable";
    case Status::RpcUnavailable:
        return "rpc_unavailable";
    case Status::SessionInvalid:
        return "session_invalid";
    case Status::Expired:
        return "session_expired";
    case Status::BudgetExceeded:
        return "budget_exceeded";
    case Status::InsufficientFunds:
        return "insufficient_funds";
    case Status::NotStaked:
        return "not_staked";
    case Status::Cooldown:
        return "cooldown";
    default:
        return pending ? "pending" : "mining";
    }
}

void pauseTrust(bool pause) {
    if (pause == trustPaused) {
        return;
    }
    trustPaused = pause;
    if (web3) {
        if (pause) {
            web3->setCertificate(nullptr, nullptr);
        } else {
            setTrust(store.config());
        }
    }
}

const char* runDevice() {
    if (store.locked()) {
        return "storage_locked";
    }
    if (serialConfig.provisioningActive()) {
        pauseTrust(true);
        return "provisioning";
    }
    pauseTrust(false);
    if (!store.ready()) {
        return store.enrolled() ? "paused" : "unconfigured";
    }
    const firmware::Config& config = store.config();

    switch (network.poll()) {
    case firmware::NetworkState::Wifi:
        return "wifi";
    case firmware::NetworkState::Clock:
        return "clock";
    case firmware::NetworkState::Ready:
        break;
    }
    if (miningBlocked) {
        return "journal_error";
    }
    if (!miner && !startMiner(config)) {
        // A mismatched or corrupt record must never fall through to new work.
        key.Clear();
        miningBlocked = true;
        return "journal_error";
    }

    const uint32_t now = millis();
    if (now - lastStepAt >= config.intervalMs) {
        miningembed::Submission submission;
        const auto status = miner->step(config.hashesPerStep, submission);
        lastStepAt = millis();
        lastMiningState = miningState(status, miner->unresolved());
    }
    return lastMiningState;
}
} // namespace

void setup() {
    Serial.setRxBufferSize(1024);
    Serial.begin(115200);
    if (store.begin() && store.ready()) {
        const firmware::Config& config = store.config();
        network.begin(
            config.ssid.c_str(),
            config.password.c_str(),
            config.ntpPrimary.c_str(),
            config.ntpFallback.c_str()
        );
    }
}

void loop() {
    serialConfig.poll(Serial, state);
    if (serialConfig.restartRequested()) {
        Serial.flush();
        delay(100);
        ESP.restart();
        return;
    }
    state = runDevice();
    delay(1);
}
