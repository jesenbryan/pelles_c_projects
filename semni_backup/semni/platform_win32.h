#pragma once

#include <windows.h>

void platformInitConsole(void);

HWND platformCreateMainWindow(HINSTANCE hInst, int nShowCmd, WNDPROC wndProc);

void platformInitCommonControls(void);
