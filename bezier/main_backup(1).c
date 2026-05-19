#define UNICODE
#define _UNICODE

#include <windows.h>
#include <GL/gl.h>
#include <math.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

#define PI 3.1415926f

HDC hdc;
HGLRC hrc;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef struct {
    float x;
    float y;
} Point;

typedef struct {
    float width;
    float height;
} Segment;

typedef struct {
    float cx, cy;
    int segmentsCount;
    Segment segments[8];
} RobotBody;

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

void drawRobotBody(RobotBody b)
{
    glBegin(GL_LINE_LOOP); // closes shape automatically

    int totalSteps = 200;

    for(int i = 0; i < totalSteps; i++)
    {
        float t = (float)i / (float)totalSteps;
        float angle = t * 2.0f * PI;

        // base circle direction
        float x = cosf(angle);
        float y = sinf(angle);

        // map angle → segment index (0–7)
        int segIndex = (int)(t * 8.0f);
        if(segIndex > 7) segIndex = 7;

        Segment s = b.segments[segIndex];

        // deform shape per segment
        float px = b.cx + x * s.width;
        float py = b.cy + y * s.height;

        glVertex2f(px, py);
    }

    glEnd();
}

RobotBody robot = {
    0.0f, 0.0f,
    8,
    {
        {0.4f, 0.2f}, // segment 0 (head area)
        {0.45f, 0.25f},
        {0.5f, 0.3f}, // chest expansion
        {0.5f, 0.35f},
        {0.45f, 0.3f}, // waist
        {0.4f, 0.25f},
        {0.38f, 0.22f},
        {0.35f, 0.2f}  // lower body
    }
};

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

    drawRobotBody(robot);

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
