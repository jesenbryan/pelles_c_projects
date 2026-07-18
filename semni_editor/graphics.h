#pragma once

#include <windows.h>

HDC graphicsGetHDC(void);
void setupOpenGL(HWND hwnd);
void screenToGL(HWND hwnd, int mx, int my, float* x, float* y);
void graphicsOnResize(int w, int h);

// Zooms the canvas view by `factor` (>1 zooms in, <1 zooms out). Always
// centered on the world origin (the robot's own rotation pivot -- see
// getCenter()) rather than the cursor, so the robot can never be scrolled
// out of view no matter how much or how fast you zoom.
void graphicsZoom(float factor);

// Pans the view by a screen-space pixel delta (as seen between two mouse
// positions during a middle-mouse drag), converting it to world units at
// the current zoom level -- mirrors the ArcSpline canvas's own drag-pan
// (see canvas.c's WM_MBUTTONDOWN/WM_MOUSEMOVE handling) so both editor
// modes pan the same way. Unlike graphicsZoom, this is a manual,
// user-driven offset rather than an automatic one, so it doesn't carry
// the "can drift the robot out of view" risk graphicsZoom's comment
// warns about.
void graphicsPan(int dxPixels, int dyPixels);

// Current view pan offset, in world units. Applied by the renderer via
// glTranslatef(-panX, -panY, 0) so drawing and screenToGL's hit-testing
// (which also adds this offset) stay in agreement.
void graphicsGetPan(float* panX, float* panY);
