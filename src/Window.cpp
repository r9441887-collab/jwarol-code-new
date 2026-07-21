#include "../include/Window.hpp"
#include "../include/Input.hpp"
#include <GL/gl.h>
#include <cstdlib>
#include <cstring>

#ifdef __linux__
#include <GL/glx.h>
typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARBPROC)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
#endif

#ifdef _WIN32

#pragma comment(lib, "opengl32.lib")

LRESULT CALLBACK AuraWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    AuraWindow* self = reinterpret_cast<AuraWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self && self->inputMgr) {
        if (uMsg == WM_KEYDOWN) self->inputMgr->setKeyState((int)wParam, true);
        if (uMsg == WM_KEYUP) self->inputMgr->setKeyState((int)wParam, false);
    }
    if (uMsg == WM_CLOSE || uMsg == WM_DESTROY) {
        if (self) self->running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void AuraWindow::initWin32(int w, int h, const std::string& title) {
    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "AuraWin";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    RECT r = {0, 0, w, h};
    if (!d3d11Mode) {
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    }

    hwnd = CreateWindowA("AuraWin", title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, hInst, NULL);

    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    if (!d3d11Mode) {
        hdc = GetDC(hwnd);
        PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1 };
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.cColorBits = 32;
        pfd.iPixelType = PFD_TYPE_RGBA;
        SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
        hrc = wglCreateContext(hdc);
        wglMakeCurrent(hdc, hrc);
    }
    running = true;
}

void AuraWindow::pollEvents() {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) running = false;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void AuraWindow::clear(float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void AuraWindow::swapBuffers() {
    if (!d3d11Mode && hdc) SwapBuffers(hdc);
}

void AuraWindow::destroyWin32() {
    if (!d3d11Mode) {
        wglMakeCurrent(NULL, NULL);
        if (hrc) wglDeleteContext(hrc);
        if (hdc) ReleaseDC(hwnd, hdc);
    }
    if (hwnd) DestroyWindow(hwnd);
    hwnd = nullptr; hdc = nullptr; hrc = nullptr;
}

AuraWindow::AuraWindow(int w, int h, const std::string& title, bool d3d11)
    : running(false), d3d11Mode(d3d11) {
    initWin32(w, h, title);
}

AuraWindow::~AuraWindow() {
    destroyWin32();
}

#elif __linux__

AuraWindow::AuraWindow(int w, int h, const std::string& title, bool d3d11)
    : running(false), d3d11Mode(d3d11) {
    const char* env = getenv("WAYLAND_DISPLAY");
    if (env && env[0] != '\0') {
#if JWAROL_HAS_WAYLAND
        useWayland = initWayland(w, h, title);
        if (useWayland) return;
#endif
    }
    useWayland = false;
    initX11(w, h, title);
}

AuraWindow::~AuraWindow() {
#if JWAROL_HAS_WAYLAND
    if (useWayland) { destroyWayland(); return; }
#endif
    destroyX11();
}

void AuraWindow::swapBuffers() {
#if JWAROL_HAS_WAYLAND
    if (useWayland) { swapBuffersWayland(); return; }
#endif
    swapBuffersX11();
}

void AuraWindow::pollEvents() {
#if JWAROL_HAS_WAYLAND
    if (useWayland) { pollEventsWayland(); return; }
#endif
    pollEventsX11();
}

void AuraWindow::clear(float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// ========================== X11 ==========================

bool AuraWindow::initX11(int w, int h, const std::string& title) {
    x11Display = XOpenDisplay(NULL);
    if (!x11Display) return false;
    SetInputX11Display(x11Display);

    x11Screen = DefaultScreen(x11Display);
    Window root = RootWindow(x11Display, x11Screen);

    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB =
        (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddressARB(
            (const GLubyte*)"glXCreateContextAttribsARB");

    int fbAttr[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        None
    };

    int fbCount = 0;
    GLXFBConfig* fbc = glXChooseFBConfig(x11Display, x11Screen, fbAttr, &fbCount);
    XVisualInfo* vi = nullptr;
    GLXContext ctx = nullptr;

    if (glXCreateContextAttribsARB && fbCount > 0) {
        int ctxAttr[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };
        ctx = glXCreateContextAttribsARB(x11Display, fbc[0], NULL, True, ctxAttr);
        if (ctx) {
            vi = glXGetVisualFromFBConfig(x11Display, fbc[0]);
        }
    }

    if (!ctx) {
        GLint att[] = {
            GLX_RGBA, GLX_DOUBLEBUFFER,
            GLX_DEPTH_SIZE, 24,
            GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8,
            None
        };
        vi = glXChooseVisual(x11Display, x11Screen, att);
        if (vi) {
            ctx = glXCreateContext(x11Display, vi, NULL, GL_TRUE);
        }
    }

    if (fbc) XFree(fbc);
    if (!vi || !ctx) return false;

    Colormap cmap = XCreateColormap(x11Display, root, vi->visual, AllocNone);
    XSetWindowAttributes swa = {0};
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | StructureNotifyMask;

    x11Window = XCreateWindow(x11Display, root, 0, 0, w, h, 0, vi->depth,
                              InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    XStoreName(x11Display, x11Window, title.c_str());
    XMapWindow(x11Display, x11Window);

    glxContext = ctx;
    glXMakeCurrent(x11Display, x11Window, glxContext);
    XFree(vi);
    running = true;
    return true;
}

void AuraWindow::pollEventsX11() {
    XEvent event;
    while (XPending(x11Display) > 0) {
        XNextEvent(x11Display, &event);
        switch (event.type) {
            case DestroyNotify:
            case ClientMessage:
                running = false;
                break;
            case KeyPress: {
                KeySym sym = XLookupKeysym(&event.xkey, 0);
                if (inputMgr && sym < 256) inputMgr->setKeyState((int)sym, true);
                break;
            }
            case KeyRelease: {
                KeySym sym = XLookupKeysym(&event.xkey, 0);
                if (inputMgr && sym < 256) inputMgr->setKeyState((int)sym, false);
                break;
            }
        }
    }
}

void AuraWindow::swapBuffersX11() {
    if (glxContext) glXSwapBuffers(x11Display, x11Window);
}

void AuraWindow::destroyX11() {
    glXMakeCurrent(x11Display, None, NULL);
    if (glxContext) glXDestroyContext(x11Display, glxContext);
    if (x11Window) XDestroyWindow(x11Display, x11Window);
    if (x11Display) XCloseDisplay(x11Display);
    x11Display = nullptr; x11Window = 0; glxContext = nullptr;
}

// ========================== Wayland ==========================

#if JWAROL_HAS_WAYLAND

// --- registry ---
static void registry_global_handler(void* data, wl_registry* registry,
                                     uint32_t name, const char* interface, uint32_t version) {
    AuraWindow* self = static_cast<AuraWindow*>(data);
    if (strcmp(interface, "wl_compositor") == 0) {
        self->wlCompositor = (wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, "wl_shell") == 0) {
        self->wlShell = (wl_shell*)wl_registry_bind(registry, name, &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        self->wlSeat = (wl_seat*)wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(self->wlSeat, &AuraWindow::seatListener, self);
    }
}

static void registry_global_remove_handler(void* data, wl_registry* registry, uint32_t name) {}

const wl_registry_listener AuraWindow::registryListener = {
    registry_global_handler,
    registry_global_remove_handler
};

// --- shell surface ---
static void shell_surface_ping_handler(void* data, wl_shell_surface* ss, uint32_t serial) {
    wl_shell_surface_pong(ss, serial);
}

static void shell_surface_configure_handler(void* data, wl_shell_surface* ss,
                                             uint32_t edges, int32_t w, int32_t h) {
    AuraWindow* self = static_cast<AuraWindow*>(data);
    if (w > 0 && h > 0 && self->wlEglWindow) {
        wl_egl_window_resize(self->wlEglWindow, w, h, 0, 0);
        self->waylandWidth = w;
        self->waylandHeight = h;
    }
}

static void shell_surface_popup_done_handler(void* data, wl_shell_surface* ss) {}

const wl_shell_surface_listener AuraWindow::shellSurfaceListener = {
    shell_surface_ping_handler,
    shell_surface_configure_handler,
    shell_surface_popup_done_handler
};

// --- seat / keyboard ---
static void seat_capabilities_handler(void* data, wl_seat* seat, uint32_t caps) {
    AuraWindow* self = static_cast<AuraWindow*>(data);
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !self->wlKeyboard) {
        self->wlKeyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(self->wlKeyboard, &AuraWindow::keyboardListener, self);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && self->wlKeyboard) {
        wl_keyboard_destroy(self->wlKeyboard);
        self->wlKeyboard = nullptr;
    }
}

static void seat_name_handler(void* data, wl_seat* seat, const char* name) {}

const wl_seat_listener AuraWindow::seatListener = {
    seat_capabilities_handler,
    seat_name_handler
};

#if JWAROL_HAS_XKBCOMMON
static void keyboard_keymap_handler(void* data, wl_keyboard* kb, uint32_t format,
                                     int fd, uint32_t size) {
    AuraWindow* self = static_cast<AuraWindow*>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    char* mapStr = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapStr == MAP_FAILED) { close(fd); return; }

    if (self->xkbKeymap) xkb_keymap_unref(self->xkbKeymap);
    if (self->xkbState) xkb_state_unref(self->xkbState);

    self->xkbKeymap = xkb_keymap_new_from_string(self->xkbContext, mapStr,
                                                   XKB_KEYMAP_FORMAT_TEXT_V1,
                                                   XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(mapStr, size);
    close(fd);

    if (self->xkbKeymap) {
        self->xkbState = xkb_state_new(self->xkbKeymap);
    }
}
#endif

static void keyboard_enter_handler(void* data, wl_keyboard* kb,
                                    uint32_t serial, wl_surface* surface, wl_array* keys) {}
static void keyboard_leave_handler(void* data, wl_keyboard* kb,
                                    uint32_t serial, wl_surface* surface) {}

static void keyboard_key_handler(void* data, wl_keyboard* kb, uint32_t serial,
                                  uint32_t time, uint32_t key, uint32_t state) {
    AuraWindow* self = static_cast<AuraWindow*>(data);
    bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    if (key == 113 && pressed) self->running = false;

    if (self->inputMgr) {
#if JWAROL_HAS_XKBCOMMON
        if (self->xkbState) {
            xkb_keysym_t sym = xkb_state_key_get_one_sym(self->xkbState, key + 8);
            if (sym < 256) {
                self->inputMgr->setKeyState((int)sym, pressed);
                if (sym >= 'a' && sym <= 'z') {
                    self->inputMgr->setKeyState((int)(sym - 32), pressed);
                }
            }
        }
#else
        if (key < 256) self->inputMgr->setKeyState((int)key, pressed);
#endif
    }
}

static void keyboard_modifiers_handler(void* data, wl_keyboard* kb, uint32_t serial,
                                        uint32_t mods_depressed, uint32_t mods_latched,
                                        uint32_t mods_locked, uint32_t group) {
#if JWAROL_HAS_XKBCOMMON
    AuraWindow* self = static_cast<AuraWindow*>(data);
    if (self->xkbState) {
        xkb_state_update_mask(self->xkbState, mods_depressed, mods_latched,
                              mods_locked, 0, 0, group);
    }
#endif
}

static void keyboard_repeat_info_handler(void* data, wl_keyboard* kb, int32_t rate, int32_t delay) {}

const wl_keyboard_listener AuraWindow::keyboardListener = {
#if JWAROL_HAS_XKBCOMMON
    keyboard_keymap_handler,
#else
    nullptr,
#endif
    keyboard_enter_handler,
    keyboard_leave_handler,
    keyboard_key_handler,
    keyboard_modifiers_handler,
    keyboard_repeat_info_handler
};

// --- init / destroy / events ---
bool AuraWindow::initWayland(int w, int h, const std::string& title) {
    wlDisplay = wl_display_connect(NULL);
    if (!wlDisplay) return false;

    wlRegistry = wl_display_get_registry(wlDisplay);
    wl_registry_add_listener(wlRegistry, &registryListener, this);
    wl_display_roundtrip(wlDisplay);

    if (!wlCompositor || !wlShell) {
        destroyWayland();
        return false;
    }

#if JWAROL_HAS_XKBCOMMON
    xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
#endif

    waylandWidth = w;
    waylandHeight = h;

    wlSurface = wl_compositor_create_surface(wlCompositor);
    wlShellSurface = wl_shell_get_shell_surface(wlShell, wlSurface);
    wl_shell_surface_add_listener(wlShellSurface, &shellSurfaceListener, this);
    wl_shell_surface_set_toplevel(wlShellSurface);
    wl_shell_surface_set_title(wlShellSurface, title.c_str());

    wlEglWindow = wl_egl_window_create(wlSurface, w, h);

    eglDisplay = eglGetDisplay((EGLNativeDisplayType)wlDisplay);
    if (eglDisplay == EGL_NO_DISPLAY) { destroyWayland(); return false; }

    EGLint major, minor;
    if (!eglInitialize(eglDisplay, &major, &minor)) { destroyWayland(); return false; }

    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfigs);
    if (numConfigs == 0) { destroyWayland(); return false; }

    eglBindAPI(EGL_OPENGL_API);

    EGLint ctxAttribs[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_NONE };
    eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        ctxAttribs[3] = 2;
        eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    }
    if (eglContext == EGL_NO_CONTEXT) { destroyWayland(); return false; }

    eglSurface = eglCreateWindowSurface(eglDisplay, config, (EGLNativeWindowType)wlEglWindow, NULL);
    if (eglSurface == EGL_NO_SURFACE) { destroyWayland(); return false; }

    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    wl_display_roundtrip(wlDisplay);
    running = true;
    return true;
}

void AuraWindow::pollEventsWayland() {
    wl_display_dispatch_pending(wlDisplay);
    wl_display_flush(wlDisplay);
}

void AuraWindow::swapBuffersWayland() {
    eglSwapBuffers(eglDisplay, eglSurface);
}

void AuraWindow::destroyWayland() {
    if (eglDisplay) {
        eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglSurface) eglDestroySurface(eglDisplay, eglSurface);
        if (eglContext) eglDestroyContext(eglDisplay, eglContext);
        eglTerminate(eglDisplay);
    }
    eglDisplay = nullptr; eglSurface = nullptr; eglContext = nullptr;

    if (wlEglWindow) wl_egl_window_destroy(wlEglWindow);
    if (wlShellSurface) wl_shell_surface_destroy(wlShellSurface);
    if (wlSurface) wl_surface_destroy(wlSurface);
    if (wlKeyboard) wl_keyboard_destroy(wlKeyboard);
    if (wlSeat) wl_seat_destroy(wlSeat);
    if (wlShell) wl_shell_destroy(wlShell);
    if (wlCompositor) wl_compositor_destroy(wlCompositor);
    if (wlRegistry) wl_registry_destroy(wlRegistry);
    if (wlDisplay) wl_display_disconnect(wlDisplay);

#if JWAROL_HAS_XKBCOMMON
    if (xkbState) xkb_state_unref(xkbState);
    if (xkbKeymap) xkb_keymap_unref(xkbKeymap);
    if (xkbContext) xkb_context_unref(xkbContext);
    xkbState = nullptr; xkbKeymap = nullptr; xkbContext = nullptr;
#endif

    wlEglWindow = nullptr; wlShellSurface = nullptr; wlSurface = nullptr;
    wlKeyboard = nullptr; wlSeat = nullptr; wlShell = nullptr;
    wlCompositor = nullptr; wlRegistry = nullptr; wlDisplay = nullptr;
}

#endif // JWAROL_HAS_WAYLAND

#endif // __linux__
