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

HWND platformCreateMainWindow(HINSTANCE hInst, int nShowCmd, WNDPROC wndProc, HMENU hMenu)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"Semni";

    // Without an explicit class cursor, Windows never resets the cursor
    // on its own when it re-enters this window's client area (the canvas)
    // -- it just leaves whatever cursor was showing before, e.g. the
    // diagonal resize arrow left over from dragging the window border.
    // Setting hCursor here makes Windows restore the normal arrow over
    // the client area automatically (via the default WM_SETCURSOR
    // handling), instead of it getting stuck as the resize icon.
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // WS_CLIPCHILDREN keeps the OpenGL SwapBuffers calls (which repaint
    // the whole client rect every frame in the render loop) from being
    // drawn on top of child controls like the Save button -- without it,
    // the button's HWND exists and is WS_VISIBLE, but the constant GL
    // repaint of the parent's client area covers it and it never appears.
    HWND hwnd = CreateWindow(
        L"Semni",
        L"Semni",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, hMenu,
        hInst, NULL
    );

    ShowWindow(hwnd, nShowCmd);
    return hwnd;
}
