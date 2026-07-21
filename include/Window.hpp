#pragma once
#include <string>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <unistd.h>
#include <sys/mman.h>

#if __has_include(<wayland-client.h>) && __has_include(<wayland-egl.h>) && __has_include(<EGL/egl.h>)
#define JWAROL_HAS_WAYLAND 1
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#if __has_include(<xkbcommon/xkbcommon.h>)
#include <xkbcommon/xkbcommon.h>
#define JWAROL_HAS_XKBCOMMON 1
#endif
#endif

#endif

class InputManager;

class AuraWindow {
public:
    AuraWindow(int w, int h, const std::string& title, bool d3d11 = false);
    ~AuraWindow();
    void pollEvents();
    void swapBuffers();
    void clear(float r, float g, float b);
    bool isOpen() const { return running; }
    bool isD3D11() const { return d3d11Mode; }
    void setInputManager(InputManager* im) { inputMgr = im; }

#ifdef _WIN32
    HDC getDC() const { return hdc; }
    HWND getHWND() const { return hwnd; }
#elif __linux__
    Display* getDisplay() const { return x11Display; }
    Window getX11Window() const { return x11Window; }
    bool isWayland() const { return useWayland; }
#if JWAROL_HAS_WAYLAND
    wl_display* getWlDisplay() const { return wlDisplay; }
    wl_surface* getWlSurface() const { return wlSurface; }
#endif
#endif

    bool running = false;
    InputManager* inputMgr = nullptr;

#if JWAROL_HAS_WAYLAND
    wl_compositor* wlCompositor = nullptr;
    wl_shell* wlShell = nullptr;
    wl_seat* wlSeat = nullptr;
    wl_keyboard* wlKeyboard = nullptr;
    wl_egl_window* wlEglWindow = nullptr;
    struct xkb_context* xkbContext = nullptr;
    struct xkb_keymap* xkbKeymap = nullptr;
    struct xkb_state* xkbState = nullptr;
    int waylandWidth = 0;
    int waylandHeight = 0;

    static const wl_registry_listener registryListener;
    static const wl_shell_surface_listener shellSurfaceListener;
    static const wl_seat_listener seatListener;
    static const wl_keyboard_listener keyboardListener;
#endif

private:
    bool d3d11Mode = false;

#ifdef _WIN32
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hrc = nullptr;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void initWin32(int w, int h, const std::string& title);
    void destroyWin32();

#elif __linux__
    bool useWayland = false;

    Display* x11Display = nullptr;
    Window x11Window = 0;
    GLXContext glxContext = nullptr;
    int x11Screen = 0;

    bool initX11(int w, int h, const std::string& title);
    void pollEventsX11();
    void destroyX11();
    void swapBuffersX11();

#if JWAROL_HAS_WAYLAND
    wl_display* wlDisplay = nullptr;
    wl_registry* wlRegistry = nullptr;
    wl_surface* wlSurface = nullptr;
    wl_shell_surface* wlShellSurface = nullptr;
    EGLDisplay eglDisplay = nullptr;
    EGLSurface eglSurface = nullptr;
    EGLContext eglContext = nullptr;

    bool initWayland(int w, int h, const std::string& title);
    void pollEventsWayland();
    void destroyWayland();
    void swapBuffersWayland();
#endif
#endif
};
