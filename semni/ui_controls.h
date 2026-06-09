#ifndef UI_CONTROLS_H
#define UI_CONTROLS_H

#include <windows.h>
#include "app.h"

HWND createSliderWindow(HINSTANCE hInst);
void uiCreateControls(HWND parent, HINSTANCE hInst, AppState* app);

#endif
