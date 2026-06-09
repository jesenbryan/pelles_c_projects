#define UNICODE
#define _UNICODE

#include <windows.h>
#include "platform_win32.h"
#include <stdio.h>
#include <commctrl.h>

void platformInitConsole(void)
{
    AllocConsole();

    if (!freopen("CONOUT$", "w", stdout))
    {
        MessageBox(NULL, L"stdout redirect failed", L"Error", MB_OK);
    }

    if (!freopen("CONOUT$", "w", stderr))
    {
        MessageBox(NULL, L"stderr redirect failed", L"Error", MB_OK);
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("Console initialized\n");
}

void platformInitCommonControls(void)
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
}

HWND platformCreateMainWindow(HINSTANCE hInst, int nShowCmd, WNDPROC wndProc)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = wndProc; 
    wc.hInstance = hInst;
    wc.lpszClassName = L"Semni";

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        L"Semni",
        L"Semni",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL,
        hInst, NULL
    );

    ShowWindow(hwnd, nShowCmd);
    return hwnd;
}

HWND platformCreateSliderWindow(HINSTANCE hInst, WNDPROC wndProc)
{
    WNDCLASS wcSlider = {0};
    wcSlider.lpfnWndProc = wndProc;
    wcSlider.hInstance = hInst;
    wcSlider.lpszClassName = L"SliderWin";

    RegisterClass(&wcSlider);

    HWND hwnd = CreateWindow(
        L"SliderWin",
        L"Controls",
        WS_OVERLAPPEDWINDOW,
        850, 100,
        400, 350,
        NULL, NULL,
        hInst, NULL
    );

    ShowWindow(hwnd, SW_SHOW);
    return hwnd;
}
