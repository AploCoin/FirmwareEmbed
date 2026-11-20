#include <Arduino.h>

#include "BoardRuntime.h"

// Pending reconciliation and TLS share the Arduino loop task.
SET_LOOP_TASK_STACK_SIZE(12 * 1024)

namespace {
firmware::BoardRuntime board;
miningembed::provisioning::FirmwareRuntime app(
    firmware::flashStorage(),
    [](const firmware::Config& config) { board.begin(config); },
    [] { return board.poll(); },
    firmware::BoardRuntime::diagnostics
);

Stream& consolePort() {
    if (HWCDC::isPlugged()) {
        return USBSerial;
    }
    return Serial;
}
} // namespace

void setup() {
    Serial.setRxBufferSize(1024);
    Serial.begin(115200);
    USBSerial.setRxBufferSize(1024);
    USBSerial.begin(115200);
    app.begin();
}

void loop() {
    Stream& port = consolePort();
    if (app.poll(port)) {
        port.flush();
        delay(100);
        ESP.restart();
        return;
    }
    delay(1);
}
