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
