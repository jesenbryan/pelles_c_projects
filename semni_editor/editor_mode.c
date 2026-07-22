#include "editor_mode.h"
#include "app.h"      // for the global `app` (Semni's own child controls live in app.ui)
#include "canvas.h"   // for hWndUI (the ArcSpline canvas's floating tool panel)
#include <stdio.h>

EditorModeState editorModeState;

void initEditorModeState(EditorModeState* modeState)
{
    if (!modeState) return;

    // Starts on the ArcSpline canvas -- that's the app's default startup
    // screen. The Semni robot editor is reached via Design Mode > Robot
    // (Semni) in the canvas's Mode menu (see canvas.c's WM_COMMAND).
    modeState->currentMode = EDITOR_MODE_ARCSPLINE;
    modeState->previousMode = EDITOR_MODE_ARCSPLINE;
}

void switchEditorMode(EditorMode newMode, EditorModeState* modeState)
{
    if (!modeState || newMode >= EDITOR_MODE_COUNT) return;

    if (modeState->currentMode == newMode) return;

    modeState->previousMode = modeState->currentMode;
    modeState->currentMode = newMode;

    applyEditorModeVisibility(modeState);

    printf("[EditorMode] Switched to: %ls (was: %ls)\n",
           getEditorModeName(newMode),
           getEditorModeName(modeState->previousMode));
}

const wchar_t* getEditorModeName(EditorMode mode)
{
    switch (mode)
    {
        case EDITOR_MODE_SEMNI:
            return L"Semni Robot";
        case EDITOR_MODE_ARCSPLINE:
            return L"ArcSpline";
        default:
            return L"Unknown";
    }
}

void applyEditorModeVisibility(EditorModeState* modeState)
{
    if (!modeState) return;

    BOOL semniActive = (modeState->currentMode == EDITOR_MODE_SEMNI);

    // Semni's own child controls (children of the shared window) --
    // created once in handleInput's WM_CREATE and just shown/hidden here,
    // never re-created.
    if (app.ui.hControlPanel)
        ShowWindow(app.ui.hControlPanel, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hPanelTitle)
        ShowWindow(app.ui.hPanelTitle, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hRobotSelector)
        ShowWindow(app.ui.hRobotSelector, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hStandingPositionButton)
        ShowWindow(app.ui.hStandingPositionButton, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hHomePositionButton)
        ShowWindow(app.ui.hHomePositionButton, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hSetStandingButton)
        ShowWindow(app.ui.hSetStandingButton, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hSetHomeButton)
        ShowWindow(app.ui.hSetHomeButton, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hSaveButton)
        ShowWindow(app.ui.hSaveButton, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hMirrorButton)
        ShowWindow(app.ui.hMirrorButton, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hMirrorButton2)
        ShowWindow(app.ui.hMirrorButton2, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hScaleLabel)
        ShowWindow(app.ui.hScaleLabel, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hScaleSlider)
        ShowWindow(app.ui.hScaleSlider, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hViewSegmentsButton)
        ShowWindow(app.ui.hViewSegmentsButton, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hDebugLogButton)
        ShowWindow(app.ui.hDebugLogButton, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hHoverPanel)
        ShowWindow(app.ui.hHoverPanel, semniActive ? SW_SHOW : SW_HIDE);
    if (app.ui.hHoverLabel)
        ShowWindow(app.ui.hHoverLabel, semniActive ? SW_SHOW : SW_HIDE);

    // The ArcSpline canvas's floating tool panel manages its own show/fade
    // via the hover hot-zone timer (see canvas.c's WM_TIMER), but force it
    // hidden immediately when entering Semni mode so it doesn't linger on
    // screen mid-fade over the robot editor.
    if (semniActive && hWndUI)
        ShowWindow(hWndUI, SW_HIDE);
}
