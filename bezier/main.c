#define UNICODE
#define _UNICODE

#include <windows.h>
#include <GL/gl.h>
#include <math.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

typedef struct {
    float x, y;
} Point;

typedef struct {
    float leftX, rightX;
    float y;
    float radius;
} CapsuleBody;

CapsuleBody robot;

HDC hdc;
HGLRC hrc;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

//robot.leftX = -0.6f;
//robot.rightX = 0.6f;
//robot.y = 0.0f;
//robot.radius = 0.2f;   // 👈 adjust size here

void drawCircle(Point c, float r)
{
    glBegin(GL_LINE_LOOP);

    for (float t = 0; t < 6.28318f; t += 0.05f)
    {
        glVertex2f(
            c.x + cosf(t) * r,
            c.y + sinf(t) * r
        );
    }

    glEnd();
}

void drawCapsuleBody(CapsuleBody b)
{
    Point left  = { b.leftX,  b.y };
    Point right = { b.rightX, b.y };

    float r = b.radius;

    // circles (ends)
    drawCircle(left, r);
    drawCircle(right, r);

    // top line
    glBegin(GL_LINES);
    glVertex2f(left.x,  left.y + r);
    glVertex2f(right.x, right.y + r);
    glEnd();

    // bottom line
    glBegin(GL_LINES);
    glVertex2f(left.x,  left.y - r);
    glVertex2f(right.x, right.y - r);
    glEnd();
}

void setupOpenGL(HWND hwnd)
{
    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0,0,0,0,0,0,
        0,
        0,
        0,
        0,0,0,0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0,0,0
    };

    hdc = GetDC(hwnd);

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);

    hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.5, 1.5, -1.5, 1.5, -1, 1);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0, 0, 0);

    drawCapsuleBody(robot);

    SwapBuffers(hdc);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CapsuleRobot";

    RegisterClass(&wc);

	robot.leftX = -0.6f;
	robot.rightX = 0.6f;
	robot.y = 0.0f;
	robot.radius = 0.2f;

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        L"Capsule Robot Body",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL,
        hInst, NULL
    );

    ShowWindow(hwnd, nShowCmd);

    setupOpenGL(hwnd);

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

        render();
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
