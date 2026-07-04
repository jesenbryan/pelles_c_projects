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
    RegisterClass(&wc);

    WNDCLASS ui = {0};
    ui.lpfnWndProc = WndProcUI;
    ui.hInstance = hInst;
    ui.lpszClassName = L"UIWindow";
    RegisterClass(&ui);

    hWndGL = CreateWindow(L"GLWindow", L"Canvas", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           100, 100, 800, 600, NULL, NULL, hInst, NULL);
    hWndUI = CreateWindow(L"UIWindow", L"Control", WS_OVERLAPPED | WS_VISIBLE,
                           920, 100, 360, 210, NULL, NULL, hInst, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
