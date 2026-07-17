#pragma once

#include "ui_state.h"

LRESULT CALLBACK WndProcUI(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Registers the tool-panel window class and creates hWndUI as a small,
// initially-hidden, layered (for the hover fade-in/out -- see canvas.c's
// WM_TIMER) popup owned by hWndParent. Sets the global hWndUI itself.
// Missing from this codebase until now -- canvas.c's WM_TIMER already
// assumed hWndUI existed and would just silently no-op every check
// against it (`if (hWndUI)`) since nothing ever created it.
void createUIWindow(HINSTANCE hInst, HWND hWndParent);
