#include "../include/Input.hpp"
#include <cstring>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/keysym.h>
static Display* g_x11Display = nullptr;

void SetInputX11Display(Display* d) { g_x11Display = d; }
#else
void SetInputX11Display(void*) {}
#endif

InputManager::InputManager() {
    std::memset(currentKeys, 0, sizeof(currentKeys));
    std::memset(lastKeys, 0, sizeof(lastKeys));
}

void InputManager::update() {
    std::memcpy(lastKeys, currentKeys, sizeof(currentKeys));
#ifdef _WIN32
    for (int i = 0; i < 256; i++) {
        currentKeys[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }
#elif __linux__
    if (g_x11Display) {
        char keys[32];
        XQueryKeymap(g_x11Display, keys);
        std::memset(currentKeys, 0, sizeof(currentKeys));
        for (int i = 0; i < 256; i++) {
            if ((keys[i / 8] >> (i % 8)) & 1) {
                KeySym sym = XKeycodeToKeysym(g_x11Display, (KeyCode)i, 0);
                if (sym < 256) currentKeys[sym & 0xFF] = true;
            }
        }
    }
#endif
}

bool InputManager::isDown(int vk) { return currentKeys[vk & 0xFF]; }
bool InputManager::isPressed(int vk) { return currentKeys[vk & 0xFF] && !lastKeys[vk & 0xFF]; }
bool InputManager::isReleased(int vk) { return !currentKeys[vk & 0xFF] && lastKeys[vk & 0xFF]; }

void InputManager::setKeyState(int key, bool pressed) {
    if (key >= 0 && key < 256) currentKeys[key] = pressed;
}
