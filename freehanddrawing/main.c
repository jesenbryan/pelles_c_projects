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

BOOL drawing = FALSE;

HDC hDC;
HGLRC hRC;

static int hasLast = 0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void addPoint(float x, float y)
{
    if (pointCount + 1 >= MAX_POINTS) return;

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
	    hasLast = 0;
	    pointCount = 0;

	    SetCapture(hWnd);   // 👈 KEY FIX

	    return 0;
	}

	case WM_LBUTTONUP:
	{
	    drawing = FALSE;
	    hasLast = 0;

	    ReleaseCapture();

	    InvalidateRect(hWnd, NULL, TRUE);  // force redraw
	    UpdateWindow(hWnd);                // immediate repaint

	    return 0;
	}

	case WM_CAPTURECHANGED:
	{
	    drawing = FALSE;
	    hasLast = 0;
	    return 0;
	}

	case WM_MOUSEMOVE:
	{
	    if (!drawing) return 0;

	    int x = LOWORD(lParam);
	    int y = HIWORD(lParam);

	    // IMPORTANT: use last in pixel space, not normalized
	    static int lastPX = 0, lastPY = 0;
	    static int hasLastP = 0;

	    if (hasLastP)
	    {
	        int dx = x - lastPX;
	        int dy = y - lastPY;

	        // stronger threshold (prevents edge “sticking line”)
	        if (dx*dx + dy*dy < 4)
	            return 0;
	    }

	    RECT r;
	    GetClientRect(hWnd, &r);

	    // safe normalization
	    float nx = (float)x / (float)(r.right - 1);
	    float ny = (float)y / (float)(r.bottom - 1);

	    nx = nx * 2.0f - 1.0f;
	    ny = 1.0f - ny * 2.0f;

	    addPoint(nx, ny);

	    lastPX = x;
	    lastPY = y;
	    hasLastP = 1;

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

	    if (drawing)
		    glColor3f(1.0f, 0.0f, 0.0f);  // red while drawing
		else
		    glColor3f(0.0f, 0.0f, 0.0f);  // black when finished

	    float thickness = 0.005f;

	    if (pointCount < 4)
	    {
	        SwapBuffers(hDC);
	        EndPaint(hWnd, &ps);
	        return 0;
	    }

	    glBegin(GL_TRIANGLE_STRIP);

	    for (int i = 2; i < pointCount - 2; i += 2)
	    {
	        float x0 = points[i - 2];
	        float y0 = points[i - 1];

	        float x1 = points[i];
	        float y1 = points[i + 1];

	        float x2 = points[i + 2];
	        float y2 = points[i + 3];

	        // smooth tangent (IMPORTANT FIX)
	        float dx = x2 - x0;
	        float dy = y2 - y0;

	        float len = sqrtf(dx * dx + dy * dy);
	        if (len == 0) continue;

	        dx /= len;
	        dy /= len;

	        float px = -dy * thickness;
	        float py = dx * thickness;

	        // IMPORTANT: interleave correctly (this removes holes)

	        glVertex2f(x1 + px, y1 + py);
	        glVertex2f(x1 - px, y1 - py);
	    }

	    glEnd();

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
