#include "../include/Window.hpp"
#include <GL/gl.h>

#ifdef _WIN32

LRESULT CALLBACK AuraWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CLOSE || uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

AuraWindow::AuraWindow(int w, int h, const std::string& title) : running(false) {
    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "AuraWin";
    RegisterClass(&wc);

    RECT r = {0, 0, w, h};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd = CreateWindowA("AuraWin", title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, hInst, NULL);
    hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1 };
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits = 32;
    pfd.iPixelType = PFD_TYPE_RGBA;
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);
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

void AuraWindow::swapBuffers() { SwapBuffers(hdc); }

AuraWindow::~AuraWindow() {
    wglMakeCurrent(NULL, NULL);
    if (hrc) wglDeleteContext(hrc);
    if (hdc) ReleaseDC(hwnd, hdc);
    if (hwnd) DestroyWindow(hwnd);
}

#elif __linux__

AuraWindow::AuraWindow(int w, int h, const std::string& title) : running(false) {
    display = XOpenDisplay(NULL);
    if (!display) return;

    screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    GLint att[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, None };
    XVisualInfo* vi = glXChooseVisual(display, screen, att);
    if (!vi) return;

    Colormap cmap = XCreateColormap(display, root, vi->visual, AllocNone);
    XSetWindowAttributes swa = {0};
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | StructureNotifyMask;

    window = XCreateWindow(display, root, 0, 0, w, h, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    XStoreName(display, window, title.c_str());
    XMapWindow(display, window);

    context = glXCreateContext(display, vi, NULL, GL_TRUE);
    glXMakeCurrent(display, window, context);
    running = true;
}

void AuraWindow::pollEvents() {
    XEvent event;
    while (XPending(display) > 0) {
        XNextEvent(display, &event);
        if (event.type == DestroyNotify || event.type == ClientMessage) running = false;
    }
}

void AuraWindow::clear(float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void AuraWindow::swapBuffers() { glXSwapBuffers(display, window); }

AuraWindow::~AuraWindow() {
    glXMakeCurrent(display, None, NULL);
    if (context) glXDestroyContext(display, context);
    if (window) XDestroyWindow(display, window);
    if (display) XCloseDisplay(display);
}

#endif
