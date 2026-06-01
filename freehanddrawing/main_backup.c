#define UNICODE
#define _UNICODE

#include <windows.h>
#include <gl/GL.h>
#include <math.h>

#pragma comment(lib, "opengl32.lib")

#define MAX_POINTS 200000
#define MAX_STROKES 1000

float points[MAX_POINTS];
int pointCount = 0;

int strokeStarts[MAX_STROKES];
int strokeCount = 0;

BOOL drawing = FALSE;

HDC hDC;
HGLRC hRC;

static float lastX, lastY;
static int hasLast = 0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void addPoint(float x, float y)
{
    if (pointCount + 1 >= MAX_POINTS) return;

    // simple smoothing (moving average)
    if (pointCount >= 2)
    {
        float px = points[pointCount - 2];
        float py = points[pointCount - 1];

        x = 0.7f * x + 0.3f * px;
        y = 0.7f * y + 0.3f * py;
    }

    points[pointCount++] = x;
    points[pointCount++] = y;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        hDC = GetDC(hWnd);

        PIXELFORMATDESCRIPTOR pfd = {0};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;

        int pf = ChoosePixelFormat(hDC, &pfd);
        SetPixelFormat(hDC, pf, &pfd);

        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        drawing = TRUE;

        if (strokeCount < MAX_STROKES)
            strokeStarts[strokeCount++] = pointCount;

        hasLast = 0;
        return 0;
    }

    case WM_LBUTTONUP:
        drawing = FALSE;
        return 0;

    case WM_MOUSEMOVE:
    {
        if (!drawing) return 0;

        RECT r;
        GetClientRect(hWnd, &r);

        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        float nx = (x / (float)r.right) * 2.0f - 1.0f;
        float ny = 1.0f - (y / (float)r.bottom) * 2.0f;

        // spacing filter (VERY important for quality)
        if (hasLast)
        {
            float dx = nx - lastX;
            float dy = ny - lastY;
            if (dx*dx + dy*dy < 0.00002f)
                return 0;
        }

        addPoint(nx, ny);

        lastX = nx;
        lastY = ny;
        hasLast = 1;

        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);

        RECT r;
        GetClientRect(hWnd, &r);

        glViewport(0, 0, r.right, r.bottom);

        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        float thickness = 0.005f;

        for (int s = 0; s < strokeCount; s++)
        {
            int start = strokeStarts[s];
            int end = (s == strokeCount - 1) ? pointCount : strokeStarts[s + 1];

            if (end - start < 4) continue;

            glColor3f(0, 0, 0);

            glBegin(GL_TRIANGLE_STRIP);

            for (int i = start; i < end - 2; i += 2)
            {
                float x1 = points[i];
                float y1 = points[i + 1];
                float x2 = points[i + 2];
                float y2 = points[i + 3];

                float dx = x2 - x1;
                float dy = y2 - y1;

                float len = sqrtf(dx * dx + dy * dy);
                if (len == 0) continue;

                dx /= len;
                dy /= len;

                // perpendicular vector (stroke width)
                float px = -dy * thickness;
                float py = dx * thickness;

                glVertex2f(x1 + px, y1 + py);
                glVertex2f(x1 - px, y1 - py);
            }

            glEnd();
        }

        SwapBuffers(hDC);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"PaintEngine";

    RegisterClass(&wc);

    CreateWindow(
        L"PaintEngine",
        L"Paint",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 900, 700,
        NULL, NULL, hInst, NULL
    );

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
