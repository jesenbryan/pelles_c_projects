#include <windows.h>
#include <GL/gl.h>

#include "graphics.h"
#include "config.h"

static HDC hdc;
static HGLRC hrc;

// current view zoom. zoom > 1 magnifies. Layered on top of the base
// 1.5-unit half-extent ortho projection used everywhere else. Kept here
// (not in AppState) since it's purely a projection concern -- screenToGL
// and the projection setup need to agree on the same value, so they live
// side by side instead of being threaded through from the caller.
//
// Deliberately NOT paired with a pan offset: an earlier version panned
// toward the cursor on every zoom step, which could drift the view away
// from the robot entirely (easy to do, since the robot is drawn as thin
// outlines, not filled shapes -- a tiny cursor offset from any actual
// line was enough to end up scrolled into blank canvas with nothing
// findable on screen). Always zooming around a fixed center (world
// origin, which is also the body's own rotation pivot -- see getCenter())
// means the robot can never be scrolled out of view, at the cost of not
// being able to zoom toward wherever the cursor happens to be.
static float g_zoom = 1.0f;

// Robot "size" slider value (see ROBOT_SCALE_MIN/MAX in config.h) -- set
// via graphicsSetRobotScale from the trackbar in input.c. Combined
// multiplicatively with g_zoom everywhere g_zoom is used below (see
// effectiveZoom()), rather than kept as a wholly separate transform, so
// this reuses the exact projection/screenToGL math that already keeps
// zoomed rendering and hit-testing in sync -- the slider gets that same
// guarantee for free instead of needing its own parallel bookkeeping.
static float g_robotScale = 0.5f;

static float effectiveZoom(void)
{
    return g_zoom * g_robotScale;
}

// Manual view pan offset, in world units -- moved by graphicsPan() during
// a middle-mouse drag (see input.c). Kept separate from g_zoom's "never
// drift automatically" guarantee: this only ever changes in response to
// an explicit user drag, so the robot can be panned out of view the same
// deliberate way ArcSpline strokes can (see canvas.c's canvas.panX/panY).
static float g_panX = 0.0f;
static float g_panY = 0.0f;

// last known viewport size, cached so a zoom change can reapply the
// projection without waiting for the next WM_SIZE
static int g_lastW = 800;
static int g_lastH = 600;

HDC graphicsGetHDC(void)
{
    return hdc;
}

// Recomputes the ortho projection from the current viewport size and
// zoom. Shared by graphicsOnResize and graphicsZoom so they can never
// drift out of sync with each other or with screenToGL.
static void applyProjection(void)
{
    if (g_lastH == 0) g_lastH = 1;

    glViewport(0, 0, g_lastW, g_lastH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)g_lastW / (float)g_lastH;
    float halfY = 1.5f / effectiveZoom();
    float halfX = halfY * aspect;

    glOrtho(-halfX, halfX, -halfY, halfY, -1, 1);

    glMatrixMode(GL_MODELVIEW);
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

    applyProjection();

    glClearColor(1,1,1,1);

    // needed so the semi-transparent handles (drawn with glColor4f alpha)
    // actually blend with what's underneath instead of being fully opaque
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void graphicsOnResize(int w, int h)
{
    if (h == 0) h = 1;

    g_lastW = w;
    g_lastH = h;

    applyProjection();
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

    float halfY = 1.5f / effectiveZoom();
    float halfX = halfY * aspect;

    *x = (nx * halfX) + g_panX;
    *y = (ny * halfY) + g_panY;
}

void graphicsZoom(float factor)
{
    float newZoom = g_zoom * factor;
    if (newZoom < MIN_ZOOM) newZoom = MIN_ZOOM;
    if (newZoom > MAX_ZOOM) newZoom = MAX_ZOOM;

    g_zoom = newZoom;

    applyProjection();
}

void graphicsPan(int dxPixels, int dyPixels)
{
    if (g_lastW == 0 || g_lastH == 0) return;

    float aspect = (float)g_lastW / (float)g_lastH;
    float halfY = 1.5f / effectiveZoom();
    float halfX = halfY * aspect;

    float worldPerPixelX = (2.0f * halfX) / (float)g_lastW;
    float worldPerPixelY = (2.0f * halfY) / (float)g_lastH;

    // screen Y grows downward, world/GL Y grows upward, so the two axes
    // flip sign relative to each other -- matches canvas.c's panX/panY drag
    g_panX -= dxPixels * worldPerPixelX;
    g_panY += dyPixels * worldPerPixelY;
}

void graphicsGetPan(float* panX, float* panY)
{
    *panX = g_panX;
    *panY = g_panY;
}

float graphicsGetZoom(void)
{
    return g_zoom;
}

void graphicsSetRobotScale(float scale)
{
    if (scale < ROBOT_SCALE_MIN) scale = ROBOT_SCALE_MIN;
    if (scale > ROBOT_SCALE_MAX) scale = ROBOT_SCALE_MAX;

    g_robotScale = scale;

    applyProjection();
}

float graphicsGetRobotScale(void)
{
    return g_robotScale;
}
