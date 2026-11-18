#pragma once
#include <Web3.h>
#include <SessionKeys.h>
#include <MiningEmbed.h>

namespace MiningApplication {
void begin(Web3& web3, aplo::SessionKey& key, const miningembed::MiningSession& session);
void tick();
} // namespace MiningApplication
