#include <Arduino.h>

#include "BoardRuntime.h"

namespace {
firmware::BoardRuntime board;
miningembed::provisioning::FirmwareRuntime app(
    firmware::flashStorage(),
    [](const firmware::Config& config) { board.begin(config); },
    [] { return board.poll(); },
    firmware::BoardRuntime::diagnostics
);
} // namespace

void setup() {
    Serial.setRxBufferSize(1024);
    Serial.begin(115200);
    app.begin();
}

void loop() {
    Stream& port = Serial;
    if (app.poll(port)) {
        port.flush();
        delay(100);
        ESP.restart();
        return;
    }
    delay(1);
}
