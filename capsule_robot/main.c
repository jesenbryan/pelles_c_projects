#define UNICODE
#define _UNICODE

#include <windows.h>
#include <gl/GL.h>
#include <math.h>

#pragma comment(lib, "opengl32.lib")

// -------------------- Robot --------------------
typedef struct
{
    float x, y;
    float w, h;
    float vx, vy;
} Robot;

Robot robot = {200, 200, 120, 60, 0, 0};

// -------------------- OpenGL helpers --------------------
void drawCircle(float cx, float cy, float r)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);

    for (int i = 0; i <= 40; i++)
    {
        float a = i * 2.0f * 3.14159f / 40.0f;
        glVertex2f(cx + cosf(a) * r, cy + sinf(a) * r);
    }

    glEnd();
}

// Capsule = rectangle + 2 circles
void drawCapsule(float x, float y, float w, float h)
{
    float r = h / 2.0f;

    // body
    glBegin(GL_QUADS);
        glVertex2f(x + r,     y);
        glVertex2f(x + w - r, y);
        glVertex2f(x + w - r, y + h);
        glVertex2f(x + r,     y + h);
    glEnd();

    // ends
    drawCircle(x + r,     y + r, r);
    drawCircle(x + w - r, y + r, r);
}

// -------------------- Input --------------------
void handleInput()
{
    robot.vx = 0;
    robot.vy = 0;

    if (GetAsyncKeyState(VK_LEFT))  robot.vx = -3;
    if (GetAsyncKeyState(VK_RIGHT)) robot.vx =  3;
    if (GetAsyncKeyState(VK_UP))    robot.vy = -3;
    if (GetAsyncKeyState(VK_DOWN))  robot.vy =  3;
}

// -------------------- Win32 + OpenGL --------------------
HDC hdc;
HGLRC hrc;

void initGL(HWND hwnd)
{
    hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);

    hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);
}

void render()
{
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 800, 600, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0.2f, 0.6f, 1.0f);
    drawCapsule(robot.x, robot.y, robot.w, robot.h);

    SwapBuffers(hdc);
}

// -------------------- Window Proc --------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// -------------------- Main --------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CapsuleRobot";

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        L"CapsuleRobot",
        L"Capsule Robot",
        WS_OVERLAPPEDWINDOW,
        100, 100, 800, 600,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    initGL(hwnd);

    MSG msg;

    while (1)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        handleInput();

        robot.x += robot.vx;
        robot.y += robot.vy;

        render();
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hrc);
    ReleaseDC(hwnd, hdc);

    return 0;
}
