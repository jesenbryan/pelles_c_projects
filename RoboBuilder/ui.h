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

// Programmatically sets Comparison Mode on/off and keeps the "Comparison
// Mode" checkbox's own displayed state in sync with it -- unlike a raw
// `canvas.comparisonMode = ...` write, which would leave hComparisonBtn
// showing the old state the next time the Environment panel is opened.
// Used by canvas.c when entering Simulation mode (see its
// ID_MODE_SIMULATION handler) to automatically switch the Environment
// view over to the reconstructed/arc-fitted drawing, since that's what
// ground collision actually tests against now, not the original strokes.
void SetComparisonModeUI(BOOL on);
