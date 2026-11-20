#pragma once
#include <MiningEmbed.h>

// A durable pre-broadcast latch, not a transaction journal. Any residue at boot
// prohibits importing a key or constructing a signer. External reconciliation
// is required; the firmware never clears a latch found during boot.
class SigningLatch {
public:
    bool begin();
    static bool writePending(const miningembed::PendingTransaction*, void*);
private:
    bool read(uint32_t offset, uint32_t* words, size_t count);
    bool erased();
    bool mark(const miningembed::PendingTransaction&);
    bool clear();
    bool ready_ = false;
    bool active_ = false;
};
