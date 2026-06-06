#include <windows.h>
#include <GL/gl.h>

#include "graphics.h"

static HDC hdc;
static HGLRC hrc;

HDC graphicsGetHDC(void)
{
    return hdc;
}

void setupOpenGL(HWND hwnd)
{
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0,0,0,0,0,0,
        0,0,
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

    glClearColor(1,1,1,1);
}

void resizeGL(int w, int h)
{
    if (h == 0) h = 1;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)w / (float)h;

    if (aspect >= 1.0f)
        glOrtho(-1.5f * aspect, 1.5f * aspect, -1.5f, 1.5f, -1, 1);
    else
        glOrtho(-1.5f, 1.5f, -1.5f / aspect, 1.5f / aspect, -1, 1);

    glMatrixMode(GL_MODELVIEW);
}

void screenToGL(HWND hwnd, int mx, int my, float *x, float *y)
{
    RECT r;
    GetClientRect(hwnd, &r);

    float w = (float)r.right;
    float h = (float)r.bottom;

    float aspect = w / h;

    float nx = (mx / w) * 2.0f - 1.0f;
    float ny = 1.0f - (my / h) * 2.0f;

    if (aspect >= 1.0f)
    {
        *x = nx * 1.5f * aspect;
        *y = ny * 1.5f;
    }
    else
    {
        *x = nx * 1.5f;
        *y = ny * 1.5f / aspect;
    }
}

void graphicsOnResize(int w, int h)
{
    if (h == 0) h = 1;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)w / (float)h;

    if (aspect >= 1.0f)
        glOrtho(-1.5f * aspect, 1.5f * aspect, -1.5f, 1.5f, -1, 1);
    else
        glOrtho(-1.5f, 1.5f, -1.5f / aspect, 1.5f / aspect, -1, 1);

    glMatrixMode(GL_MODELVIEW);
}

