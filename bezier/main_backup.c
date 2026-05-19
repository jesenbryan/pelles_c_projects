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

HDC hdc;
HGLRC hrc;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

void line(Point a, Point b)
{
    glBegin(GL_LINES);
    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glEnd();
}

void halfCircle(Point c, float r, float startAngle, float endAngle)
{
    glBegin(GL_LINE_STRIP);

    for(float t = startAngle; t <= endAngle; t += 0.05f)
    {
        glVertex2f(
            c.x + cosf(t) * r,
            c.y + sinf(t) * r
        );
    }

    glEnd();
}

Point bezier(Point p0, Point p1, Point p2, float t)
{
    float u = 1.0f - t;

    Point p;
    p.x = u*u*p0.x + 2*u*t*p1.x + t*t*p2.x;
    p.y = u*u*p0.y + 2*u*t*p1.y + t*t*p2.y;

    return p;
}

void drawRobotBody()
{
    Point left  = {-0.6f, 0.0f};
    Point right = { 0.6f, 0.0f};
    float r = 0.4f;

    // LEFT HALF CIRCLE (top → bottom)
    halfCircle(left, r, 1.57f, 4.71f);

    // RIGHT HALF CIRCLE (bottom → top)
    halfCircle(right, r, -1.57f, 1.57f);

    // TOP CURVE (connect left → right)
    glBegin(GL_LINE_STRIP);
    for(float t = 0; t <= 1; t += 0.02f)
    {
        Point p = bezier(
            (Point){-0.6f, 0.4f},
            (Point){0.0f, 0.7f},
            (Point){0.6f, 0.4f},
            t
        );
        glVertex2f(p.x, p.y);
    }
    glEnd();

    // BOTTOM CURVE (connect right → left)
    glBegin(GL_LINE_STRIP);
    for(float t = 0; t <= 1; t += 0.02f)
    {
        Point p = bezier(
            (Point){0.6f, -0.4f},
            (Point){0.0f, -0.7f},
            (Point){-0.6f, -0.4f},
            t
        );
        glVertex2f(p.x, p.y);
    }
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
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(0, 0, 0);

    drawRobotBody();

    SwapBuffers(hdc);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"RobotWindow";

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        L"Robot Body",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL,
        hInst, NULL
    );

    ShowWindow(hwnd, nShow);

    setupOpenGL(hwnd);

    MSG msg = {0};

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
