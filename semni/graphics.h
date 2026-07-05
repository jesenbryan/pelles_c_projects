#pragma once

#include <windows.h>

HDC graphicsGetHDC(void);
void setupOpenGL(HWND hwnd);
void screenToGL(HWND hwnd, int mx, int my, float* x, float* y);
void graphicsOnResize(int w, int h);
