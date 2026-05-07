#pragma once
#ifdef _WIN32
#include <windows.h>
#endif

class InputManager {
private:
    bool currentKeys[256];
    bool lastKeys[256];
public:
    InputManager();
    void update();
    bool isDown(int vk);
    bool isPressed(int vk);
    bool isReleased(int vk);
};
