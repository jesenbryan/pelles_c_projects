#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <windows.h>

HDC graphicsGetHDC(void);
void setupOpenGL(HWND hwnd);
void resizeGL(int w, int h);
void screenToGL(HWND hwnd, int mx, int my, float* x, float* y);
void graphicsOnResize(int w, int h);

#endif
