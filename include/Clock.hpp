#pragma once
#include <chrono>

class Clock {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;

public:
    Clock() {
        lastTime = std::chrono::high_resolution_clock::now();
    }

    float tick() {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> delta = now - lastTime;
        lastTime = now;
        return delta.count();
    }
};