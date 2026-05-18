#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

HWND hSlider, hButton;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(icex);
        icex.dwICC = ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        // ===== SLIDER =====
        hSlider = CreateWindowEx(
            0,
            TRACKBAR_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            50, 50, 400, 50,
            hWnd,
            NULL,
            GetModuleHandle(NULL),
            NULL
        );

        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessage(hSlider, TBM_SETPOS, TRUE, 50);

        // ===== BUTTON =====
        hButton = CreateWindowEx(
            0,
            L"BUTTON",
            L"Click Me",
            WS_CHILD | WS_VISIBLE,
            50, 120, 120, 40,
            hWnd,
            (HMENU)1,   // button ID
            GetModuleHandle(NULL),
            NULL
        );

        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == 1) // button clicked
        {
            MessageBox(hWnd, L"Button clicked!", L"Info", MB_OK);
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MyWindow";

    RegisterClass(&wc);

    HWND hWnd = CreateWindow(
        L"MyWindow",
        L"Windows",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 250,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(hWnd, nShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
