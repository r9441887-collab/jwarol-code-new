#include "../include/Input.hpp"
#include <cstring>

InputManager::InputManager() {
    std::memset(currentKeys, 0, 256);
    std::memset(lastKeys, 0, 256);
}

void InputManager::update() {
    std::memcpy(lastKeys, currentKeys, 256);
#ifdef _WIN32
    for (int i = 0; i < 256; i++) {
        currentKeys[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }
#else
    // Linux: будет обновляться через X11 события
#endif
}

bool InputManager::isDown(int vk) { return currentKeys[vk]; }
bool InputManager::isPressed(int vk) { return currentKeys[vk] && !lastKeys[vk]; }
bool InputManager::isReleased(int vk) { return !currentKeys[vk] && lastKeys[vk]; }
