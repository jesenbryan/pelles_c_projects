#define UNICODE
#define _UNICODE

#include <windows.h>
#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

#define MAX_POINTS 200000
#define MAX_STROKES 1000

float points[MAX_POINTS];
int pointCount = 0;

int strokeStarts[MAX_STROKES];
int strokeCount = 0;

BOOL drawing = FALSE;

HGLRC hRC;
HDC hDC;

HWND hWndGL;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
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

        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        drawing = TRUE;

        strokeStarts[strokeCount++] = pointCount;

        return 0;
    }

    case WM_LBUTTONUP:
    {
        drawing = FALSE;
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!drawing)
            return 0;

        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        RECT r;
        GetClientRect(hWnd, &r);

        float glx = (x / (float)r.right) * 2.0f - 1.0f;
        float gly = 1.0f - (y / (float)r.bottom) * 2.0f;

        points[pointCount++] = glx;
        points[pointCount++] = gly;

        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glColor3f(0.0f, 0.0f, 0.0f);
        glLineWidth(2.0f); // fixed thickness

        for (int s = 0; s < strokeCount; s++)
        {
            int start = strokeStarts[s];
            int end = (s == strokeCount - 1) ? pointCount : strokeStarts[s + 1];

            glBegin(GL_LINE_STRIP);

            for (int i = start; i < end; i += 2)
                glVertex2f(points[i], points[i + 1]);

            glEnd();
        }

        SwapBuffers(hDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hRC);
        ReleaseDC(hWnd, hDC);

        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"GLWindow";
    RegisterClass(&wc);

    hWndGL = CreateWindow(
        L"GLWindow",
        L"OpenGL Canvas",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 800, 600,
        NULL, NULL, hInst, NULL
    );

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
