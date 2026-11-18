#include "MiningApplication.h"
#include "BoardRuntime.h"
#include "FirmwareConfig.h"
#include <MiningEmbed.h>
#include <AploContracts.h>
#include <Util.h>

namespace {
miningembed::MiningEmbed* miner = nullptr;
Web3* client = nullptr;
miningembed::Submission submission;
unsigned long lastReceiptPoll = 0;
uint32_t receiptPolls = 0;

void printStatus(const char* stage, miningembed::Status status) {
    Serial.print(stage);
    Serial.print(F(": "));
    Serial.println(miningembed::statusName(status));
    if (status == miningembed::Status::RpcUnavailable ||
        status == miningembed::Status::SignFailed ||
        status == miningembed::Status::BroadcastUncertain) {
        const aplo::SessionResult& error = miner->rpcError();
        Serial.print(F("RPC/signing diagnostic: "));
        Serial.print(static_cast<int>(error.code));
        Serial.print(F(" / "));
        Serial.println(error.message.c_str());
    }
}

void ownerDiagnostics() {
    const std::string owner = FirmwareConfig::ownerAddress;
    const std::string staking = APLO_STAKING_CONTRACT;
    Serial.print(F("Owner APLO balance: "));
    Serial.println(client->AploGetAploBalanceString(&owner).c_str());
    Serial.print(F("Owner GAPLO gas balance: "));
    Serial.println(client->AploGetGasBalanceString(&owner).c_str());
    const uint256_t stake = client->AploGetStake(&staking, &owner);
    Serial.print(F("Owner staked amount (base units): "));
    Serial.println(stake.str(10).c_str());
    const uint256_t multiplier = client->AploGetStakeMultiplier(&staking, &owner);
    Serial.print(F("Owner multiplier (scaled by 10): "));
    Serial.println(multiplier.str(10).c_str());
    Serial.println(F("Balance/stake display is diagnostic; checked library RPC gates submission."));
}

void pollReceipt() {
    const unsigned long now = millis();
    if (lastReceiptPoll && now - lastReceiptPoll < FirmwareConfig::receiptPollDelayMs) {
        return;
    }
    lastReceiptPoll = now;
    const auto status = miner->receipt(submission);
    BoardRuntime::cooperate();
    if (status != miningembed::Status::Ok) {
        printStatus("Receipt lookup failed; retaining unresolved submission", status);
        return;
    }
    using State = miningembed::SubmissionState;
    switch (submission.state) {
    case State::Confirmed:
        Serial.println(F("Receipt status=1: mining transaction confirmed."));
        BoardRuntime::reportMemory("after confirmed receipt");
        receiptPolls = 0;
        ownerDiagnostics();
        break;
    case State::Reverted:
        Serial.println(F("Receipt status=0: mining transaction reverted on-chain."));
        receiptPolls = 0;
        break;
    case State::Uncertain:
        Serial.println(F("Broadcast outcome uncertain; receipt absent. The request may not have "
                         "reached RPC; no resend."));
        break;
    case State::Pending:
    case State::Broadcast:
        Serial.println(F("Receipt pending; no additional submission will be made."));
        ++receiptPolls;
        if (receiptPolls % FirmwareConfig::receiptPollAttempts == 0) {
            Serial.println(F("Receipt polling window elapsed; still unresolved, continuing to "
                             "track the same hash."));
        }
        break;
    case State::NotSent:
        Serial.println(F("No outstanding receipt."));
        break;
    }
}
} // namespace

void MiningApplication::begin(Web3& web3, aplo::SessionKey& key,
                              const miningembed::MiningSession& session) {
    // One-shot Arduino setup. Both borrowed inputs outlive these static adapters.
    static aplo::SessionKeys rpc(web3);
    static miningembed::MiningEmbed instance(rpc, key, session);
    client = &web3;
    miner = &instance;
    Serial.println(F("Restricted mining initialized; live authorization is checked before each "
                     "batch/submission."));
    ownerDiagnostics();
}

void MiningApplication::tick() {
    if (!miner) {
        return;
    }
    if (miner->unresolved()) {
        pollReceipt();
        return;
    }
    miningembed::MinerParams params;
    auto status = miner->readParams(params);
    if (status != miningembed::Status::Ok) {
        printStatus("Mining paused", status);
        return;
    }
    Serial.print(F("Miner last block: "));
    Serial.println(uint256_t(params.lastBlock).str(10).c_str());
    Serial.print(F("Difficulty target: "));
    Serial.println(Util::ConvertBytesToHex(params.target, sizeof(params.target)).c_str());
    BoardRuntime::reportMemory("before mining batch");
    miningembed::Candidate candidate;
    status = miner->findCandidate(params, FirmwareConfig::hashesPerCycle, candidate);
    BoardRuntime::reportMemory("after mining batch");
    if (status != miningembed::Status::Ok) {
        printStatus("Mining batch", status);
        return;
    }
    Serial.print(F("Candidate nonce: "));
    Serial.println(Util::ConvertBytesToHex(candidate.nonce, sizeof(candidate.nonce)).c_str());
    Serial.print(F("Candidate hash: "));
    Serial.println(Util::ConvertBytesToHex(candidate.hash, sizeof(candidate.hash)).c_str());
    submission = miningembed::Submission();
    status = miner->submit(candidate, submission);
    BoardRuntime::reportMemory("after submission / signing");
    if (status != miningembed::Status::Ok) {
        printStatus("Submission", status);
    }
    if (!submission.hash.empty()) {
        Serial.print(F("Transaction hash: "));
        Serial.println(submission.hash.c_str());
        Serial.println(F("Broadcast does not imply execution success; tracking receipt."));
        receiptPolls = 0;
        lastReceiptPoll = 0;
    }
}
