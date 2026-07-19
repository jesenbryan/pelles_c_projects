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
// Same zoom convention as graphics.c's g_zoom (bigger = zoomed IN) --
// simplest to reuse since the robot's own projection already follows that
// convention (applyProjection's halfY = 1.5 / zoom), and its own drag hit-
// testing (canvas.c's WM_LBUTTONDOWN/WM_MOUSEMOVE, via screenToGL) needs to
// agree with the projection actually used to draw it either way. The
// robot's ACTUAL effective zoom during Simulation is
// simCameraGetZoom() * graphicsGetRobotScale() -- the "size" slider set in
// Design > Robot mode is layered on top here too, so a robot sized down/up
// with that slider renders at that same size in Simulation instead of
// always reverting to its unscaled size (see graphics.c's applyProjection/
// screenToGL).

float simCameraGetZoom(void);

// Zooms by `factor` (>1 zooms in, <1 zooms out), clamped to MIN_ZOOM/
// MAX_ZOOM (config.h) -- same clamp range and multiplicative-per-step feel
// as graphicsZoom.
void simCameraZoom(float factor);

// Pans by a screen-space pixel delta (as seen between two mouse positions
// during a middle-mouse drag) -- see simCameraGetWorldPan for how this
// converts into a specific subsystem's own world units.
void simCameraPan(int dxPixels, int dyPixels);

// Converts the accumulated pan into world units for a caller-specific
// projection half-extent (halfExtentX/halfExtentY -- e.g. graphics.c's own
// halfX/halfY from applyProjection, or canvas.c's own from
// UpdateProjection). Internally, pan is tracked as a FRACTION of the
// current view rather than a fixed world-space value -- a given pixel drag
// always shifts the view by the same fraction of whatever's currently
// visible, regardless of the current zoom level or of any extra per-
// subsystem multiplier (like graphics.c's robot-size slider) layered into
// that subsystem's own effective zoom. Multiplying that fraction by EACH
// subsystem's own CURRENT half-extent (computed however that subsystem
// likes -- the environment's and the robot's projection formulas don't
// even share a base unit) is what lets two totally different projections
// still end up shifted by the exact same number of screen pixels for a
// single simCameraPan() call.
void simCameraGetWorldPan(float halfExtentX, float halfExtentY, float* panX, float* panY);

// Resets zoom to 100% and pan to center -- bound to Ctrl+0 in Simulation
// mode (see canvas.c's WM_KEYDOWN). Does NOT touch graphics.c's
// g_robotScale (the size slider) -- that's the robot's own configured
// size, not a camera position, same reasoning as everywhere else this
// camera deliberately leaves it alone.
void simCameraReset(void);
