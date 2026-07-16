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
    
    // UI elements for mode switcher
    HWND hEditorModeButton;
    HWND hModeIndicatorLabel;
    
    // Mode-specific state visibility flags
    int showSemniUI;      // Show/hide Semni controls
    int showCanvasUI;     // Show/hide Canvas/Arc controls
} EditorModeState;

// ---- FUNCTION DECLARATIONS ----

// Initialize editor mode system
void initEditorModeState(EditorModeState* modeState);

// Switch to a specific editor mode
void switchEditorMode(EditorMode newMode, EditorModeState* modeState);

// Toggle between the two modes
void toggleEditorMode(EditorModeState* modeState);

// Get human-readable name for a mode
const wchar_t* getEditorModeName(EditorMode mode);

// Create UI button for mode switching
void createEditorModeButton(HWND hwndParent, EditorModeState* modeState);

// Update button positions on window resize
void updateEditorModeButtonLayout(HWND hwnd, EditorModeState* modeState);

// Show/hide UI elements based on current mode
void updateEditorModeUIVisibility(EditorModeState* modeState, 
                                   HWND hSemniButton1, HWND hSemniButton2, 
                                   HWND hSemniButton3, HWND hSemniButton4,
                                   HWND hCanvasButton1, HWND hCanvasButton2);
