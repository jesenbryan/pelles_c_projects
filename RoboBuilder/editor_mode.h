#pragma once

#include <windows.h>

// ---- EDITOR MODE TYPES ----
typedef enum {
    EDITOR_MODE_SEMNI = 0,           // Robot model editor
    EDITOR_MODE_ARCSPLINE = 1,       // Arc-spline segmentation editor
    EDITOR_MODE_COUNT = 2
} EditorMode;

// ---- EDITOR MODE STATE ----
typedef struct {
    EditorMode currentMode;
    EditorMode previousMode;
} EditorModeState;

// The single running instance of the mode switcher's state, defined in
// editor_mode.c. Both canvas.c (Design Mode menu) and main.c's shared
// WndProc read/drive this to decide whether input/paint messages go to
// the ArcSpline canvas or the Semni robot editor.
extern EditorModeState editorModeState;

// ---- FUNCTION DECLARATIONS ----

// Initialize editor mode system. Starts in EDITOR_MODE_ARCSPLINE, since
// that's the app's default startup screen -- the Semni editor is only
// reached via Design Mode > Robot (Semni) in the canvas's Mode menu.
void initEditorModeState(EditorModeState* modeState);

// Switch to a specific editor mode. No-op if already in newMode. Shows/
// hides the two subsystems' windows and child controls via
// applyEditorModeVisibility, and logs the transition the same way the
// rest of the app logs mode changes.
void switchEditorMode(EditorMode newMode, EditorModeState* modeState);

// Get human-readable name for a mode (used for logging).
const wchar_t* getEditorModeName(EditorMode mode);

// Shows the current mode's windows/controls (Semni's buttons + hover
// label, or the ArcSpline canvas's floating UI panel) and hides the
// other's. Called once right after both subsystems' windows/controls
// exist (end of WM_CREATE) and again on every switchEditorMode call.
void applyEditorModeVisibility(EditorModeState* modeState);
