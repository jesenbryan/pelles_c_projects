#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include "geometry.h"
#include "robot.h"


#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")

Semni robot;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void drawRobot(Semni* r)
{
    glPushMatrix();

    glTranslatef(r->leftX, r->y, 0);
    glRotatef(r->angle, 0, 0, 1);

    glBegin(GL_LINES);
        glVertex2f(-0.2f, 0.0f);
        glVertex2f( 0.2f, 0.0f);
    glEnd();

    glPopMatrix();
}

void draw()
{
    glClear(GL_COLOR_BUFFER_BIT);

    drawRobot(&robot);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"RobotWindow";

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        L"RobotWindow",
        L"Robot Simulation",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(hwnd, nShowCmd);

    // simple OpenGL context (Pelles C style minimal setup)
    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);

    HGLRC glrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, glrc);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

    MSG msg;
    while (1)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return 0;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        draw();

        SwapBuffers(hdc);
    }

    return 0;
}
