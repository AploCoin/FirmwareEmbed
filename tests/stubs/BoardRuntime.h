#pragma once
#include <stddef.h>
#include <string>
#define F(x) x
struct TestSerial {
    std::string input;
    std::string output;
    size_t position = 0;
    bool waitOnce = false;
    int available() {
        if (waitOnce) {
            waitOnce = false;
            return 0;
        }
        return position < input.size();
    }
    int read() {
        const int next = static_cast<unsigned char>(input[position]);
        ++position;
        return next;
    }
    void println(const char* text) {
        output += text;
        output += '\n';
    }
};
extern TestSerial Serial;
extern unsigned yields;
extern unsigned delays;
inline void delay(unsigned) {
    ++delays;
}
namespace BoardRuntime {
inline void cooperate() {
    ++yields;
}
} // namespace BoardRuntime
