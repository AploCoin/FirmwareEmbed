#pragma once

#include <MiningEmbed.h>
#include <Web3.h>

namespace DeviceSetup {
bool readSession(miningembed::MiningSession& session);
// Blocks cooperatively until DHCP and certificate time are available.
Web3& connect();
} // namespace DeviceSetup
