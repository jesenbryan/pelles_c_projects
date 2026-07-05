#pragma once

#include "app.h"
#include <windows.h>

LRESULT handleInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, AppState* app);
