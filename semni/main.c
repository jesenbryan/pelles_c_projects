#define UNICODE
#define _UNICODE

// ---------------- SYSTEM HEADERS ----------------
#include <windows.h>
#include <stdio.h>
#include <math.h>

// ---------------- OPENGL / WIN32 EXT ----------------
#include <GL/gl.h>
#include <commctrl.h>

// ---------------- PROJECT CORE ----------------
#include "app.h"
#include "config.h"
#include "mode.h"
#include "geometry.h"

// ---------------- SIMULATION / DOMAIN ----------------
#include "robot.h"
#include "environment.h"
#include "paint.h"

// ---------------- RENDER / PLATFORM ----------------
#include "graphics.h"
#include "renderer.h"
#include "platform_win32.h"

// ---------------- INPUT / UI ----------------
#include "input.h"
#include "ui_controls.h"

// ---------------- APP LIFECYCLE ----------------
#include "app_init.h"

// Win32 core
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// OpenGL
#pragma comment(lib, "opengl32.lib")

// UI controls
#pragma comment(lib, "comctl32.lib")

// ---------------- APPLICATION STATE ----------------

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
