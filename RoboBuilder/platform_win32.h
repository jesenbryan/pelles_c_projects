#pragma once

#include <windows.h>

void platformInitConsole(void);

// hMenu may be NULL for no menu bar.
HWND platformCreateMainWindow(HINSTANCE hInst, int nShowCmd, WNDPROC wndProc, HMENU hMenu);

void platformInitCommonControls(void);
