#pragma once
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
void SetInputX11Display(Display* d);
#else
void SetInputX11Display(void*);
#endif

class InputManager {
public:
    bool currentKeys[256];
    bool lastKeys[256];

    InputManager();
    void update();
    bool isDown(int vk);
    bool isPressed(int vk);
    bool isReleased(int vk);
    void setKeyState(int key, bool pressed);
};
