#pragma once

#include "ui_state.h"

LRESULT CALLBACK WndProcGL(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateProjection(void);

// The HDC of the single shared OpenGL window, set up once in WndProcGL's
// WM_CREATE handler. Exposed so the Semni robot editor (which shares the
// same window/GL context when the app is in Editor Mode: Semni) can pass
// it to renderApp() for SwapBuffers, instead of creating its own separate
// GL context via graphics.c's setupOpenGL().
HDC canvasGetHDC(void);

// Draws the ArcSpline canvas scene (sets its own projection via
// UpdateProjection first) into whatever's already in the color buffer --
// no clear, no swap -- so renderCombinedFrame can composite it alongside
// the Semni robot scene in the same frame. dimAmount scales every draw
// call's own alpha (opacity = 1 - dimAmount) rather than covering the
// finished scene with a full-screen overlay -- that would also darken the
// shared white canvas background underneath it, making an otherwise-empty
// canvas look grayed out everywhere instead of just its own strokes/lines
// fading. Used for when the ArcSpline canvas isn't the currently active
// editor mode but should still show as background context rather than
// disappearing. Also used directly by WM_PAINT's real drawing work via
// renderCombinedFrame.
void canvasRenderFrame(float dimAmount);

// Draws ONE combined frame: clears the color buffer once, then draws both
// editor subsystems (ArcSpline canvas via canvasRenderFrame, Semni robot
// editor via renderRobotScene) -- whichever one is currently active
// (editorModeState.currentMode) draws at full opacity on top, the other
// draws dimmed underneath, so switching Design Mode just changes which one
// is crisp/interactive instead of hiding the other outright. Ends with one
// SwapBuffers call. Called every iteration of main.c's message loop, and
// also from WM_PAINT so an OS-triggered repaint (e.g. window restore)
// still shows the correct combined frame.
void renderCombinedFrame(void);
