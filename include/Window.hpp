#pragma once
#include <string>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#endif

class AuraWindow {
public:
    AuraWindow(int w, int h, const std::string& title);
    ~AuraWindow();
    void pollEvents();
    void swapBuffers();
    void clear(float r, float g, float b);
    bool isOpen() const { return running; }

#ifdef _WIN32
    HDC getDC() const { return hdc; }
    HWND getHWND() const { return hwnd; }
#elif __linux__
    Display* getDisplay() const { return display; }
    Window getWindow() const { return window; }
#endif

private:
#ifdef _WIN32
    HWND hwnd;
    HDC hdc;
    HGLRC hrc;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#elif __linux__
    Display* display;
    Window window;
    GLXContext context;
    int screen;
#endif
    bool running;
};
