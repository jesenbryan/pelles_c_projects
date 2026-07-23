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
#include "geometry.h"

// ---------------- SIMULATION / DOMAIN ----------------
#include "robot.h"

// ---------------- RENDER / PLATFORM ----------------
#include "graphics.h"
#include "renderer.h"
#include "platform_win32.h"

// ---------------- INPUT / UI ----------------
#include "input.h"

// ---------------- ARCSPLINE SEGMENTATION PROJECT ----------------
// This app used to be two separate Pelles C projects -- ArcSpline
// Segmentation (canvas.c/ui.c: trace a drawing into arc segments) and
// Semni (app.h/robot.c/input.c/renderer.c: pose a jointed robot figure).
// They now share a single window/process: the ArcSpline canvas is the
// default screen, and picking Design Mode > Robot (Semni) from its Mode
// menu switches the SAME window over to the Semni robot editor (see
// editor_mode.h). Both subsystems' GL drawing, input handling, and child
// controls stay intact; WndProcShared below just routes messages to
// whichever one is currently active.
#include "canvas.h"
#include "ui.h"
#include "editor_mode.h"

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

// ---------------- MENU ----------------

// Builds the "File" / "Mode" menu bar that used to live in the (now
// missing) original ArcSpline Segmentation main.c -- canvas.c's
// WM_COMMAND handler already expected this exact shape (File at index 0,
// Mode at index 1, Design Mode as Mode's first submenu) via its
// GetSubMenu(hMenuBar, 1) / GetSubMenu(hModeMenu, 0) calls, it just never
// had anything building it.
static HMENU buildMainMenu(void)
{
    HMENU hMenuBar = CreateMenu();

    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_UPLOAD, L"Upload Image...");
    AppendMenu(hFileMenu, MF_STRING, ID_SAVE, L"Save...");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");

    HMENU hDesignMenu = CreatePopupMenu();
    AppendMenu(hDesignMenu, MF_STRING, ID_LAYER_ROBOT, L"Robot (Semni)");
    AppendMenu(hDesignMenu, MF_STRING, ID_LAYER_ENVIRONMENT, L"Environment");

    HMENU hModeMenu = CreatePopupMenu();
    AppendMenu(hModeMenu, MF_POPUP, (UINT_PTR)hDesignMenu, L"Design Mode");
    AppendMenu(hModeMenu, MF_STRING, ID_MODE_SIMULATION, L"Simulation");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hModeMenu, L"&Mode");

    // Matches designLayer's actual startup default (LAYER_ENVIRONMENT --
    // see canvas.c) so the menu's check marks agree with reality from the
    // first frame instead of only updating after the user picks something.
    CheckMenuItem(hDesignMenu, ID_LAYER_ROBOT, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(hDesignMenu, ID_LAYER_ENVIRONMENT, MF_BYCOMMAND | MF_CHECKED);

    return hMenuBar;
}

// ---------------- WINDOW PROC ----------------

// Routes messages for the single shared window to whichever subsystem
// currently owns the screen -- WndProcGL (canvas.c, ArcSpline drawing/
// input) or handleInput (input.c, Semni robot editing) -- based on
// editorModeState.currentMode.
//
// A few message types are exceptions to straight mode-gating:
//   - WM_CREATE/WM_SIZE/WM_COMMAND go to BOTH handlers unconditionally.
//     Both are self-guarding (WM_COMMAND dispatches only on IDs each side
//     actually owns, and the two ID sets never overlap -- canvas.h's are
//     single-digit/low IDs, app.h's ID_*_BUTTON are 1001+), so this keeps
//     the menu and both sets of child controls live and correctly sized
//     no matter which mode is showing, without needing this dispatcher to
//     know either subsystem's message-level details.
//   - WM_KEYDOWN with VK_ESCAPE is intercepted here to jump back to the
//     ArcSpline canvas from the Semni editor, since the only other way
//     back in is the Design Mode menu (which is still reachable, but a
//     keyboard shortcut out of a full-screen editor is expected).
//   - WM_DESTROY goes to WndProcGL specifically, since that's the one
//     that owns the hover-panel timer (KillTimer) alongside the usual
//     PostQuitMessage.
LRESULT CALLBACK WndProcShared(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        WndProcGL(hwnd, msg, wParam, lParam);        // sets up the shared GL context + font
        handleInput(hwnd, msg, wParam, lParam, &app); // creates Semni's own child buttons
        applyEditorModeVisibility(&editorModeState);  // hide whichever side isn't active yet
        return 0;

    case WM_SIZE:
        WndProcGL(hwnd, msg, wParam, lParam);
        handleInput(hwnd, msg, wParam, lParam, &app);
        return 0;

    case WM_COMMAND:
        WndProcGL(hwnd, msg, wParam, lParam);
        handleInput(hwnd, msg, wParam, lParam, &app);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && editorModeState.currentMode == EDITOR_MODE_SEMNI)
        {
            switchEditorMode(EDITOR_MODE_ARCSPLINE, &editorModeState);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        // 'P' toggles the exact-pixel-position readout (app.showPixelCoords,
        // drawn by canvas.c's canvasRenderFrame) -- handled here rather than
        // in handleInput/WndProcGL individually so it's a single flag that
        // behaves identically and stays on/off across both Design > Robot
        // and Design > Environment. lParam bit 30 is set on every repeated
        // WM_KEYDOWN while the key is held down, clear only on the very
        // first press -- same repeat-safe-toggle guard canvas.c's Shift+G
        // auto-gravity uses, so holding P doesn't flicker the readout
        // on/off/on/... instead of toggling once per press.
        if (wParam == 'P')
        {
            BOOL isAutoRepeat = (lParam & 0x40000000) != 0;
            if (!isAutoRepeat)
            {
                app.showPixelCoords = !app.showPixelCoords;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        break;

    case WM_DESTROY:
        return WndProcGL(hwnd, msg, wParam, lParam);

    default:
        break;
    }

    // Everything else (paint, mouse, wheel, timer, ...) only goes to
    // whichever subsystem is actually showing right now.
    if (editorModeState.currentMode == EDITOR_MODE_SEMNI)
        return handleInput(hwnd, msg, wParam, lParam, &app);
    else
        return WndProcGL(hwnd, msg, wParam, lParam);
}

// ---------------- MAIN ----------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd)
{
	platformInitConsole();
	//fflush(stdout);

    platformInitCommonControls();

    initAppState(&app);
    initEditorModeState(&editorModeState);

    HMENU hMenuBar = buildMainMenu();

    app.hwndMain = platformCreateMainWindow(hInst, nShowCmd, WndProcShared, hMenuBar);

    // The shared window IS the ArcSpline canvas's GL window now -- set
    // the global canvas.c/ui.c code already expects (see WndProcGL/
    // WndProcUI's `if (hWndGL) InvalidateRect(hWndGL, ...)` calls).
    hWndGL = app.hwndMain;

    createUIWindow(hInst, app.hwndMain);

    // setupOpenGL() leaves the projection at a fixed square ortho
    // (-1.5..1.5 on both axes). The window is 800x600, not square, and
    // that mismatch was only ever getting corrected by WM_SIZE -- which
    // doesn't fire for the window's initial size, only on an actual
    // resize afterward (e.g. maximizing). Until then, what's drawn and
    // where screenToGL thinks the mouse is disagree. renderCombinedFrame
    // reasserts both subsystems' own projections every frame anyway (see
    // its comment), so this doesn't strictly need doing here too, but
    // glWindowWidth/glWindowHeight themselves (read by both projections)
    // do need to be correct before that very first call.
    RECT initialRect;
    GetClientRect(app.hwndMain, &initialRect);
    glWindowWidth = initialRect.right;
    glWindowHeight = initialRect.bottom;

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

        // Both editor subsystems now draw into the same shared canvas every
        // frame -- whichever one is active (editorModeState.currentMode)
        // draws crisp and on top, the other draws dimmed underneath, rather
        // than the two of them taking turns owning the whole window. See
        // canvas.c's renderCombinedFrame for the actual clear/draw/dim/swap
        // sequence; WM_PAINT calls the exact same function for OS-triggered
        // repaints (e.g. window restore), so this is the only place that
        // needs to drive it continuously.
        renderCombinedFrame();
    }
}
