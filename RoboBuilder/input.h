#pragma once

#include "app.h"
#include <windows.h>

LRESULT handleInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, AppState* app);

// Confirms the robot panel's Weight box: applies a valid typed number to the
// active robot's weight (invalid/empty text is discarded) and reformats the
// box as "N.NN". Called on Enter / focus loss, and by File > Save so an
// unconfirmed value still gets saved.
void commitWeightEdit(AppState* app);
