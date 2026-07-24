#include <windows.h>
#include <GL/gl.h>

#include "graphics.h"
#include "config.h"
#include "ui_state.h"   // for appMode -- Simulation mode routes through sim_camera instead
#include "sim_camera.h"

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

// Current "Robot Size" slider value (see ROBOT_SCALE_MIN/MAX in config.h,
// input.c's WM_HSCROLL) -- folded directly into effectiveZoom below so
// resizing the robot and zooming the camera share the exact same
// projection math.
static float g_robotScale = 0.5f;

static float effectiveZoom(void)
{
    return g_zoom * g_robotScale;
}

// Manual view pan offset, in world units -- moved ONLY by graphicsPan()
// during a middle-mouse drag (see input.c) or reset by graphicsResetView.
// Kept separate from g_zoom's "never drift automatically" guarantee: this
// only ever changes in response to an explicit user drag, so the robot
// can be panned out of view the same deliberate way ArcSpline strokes can
// (see canvas.c's canvas.panX/panY). Deliberately NOT touched by
// graphicsSetRobotScale (see g_scaleAnchorX/Y below) -- keeping "the user
// dragged the view" and "the Robot Size slider shifted the view to keep
// the robot centered" as two separate numbers that ADD together (see
// graphicsGetPan) lets drawDashedHorizontalLine cancel out just the
// second one and stay put while the robot still correctly grows/shrinks
// in place underneath it.
static float g_panX = 0.0f;
static float g_panY = 0.0f;

// Extra pan contributed by graphicsSetRobotScale alone, to keep the
// robot's own center fixed on screen across a Robot Size change (see that
// function's comment for the derivation). Added to g_panX/g_panY (not
// merged into them) everywhere the COMBINED view pan is needed (rendering,
// hit-testing) via graphicsGetPan -- but graphicsGetManualPan deliberately
// exposes g_panX/g_panY alone, without this, for the one caller that needs
// to ignore it (the ground reference line).
static float g_scaleAnchorX = 0.0f;
static float g_scaleAnchorY = 0.0f;

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
//
// In Simulation mode, the robot is driven by sim_camera's own independent
// zoom instead of g_zoom -- see sim_camera.h for why (keeps the robot and
// the ArcSpline environment zooming/panning together as one scene, without
// disturbing either subsystem's own Design-mode camera) -- but the "Robot
// Size" slider (g_robotScale) still layers on top either way, so resizing
// the robot has the same visible effect whether or not Simulation mode is
// active.
static void applyProjection(void)
{
    if (g_lastH == 0) g_lastH = 1;

    glViewport(0, 0, g_lastW, g_lastH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)g_lastW / (float)g_lastH;
    float zoom = (appMode == APP_MODE_SIMULATION) ? (simCameraGetZoom() * g_robotScale) : effectiveZoom();
    float halfY = 1.5f / zoom;
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

// In Simulation mode, reads sim_camera's zoom/pan instead of g_zoom/
// g_panX/g_panY -- has to agree with whatever applyProjection/
// renderRobotScene actually drew the robot with (see sim_camera.h), or
// hit-testing (e.g. canvas.c's whole-robot drag) would disagree with what's
// on screen.
void screenToGL(HWND hwnd, int mx, int my, float *x, float *y)
{
    RECT r;
    GetClientRect(hwnd, &r);

    float w = (float)r.right;
    float h = (float)r.bottom;

    float aspect = w / h;

    float nx = (mx / w) * 2.0f - 1.0f;
    float ny = 1.0f - (my / h) * 2.0f;

    float zoom, panX, panY, halfY, halfX;
    if (appMode == APP_MODE_SIMULATION)
    {
        zoom = simCameraGetZoom() * g_robotScale; // keep in sync with applyProjection above
        halfY = 1.5f / zoom;
        halfX = halfY * aspect;
        simCameraGetWorldPan(halfX, halfY, &panX, &panY);
    }
    else
    {
        zoom = effectiveZoom();
        halfY = 1.5f / zoom;
        halfX = halfY * aspect;
        panX = g_panX + g_scaleAnchorX;
        panY = g_panY + g_scaleAnchorY;
    }

    *x = (nx * halfX) + panX;
    *y = (ny * halfY) + panY;
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

// In Simulation mode, returns sim_camera's pan (converted into the robot's
// own current world units -- see simCameraGetWorldPan) instead of g_panX/
// g_panY -- renderer.c's renderRobotScene/renderApp call this to position
// the robot, so this branch is what actually makes the robot follow the
// sim camera during simulation instead of Semni's own Design-mode pan.
void graphicsGetPan(float* panX, float* panY)
{
    if (appMode == APP_MODE_SIMULATION)
    {
        // Same half-extent as applyProjection, so the pan this hands back
        // agrees exactly with the projection the robot was actually just
        // drawn with this frame.
        float aspect = (float)g_lastW / (float)g_lastH;
        float zoom = simCameraGetZoom() * g_robotScale;
        float halfY = 1.5f / zoom;
        float halfX = halfY * aspect;
        simCameraGetWorldPan(halfX, halfY, panX, panY);
        return;
    }
    *panX = g_panX + g_scaleAnchorX;
    *panY = g_panY + g_scaleAnchorY;
}

// The pure user-drag component of the Design-mode pan (graphicsPan/
// graphicsResetView only) -- excludes g_scaleAnchorX/Y, unlike
// graphicsGetPan above. The ground reference line (renderer.c's
// drawDashedHorizontalLine) needs exactly this: it wants to track a
// genuine user pan/zoom like everything else on screen, but stay
// completely still when the Robot Size slider moves the ANCHOR part of
// the pan instead.
void graphicsGetManualPan(float* panX, float* panY)
{
    *panX = g_panX;
    *panY = g_panY;
}

float graphicsGetZoom(void)
{
    return g_zoom;
}

// Sets the "Robot Size" slider's value directly (input.c's WM_HSCROLL
// passes pos/100.0f, see ROBOT_SCALE_MIN/MAX in config.h) -- folds
// straight into effectiveZoom/applyProjection/screenToGL/graphicsGetPan,
// so this is a projection multiplier applied on top of camera zoom, not a
// change to the robot's own stored geometry.
//
// (centerX, centerY) is the robot's own current center (in world units --
// input.c passes whichever of getCenter/getRockyCenter/getStiloCenter
// matches the active kind). Since this is a pure projection zoom, not a
// geometry change, it scales everything around the same point
// applyProjection always has: the combined pan (g_panX+g_scaleAnchorX,
// g_panY+g_scaleAnchorY), NOT the robot's center -- so without this
// adjustment, a robot whose center isn't already sitting on that point
// (true almost all the time; see app_init.c's poses, most of which put
// the center well below y=0) visibly drifts across the screen as it
// resizes instead of growing/shrinking in place.
//
// Solving for the new ANCHOR (not the user's own manual pan, g_panX/Y,
// which this deliberately leaves untouched -- see its comment) that keeps
// (centerX, centerY) projected to the exact same screen position before
// and after: writing D = center - manualPan (the center relative to
// wherever the user has manually panned to), (D - anchor)/half must stay
// constant, and half scales by oldScale/newScale when only the robot-size
// factor of effectiveZoom changes -- gives
// anchor' = D - (oldScale/newScale) * (D - anchor), applied per-axis
// below.
void graphicsSetRobotScale(float scale, float centerX, float centerY)
{
    if (g_robotScale > 0.0f && scale > 0.0f)
    {
        float ratio = g_robotScale / scale;

        float dx = centerX - g_panX;
        float dy = centerY - g_panY;

        g_scaleAnchorX = dx - ratio * (dx - g_scaleAnchorX);
        g_scaleAnchorY = dy - ratio * (dy - g_scaleAnchorY);
    }

    g_robotScale = scale;
    applyProjection();
}

// Current "Robot Size" slider value -- exposed so input.c's pick-
// tolerance math and canvas.c's robot/environment coordinate conversion
// (robotPointToEnvWorld/robotLengthToEnvWorld) can fold it in the same
// way applyProjection does, since it affects the robot's on-screen size
// exactly like an extra zoom factor.
float graphicsGetRobotScale(void)
{
    return g_robotScale;
}

void graphicsResetView(void)
{
    g_zoom = 1.0f;
    g_panX = 0.0f;
    g_panY = 0.0f;
    g_scaleAnchorX = 0.0f;
    g_scaleAnchorY = 0.0f;

    applyProjection();
}
