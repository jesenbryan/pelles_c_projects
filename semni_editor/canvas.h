#pragma once

#include "ui_state.h"

LRESULT CALLBACK WndProcGL(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateProjection(void);

// The HDC of the single shared OpenGL window, set up once in WndProcGL's
// WM_CREATE handler. Exposed so the Semni robot editor (which shares the
// same window/GL context when the app is in Editor Mode: Semni) can pass
// it to renderApp() for SwapBuffers, instead of creating its own separate
// GL context via graphics.c's setupOpenGL().
HDC canvasGetHDC(void);
