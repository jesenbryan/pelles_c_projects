#include "sim_camera.h"
#include "config.h"
#include "ui_state.h" // for glWindowWidth/glWindowHeight -- pixel->world pan conversion

// See sim_camera.h for why this camera exists separately from both
// canvas.zoom/panX/panY and graphics.c's g_zoom/g_panX/g_panY. Bigger zoom
// = zoomed in, same convention as graphics.c's g_zoom.
static float simZoom = 1.0f;
static float simPanX = 0.0f;
static float simPanY = 0.0f;

// The robot's own base half-extent, matching graphics.c's applyProjection:
// halfY = 1.5 / zoom. simPanX/simPanY are tracked in these same units (see
// simCameraPan) since that's the one existing "screen pixel <-> world
// unit" conversion this file can reuse verbatim from graphics.c's own
// pan/projection math. simCameraGetPanScaled converts out of this base for
// any other subsystem (e.g. the ArcSpline canvas's own base of 1.0).
#define ROBOT_BASE_HALF_EXTENT 1.5f

float simCameraGetZoom(void)
{
    return simZoom;
}

void simCameraZoom(float factor)
{
    float newZoom = simZoom * factor;
    if (newZoom < MIN_ZOOM) newZoom = MIN_ZOOM;
    if (newZoom > MAX_ZOOM) newZoom = MAX_ZOOM;
    simZoom = newZoom;
}

void simCameraGetPan(float* panX, float* panY)
{
    *panX = simPanX;
    *panY = simPanY;
}

void simCameraGetPanScaled(float baseHalfExtent, float* panX, float* panY)
{
    float ratio = baseHalfExtent / ROBOT_BASE_HALF_EXTENT;
    *panX = simPanX * ratio;
    *panY = simPanY * ratio;
}

void simCameraPan(int dxPixels, int dyPixels)
{
    if (glWindowWidth == 0 || glWindowHeight == 0) return;

    // Same halfY = 1.5 / zoom convention as graphics.c's applyProjection/
    // graphicsPan, so a pixel delta maps to the same world distance the
    // projection actually draws it as.
    float aspect = (float)glWindowWidth / (float)glWindowHeight;
    float halfY = ROBOT_BASE_HALF_EXTENT / simZoom;
    float halfX = halfY * aspect;

    float worldPerPixelX = (2.0f * halfX) / (float)glWindowWidth;
    float worldPerPixelY = (2.0f * halfY) / (float)glWindowHeight;

    // screen Y grows downward, world/GL Y grows upward -- same sign flip
    // as canvas.c's own panX/panY drag and graphics.c's graphicsPan.
    simPanX -= dxPixels * worldPerPixelX;
    simPanY += dyPixels * worldPerPixelY;
}
