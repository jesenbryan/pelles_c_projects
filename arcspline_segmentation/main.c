#include "ui_state.h"
#include "canvas.h"
#include "ui.h"
#include "debug.h"
#include <stdio.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    OpenConsole();
    printf("Console initialized!\n");
    printf("Draw a curve on the canvas, then click Trace.\n");

    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProcGL;
    wc.hInstance = hInst;
    wc.lpszClassName = L"GLWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);   // NEW: without this, WM_SETCURSOR has
                                                 // nothing to reset to, so a resize
                                                 // cursor picked up near the window
                                                 // border can stay stuck over the canvas
    RegisterClass(&wc);

	WNDCLASS ui = {0};
	ui.lpfnWndProc = WndProcUI;
	ui.hInstance = hInst;
	ui.lpszClassName = L"UIWindow";
	ui.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	ui.hCursor = LoadCursor(NULL, IDC_ARROW);   // NEW: same fix for the control panel
	RegisterClass(&ui);

    hWndGL = CreateWindow(L"GLWindow", L"Canvas", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           100, 100, 800, 600, NULL, NULL, hInst, NULL);
    
    // Create File menu
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, ID_UPLOAD, L"Upload BMP...");
    AppendMenuW(hFileMenu, MF_STRING, ID_SAVE, L"Save BMP...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"File");

    // Mode menu: Design vs Simulation, mutually exclusive (radio-style
    // checkmarks maintained in WndProcGL's WM_COMMAND handler).
    HMENU hModeMenu = CreatePopupMenu();
    AppendMenuW(hModeMenu, MF_STRING | MF_CHECKED, ID_MODE_DESIGN, L"Design Mode");
    AppendMenuW(hModeMenu, MF_STRING, ID_MODE_SIMULATION, L"Simulation Mode");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hModeMenu, L"Mode");

    SetMenu(hWndGL, hMenu);
    
    // NEW: created hidden (no WS_VISIBLE) and WS_EX_LAYERED (so its alpha can
    // be animated) - canvas.c's WM_TIMER hover check in WndProcGL fades this
    // window in/out and repositions it as the cursor moves over the GL
    // window's top-right corner.
    hWndUI = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"UIWindow", L"Control", WS_POPUP,
                       920, 100, 220, 210, NULL, NULL, hInst, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
