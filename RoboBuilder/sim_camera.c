#include "sim_camera.h"
#include "config.h"
#include "ui_state.h" // for glWindowWidth/glWindowHeight -- pixel->fraction pan conversion

// See sim_camera.h for why this camera exists separately from both
// canvas.zoom/panX/panY and graphics.c's g_zoom/g_panX/g_panY. Bigger zoom
// = zoomed in, same convention as graphics.c's g_zoom.
static float simZoom = 1.0f;

// Accumulated pan, tracked as a FRACTION of the current view rather than a
// fixed world-space value -- see simCameraGetWorldPan's comment
// (sim_camera.h) for why this is what lets the environment and the robot,
// whose projections use completely different formulas/base units (and, for
// the robot, an extra per-subsystem multiplier -- the size slider), still
// stay in exact pixel lockstep with each other.
static float simPanFracX = 0.0f;
static float simPanFracY = 0.0f;

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

void simCameraPan(int dxPixels, int dyPixels)
{
    if (glWindowWidth == 0 || glWindowHeight == 0) return;

    // Deliberately independent of zoom/scale -- see sim_camera.h. A pixel
    // delta is always the same FRACTION of the viewport, no matter how far
    // zoomed in/out either subsystem currently is, or what extra scale
    // multiplier (e.g. the robot's size slider) is layered into its own
    // effective zoom -- only the WORLD distance that fraction corresponds
    // to changes with zoom/scale, not the fraction itself.
    simPanFracX -= (2.0f * dxPixels) / (float)glWindowWidth;
    simPanFracY += (2.0f * dyPixels) / (float)glWindowHeight;
}

void simCameraGetWorldPan(float halfExtentX, float halfExtentY, float* panX, float* panY)
{
    *panX = simPanFracX * halfExtentX;
    *panY = simPanFracY * halfExtentY;
}

void simCameraReset(void)
{
    simZoom = 1.0f;
    simPanFracX = 0.0f;
    simPanFracY = 0.0f;
}
