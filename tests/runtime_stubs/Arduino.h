#pragma once
#include <string>
#include <stdint.h>
#define F(text) text
struct TestSerial {
    std::string output;
    void print(const char* text) {
        output += text;
    }
    void println(const char* text) {
        output += text;
        output += '\n';
    }
    void print(int) {}
};
extern TestSerial Serial;
extern unsigned long clockMs;
inline unsigned long millis() {
    return clockMs;
}
inline void delay(unsigned long duration) {
    clockMs += duration;
}
