#pragma once

#include <windows.h>
#include "app.h"

// Renders "app" into the back buffer without the draggable handles, reads
// that back, and writes it out as an uncompressed 24-bit BMP -- so the
// exported image shows the robot/paint/environment content only, not the
// editor's handle overlay. "hwnd" is just used to get the canvas size.
// Returns 1 on success, 0 on failure (bad window size, can't open the
// file, etc). What's currently displayed on screen is left untouched.
int saveCanvasAsBMP(const char* filename, HWND hwnd, AppState* app);

// Saves the robot's current pose as mathematical equations in a text file.
// Includes all geometric parameters (circle positions/radii, arc angles, etc)
// in a human-readable mathematical format.
int saveRobotAsEquations(const char* filename, AppState* app);
