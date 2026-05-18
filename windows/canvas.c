#include <windows.h>
#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

HGLRC hRC;
HDC hDC;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_CREATE:
    {
        PIXELFORMATDESCRIPTOR pfd = {0};

        hDC = GetDC(hWnd);

        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW |
                      PFD_SUPPORT_OPENGL |
                      PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;

        int pf = ChoosePixelFormat(hDC, &pfd);
        SetPixelFormat(hDC, pf, &pfd);

        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);

        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // DRAW TRIANGLE
        glBegin(GL_TRIANGLES);

            glColor3f(1,0,0);
            glVertex2f(-0.5f, -0.5f);

            glColor3f(0,1,0);
            glVertex2f(0.5f, -0.5f);

            glColor3f(0,0,1);
            glVertex2f(0.0f, 0.5f);

        glEnd();

        SwapBuffers(hDC);

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_DESTROY:
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hRC);
        ReleaseDC(hWnd, hDC);

        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInst,
                   HINSTANCE hPrev,
                   LPSTR lpCmd,
                   int nShow)
{
    WNDCLASS wc = {0};

    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"OpenGLWindow";

    RegisterClass(&wc);

    HWND hWnd = CreateWindow(
        L"OpenGLWindow",
        L"OpenGL Test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100,
        800, 600,
        NULL,
        NULL,
        hInst,
        NULL
    );

    MSG msg;

    while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
