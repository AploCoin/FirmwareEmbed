#ifndef MININGEMBED_H
#define MININGEMBED_H
#include <MiningWork.h>
#include <uint256/uint256_t.h>
#include <SessionKeys.h>

namespace miningembed {
enum class Status {
    Ok,
    NoCandidate,
    InvalidArgument,
    RpcUnavailable,
    SessionInvalid,
    Expired,
    BudgetExceeded,
    InsufficientFunds,
    NotStaked,
    Cooldown,
    StaleChallenge,
    InvalidProof,
    RandomUnavailable,
    Busy,
    SignFailed,
    BroadcastUncertain
};
const char* statusName(Status status);
enum class SubmissionState { NotSent, Broadcast, Pending, Reverted, Confirmed, Uncertain };
struct Submission {
    SubmissionState state = SubmissionState::NotSent;
    std::string hash;
};
// Copy public authorization from an owner-approved, confirmed registration.
// Only GAplo / mine(bytes32), zero APLO, finite GAPLO are accepted.
struct MiningSession {
    std::string owner;
    std::string signer;
    uint256_t chainId = 0;
    uint256_t gaploBudget = 0;
    uint64_t expiry = 0;
};
struct TestState {
    Status readStatus = Status::Ok;
    Status findStatus = Status::Ok;
    Status submitStatus = Status::Ok;
    Status receiptStatus = Status::Ok;
    SubmissionState receiptState = SubmissionState::Pending;
    bool outstanding = false;
    unsigned reads = 0;
    unsigned submits = 0;
    unsigned receipts = 0;
};
extern TestState testState;
class MiningEmbed {
public:
    MiningEmbed(aplo::SessionKeys&, aplo::SessionKey&, const MiningSession&) {}
    Status readParams(MinerParams&) {
        ++testState.reads;
        return testState.readStatus;
    }
    Status findCandidate(const MinerParams&, uint32_t, Candidate&) {
        return testState.findStatus;
    }
    Status submit(const Candidate&, Submission& out) {
        ++testState.submits;
        testState.outstanding = true;
        out.hash = "synthetic-hash";
        out.state = SubmissionState::Uncertain;
        return testState.submitStatus;
    }
    Status receipt(Submission& out) {
        ++testState.receipts;
        if (testState.receiptStatus == Status::Ok) {
            out.state = testState.receiptState;
            testState.outstanding =
                out.state != SubmissionState::Confirmed && out.state != SubmissionState::Reverted;
        }
        return testState.receiptStatus;
    }
    bool unresolved() const {
        return testState.outstanding;
    }
    const aplo::SessionResult& rpcError() const {
        static aplo::SessionResult error;
        return error;
    }
};
} // namespace miningembed
#endif
