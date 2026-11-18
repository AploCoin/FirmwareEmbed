#pragma once

// Optional unprivileged LAN SNTP adapter. No public time server is contacted.
// Configure APLO_LOCAL_NTP_HOST and APLO_LOCAL_NTP_PORT in ignored DeviceConfig.h.
bool synchronizeLocalTime();
