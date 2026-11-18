#pragma once

namespace aplo {
class SessionKey;
}

namespace SessionProvisioner {
using ImportKey = bool (*)(const char* hex, void* context);
// Serial line is never echoed, never printed, never persisted by firmware.
// Caller imports synchronously; the temporary character buffer is then zeroed.
bool importFromSerial(ImportKey importKey, void* context);
void importApprovedSession(aplo::SessionKey& key, const char* address);
} // namespace SessionProvisioner
