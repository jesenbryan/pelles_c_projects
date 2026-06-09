#define UNICODE
#define _UNICODE

#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <commctrl.h>
#include <stdio.h>

#include "geometry.h"
#include "app.h"
#include "mode.h"

#include "environment.h"
#include "paint.h"

#include "robot.h"
#include "renderer.h"
#include "config.h"
#include "input.h"
#include "graphics.h"
#include "ui_controls.h"
#include "platform_win32.h"
#include "app_init.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")

// ---------------- DATA ----------------

AppState app;

// ---------------- WINDOW PROC ----------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return handleInput(hwnd, msg, wParam, lParam, &app);
}

LRESULT CALLBACK SliderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return handleInput(hwnd, msg, wParam, lParam, &app);
}

// ---------------- MAIN ----------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd)
{
	app.lastLogTime = 0;

	platformInitConsole();
	//fflush(stdout);

    platformInitCommonControls();

    initAppState(&app);

	HWND hwnd = platformCreateMainWindow(hInst, nShowCmd, WndProc);

    setupOpenGL(hwnd);

	app.ui.SliderWindow = platformCreateSliderWindow(hInst, WndProc);

    uiCreateControls(app.ui.SliderWindow, hInst, &app);

    MSG msg;

    while (1)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return 0;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        renderApp(&app, graphicsGetHDC());
    }
}
