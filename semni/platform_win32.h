#ifndef PLATFORM_WIN32_H
#define PLATFORM_WIN32_H

#include <windows.h>

void platformInitConsole(void);

HWND platformCreateMainWindow(HINSTANCE hInst, int nShowCmd, WNDPROC wndProc);

HWND platformCreateSliderWindow(HINSTANCE hInst, WNDPROC wndProc);

void platformInitCommonControls(void);

#endif
