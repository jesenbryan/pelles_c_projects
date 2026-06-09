#define UNICODE
#define _UNICODE

#include "ui_controls.h"
#include <windows.h>
#include <commctrl.h>

#define ID_LEFT  101
#define ID_RIGHT 102



HWND createSliderWindow(HINSTANCE hInst)
{
    HWND hwndSlider = CreateWindow(
        L"SliderWin",
        L"Controls",
        WS_OVERLAPPEDWINDOW,
        850, 100,
        400, 350,
        NULL, NULL,
        hInst, NULL
    );

    ShowWindow(hwndSlider, SW_SHOW);
    return hwndSlider;
}

void uiCreateControls(HWND hwndSlider, HINSTANCE hInst, AppState* app)
{
    HWND labelLeft = CreateWindow(
        L"STATIC",
        L"Left Circle Radius",
        WS_CHILD | WS_VISIBLE,
        20, 0, 200, 20,
        hwndSlider,
        NULL,
        hInst,
        NULL
    );

	app->ui.labelLeft = labelLeft; //tbd
	
    HWND sliderLeft = CreateWindowEx(
        0, TRACKBAR_CLASS, L"Left Radius",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        20, 20, 240, 40,
        hwndSlider,
        (HMENU)ID_LEFT,
        hInst,
        NULL
    );

    app->ui.sliderLeft = sliderLeft;

    SendMessage(sliderLeft, TBM_SETRANGE, TRUE, MAKELPARAM(100, 300));
    SendMessage(sliderLeft, TBM_SETPOS, TRUE, 200);

    HWND labelRight = CreateWindow(
        L"STATIC",
        L"Right Circle Radius",
        WS_CHILD | WS_VISIBLE,
        20, 60, 200, 20,
        hwndSlider,
        NULL,
        hInst,
        NULL
    );

	app->ui.labelRight = labelRight;

    HWND sliderRight = CreateWindowEx(
        0, TRACKBAR_CLASS, L"Right Radius",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        20, 80, 240, 40,
        hwndSlider,
        (HMENU)ID_RIGHT,
        hInst,
        NULL
    );

    app->ui.sliderRight = sliderRight;

    SendMessage(sliderRight, TBM_SETRANGE, TRUE, MAKELPARAM(100, 300));
    SendMessage(sliderRight, TBM_SETPOS, TRUE, 200);
}
