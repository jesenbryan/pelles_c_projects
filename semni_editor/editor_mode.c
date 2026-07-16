#include "editor_mode.h"
#include <stdio.h>
#include <commctrl.h>

// Button ID for editor mode switcher
#define ID_EDITOR_MODE_BUTTON 2001

void initEditorModeState(EditorModeState* modeState)
{
    if (!modeState) return;
    
    modeState->currentMode = EDITOR_MODE_SEMNI;  // Start with Semni editor
    modeState->previousMode = EDITOR_MODE_SEMNI;
    
    modeState->hEditorModeButton = NULL;
    modeState->hModeIndicatorLabel = NULL;
    
    modeState->showSemniUI = 1;      // Show Semni UI by default
    modeState->showCanvasUI = 0;     // Hide Canvas UI by default
}

void switchEditorMode(EditorMode newMode, EditorModeState* modeState)
{
    if (!modeState || newMode >= EDITOR_MODE_COUNT) return;
    
    if (modeState->currentMode == newMode) return;
    
    modeState->previousMode = modeState->currentMode;
    modeState->currentMode = newMode;
    
    // Update visibility flags
    if (newMode == EDITOR_MODE_SEMNI)
    {
        modeState->showSemniUI = 1;
        modeState->showCanvasUI = 0;
    }
    else if (newMode == EDITOR_MODE_ARCSPLINE)
    {
        modeState->showSemniUI = 0;
        modeState->showCanvasUI = 1;
    }
    
    // Update button text to show current mode
    if (modeState->hEditorModeButton)
    {
        const wchar_t* modeName = getEditorModeName(newMode);
        wchar_t buttonText[256];
        wsprintf(buttonText, L"Editor: %s", modeName);
        SetWindowText(modeState->hEditorModeButton, buttonText);
    }
    
    printf("[EditorMode] Switched to: %s (was: %s)\n", 
           getEditorModeName(newMode),
           getEditorModeName(modeState->previousMode));
}

void toggleEditorMode(EditorModeState* modeState)
{
    if (!modeState) return;
    
    EditorMode nextMode = (modeState->currentMode == EDITOR_MODE_SEMNI) 
        ? EDITOR_MODE_ARCSPLINE 
        : EDITOR_MODE_SEMNI;
    
    switchEditorMode(nextMode, modeState);
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

void createEditorModeButton(HWND hwndParent, EditorModeState* modeState)
{
    if (!modeState) return;
    
    // Create the mode switcher button
    modeState->hEditorModeButton = CreateWindow(
        L"BUTTON",
        L"Editor: Semni Robot",
        WS_VISIBLE | WS_CHILD,
        260, 10, 150, 30,
        hwndParent,
        (HMENU)ID_EDITOR_MODE_BUTTON,
        NULL,
        NULL
    );
    
    if (!modeState->hEditorModeButton)
    {
        printf("[EditorMode] Failed to create button\n");
    }
    else
    {
        printf("[EditorMode] Button created successfully\n");
    }
}

void updateEditorModeButtonLayout(HWND hwnd, EditorModeState* modeState)
{
    if (!modeState || !modeState->hEditorModeButton) return;
    
    RECT rect;
    GetClientRect(hwnd, &rect);
    
    // Position editor mode button at top, after existing buttons
    // (Standing and Home buttons are at 10,10 and 140,10)
    int xEditorButton = 260;
    int yTop = 10;
    
    SetWindowPos(modeState->hEditorModeButton, NULL,
         xEditorButton, yTop, 0, 0,
         SWP_NOZORDER | SWP_NOSIZE);
}

void updateEditorModeUIVisibility(EditorModeState* modeState,
                                   HWND hSemniButton1, HWND hSemniButton2,
                                   HWND hSemniButton3, HWND hSemniButton4,
                                   HWND hCanvasButton1, HWND hCanvasButton2)
{
    if (!modeState) return;
    
    // Show/hide Semni-specific buttons
    if (hSemniButton1)
        ShowWindow(hSemniButton1, modeState->showSemniUI ? SW_SHOW : SW_HIDE);
    if (hSemniButton2)
        ShowWindow(hSemniButton2, modeState->showSemniUI ? SW_SHOW : SW_HIDE);
    if (hSemniButton3)
        ShowWindow(hSemniButton3, modeState->showSemniUI ? SW_SHOW : SW_HIDE);
    if (hSemniButton4)
        ShowWindow(hSemniButton4, modeState->showSemniUI ? SW_SHOW : SW_HIDE);
    
    // Show/hide Canvas/Arc-spline buttons
    if (hCanvasButton1)
        ShowWindow(hCanvasButton1, modeState->showCanvasUI ? SW_SHOW : SW_HIDE);
    if (hCanvasButton2)
        ShowWindow(hCanvasButton2, modeState->showCanvasUI ? SW_SHOW : SW_HIDE);
    
    printf("[EditorMode] UI visibility updated for mode: %s\n",
           getEditorModeName(modeState->currentMode));
}

// Get the button ID for mode switcher (used in input handling)
int getEditorModeButtonID(void)
{
    return ID_EDITOR_MODE_BUTTON;
}
