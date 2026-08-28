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
    AppendMenu(hDesignMenu, MF_STRING, ID_LAYER_ROBOT, L"Robot");
    AppendMenu(hDesignMenu, MF_STRING, ID_LAYER_ENVIRONMENT, L"Environment");

    HMENU hModeMenu = CreatePopupMenu();
    AppendMenu(hModeMenu, MF_POPUP, (UINT_PTR)hDesignMenu, L"Design");
    AppendMenu(hModeMenu, MF_STRING, ID_MODE_SIMULATION, L"Simulation");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hModeMenu, L"&Mode");

    // Own top-level menu instead of living inside File -- added AFTER Mode
    // (index 2) rather than before it, since canvas.c's WM_COMMAND
    // (ID_LAYER_ROBOT/ID_LAYER_ENVIRONMENT handling) locates Mode/Design
    // via GetSubMenu(hMenuBar, 1)/GetSubMenu(hModeMenu, 0) by fixed index,
    // and inserting anything before Mode would silently shift it to the
    // wrong slot. Same reasoning applies to View just below (index 2) --
    // it also has to land after Mode, not before it.
    //
    // Holds general rendering preferences, as opposed to Mode's "what am I
    // doing right now" choices -- "Hide Inactive Layer" (canvas.c's
    // hideInactiveLayer, WM_COMMAND's ID_TOGGLE_HIDE_INACTIVE handling) is
    // the first thing that belongs here: whether switching between Design >
    // Robot/Environment makes whichever one isn't active disappear
    // entirely, rather than the normal partial dim. On by default (see
    // canvas.c's own hideInactiveLayer comment for why) -- MF_CHECKED here
    // just makes the menu's own initial checkmark agree with that from the
    // first frame; canvas.c's own ID_TOGGLE_HIDE_INACTIVE handler locates
    // this menu via GetSubMenu(hMenuBar, 2) by fixed index, same as Mode/
    // Design above -- inserting anything between Mode and View, or before
    // View, would silently break that lookup too.
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenu(hViewMenu, MF_STRING | MF_CHECKED, ID_TOGGLE_HIDE_INACTIVE, L"Hide Inactive Layer");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, L"&View");

    HMENU hHelpMenu = CreatePopupMenu();
    // Lists every keyboard/mouse control for posing/simulating the robot
    // (see canvas.c's WM_COMMAND ID_HELP handler).
    AppendMenu(hHelpMenu, MF_STRING, ID_HELP, L"Controls Help...");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, L"&Help");

    // Matches designLayer's actual startup default (LAYER_ENVIRONMENT --
    // see canvas.c) so the menu's check marks agree with reality from the
    // first frame instead of only updating after the user picks something.
    CheckMenuItem(hDesignMenu, ID_LAYER_ROBOT, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(hDesignMenu, ID_LAYER_ENVIRONMENT, MF_BYCOMMAND | MF_CHECKED);

    // Matches hideInactiveLayer's own default (TRUE, canvas.c) for the
    // same "checkmark agrees with reality from the first frame" reason as
    // above -- also already set via the AppendMenu's own MF_CHECKED flag
    // just above, this just keeps it explicit/self-documenting the same
    // way ID_LAYER_ENVIRONMENT's own CheckMenuItem call is, rather than
    // relying solely on the AppendMenu flag.
    CheckMenuItem(hViewMenu, ID_TOGGLE_HIDE_INACTIVE, MF_BYCOMMAND | MF_CHECKED);

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
