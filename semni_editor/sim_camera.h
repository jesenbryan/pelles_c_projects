#pragma once

#include <windows.h>

// Simulation mode's own, independent camera (zoom + pan) -- entirely
// separate from the ArcSpline canvas's canvas.zoom/panX/panY (Design >
// Environment) and Semni's own g_zoom/g_panX/g_panY (Design > Robot,
// graphics.c). While appMode == APP_MODE_SIMULATION, canvas.c's
// renderCombinedFrame drives BOTH subsystems (environment + robot) through
// this single camera instead -- see UpdateProjection/canvasRenderFrame
// (canvas.c) and applyProjection/screenToGL/graphicsGetPan (graphics.c),
// each of which branch on appMode to read from here instead of their own
// Design-mode state. This keeps the environment and robot zooming/panning
// together as one scene during simulation, and means leaving Simulation
// mode leaves each subsystem's own Design-mode camera exactly where it was
// -- neither camera is ever touched by anything that happens in
// Simulation.
//
// Same zoom convention as graphics.c's g_zoom (bigger = zoomed IN, layered
// as 1.5 / zoom into the ortho half-extent) -- simplest to reuse since both
// subsystems' projection math already follows that convention, and the
// robot's own drag hit-testing (canvas.c's WM_LBUTTONDOWN/WM_MOUSEMOVE,
// via screenToGL) needs to agree with the projection actually used to draw
// it either way.

float simCameraGetZoom(void);

// Zooms by `factor` (>1 zooms in, <1 zooms out), clamped to MIN_ZOOM/
// MAX_ZOOM (config.h) -- same clamp range and multiplicative-per-step feel
// as graphicsZoom.
void simCameraZoom(float factor);

// Pan in the robot's own world units -- i.e. already the exact value
// graphics.c's graphicsGetPan hands off to renderRobotScene/screenToGL, no
// conversion needed there. See simCameraGetPanScaled below for any OTHER
// subsystem (base half-extent != graphics.c's 1.5).
void simCameraGetPan(float* panX, float* panY);

// Same pan, but scaled for a caller-specific base half-extent instead of
// graphics.c's own 1.5 (see applyProjection's halfY = 1.5 / zoom). The
// ArcSpline canvas's own UpdateProjection uses a different base (1.0, its
// own zoom=1 half-extent) -- since the two subsystems' projections don't
// share a base unit, panning them by the identical raw world-space value
// would shift them by different numbers of SCREEN pixels for the same
// drag, breaking the "moves together as one scene" goal this camera
// exists for. Scaling by (baseHalfExtent / 1.5) cancels that out, so
// whichever subsystem calls this with its own base stays in exact
// pixel-lockstep with the robot, no matter the current zoom level.
void simCameraGetPanScaled(float baseHalfExtent, float* panX, float* panY);

// Pans by a screen-space pixel delta (as seen between two mouse positions
// during a middle-mouse drag), converted to world units (in the robot's
// own base, see simCameraGetPan) at the sim camera's current zoom -- same
// role/convention as canvas.c's own drag-pan and graphics.c's graphicsPan.
void simCameraPan(int dxPixels, int dyPixels);
