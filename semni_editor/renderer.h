#pragma once

#include <windows.h>
#include "app.h"

typedef struct {
    int activeHandle;
    int draggingSeamArc1;
    int draggingSeamArc2;
    int draggingInner;
    int draggingKnee;
    int draggingThigh1;
    int draggingThigh2;
    int draggingAnkle;
    int draggingShin1;
    int draggingShin2;

    // hover-only state for the joint circle handles (hip/knee/ankle/
    // head/butt), so they can highlight from a hover, not just a drag
    int hoverHip;
    int hoverKnee;
    int hoverAnkle;
    int hoverHead;
    int hoverButt;

    // Shift held during the most recent mouse move -- combined with
    // hoverHip to preview the shift+scroll hip rotation's reach (thigh,
    // knee, shin, foot) in blue before any rotation actually happens
    int shiftHeld;

    // "View Segments" toggle (app->showCircleSegments) -- when set,
    // drawSemni also overlays the full circle each fillet arc (seam/
    // thigh/shin) was trimmed from. See drawSemniCircleSegments below.
    int showSegments;
} RenderState;

void renderApp(AppState* app, HDC hdc);

// Same scene, minus the draggable handles, and without swapping buffers --
// for exporting a frame without the editor's UI overlay. See definition
// in renderer.c for details.
void renderAppForSave(AppState* app);

// Draws the robot scene (with handles) without clearing the color buffer
// or swapping buffers, so it can be composited into a single shared frame
// alongside the ArcSpline canvas -- see main.c/canvas.c's renderCombinedFrame.
// Assumes the caller has already set Semni's own projection (graphicsOnResize)
// and enabled GL_BLEND. dimAmount scales every draw call's own alpha (0 =
// full opacity, up to 1 = heavily dimmed) so this scene can be shown as
// dimmed background context when Semni isn't the currently active editor
// mode, without a full-screen overlay that would also darken the shared
// white canvas background underneath it.
void renderRobotScene(AppState* app, float dimAmount);

void drawSemni(Semni b, RenderState* rs, int includeHandles, float opacity);

// Overlays the full circle behind each fillet arc (seam1/2, thigh1/2,
// shin1/2) -- every curve on Semni is genuinely just an arc trimmed from
// some circle (see app.h's seamArc1Angle comment), so this makes that
// literally visible, the same idea as the ArcSpline canvas's own View
// Segments overlay (canvas.c). The five body circles (head, butt, hip,
// knee, ankle) aren't included here since drawSemniBody/drawThigh/drawShin
// already draw those in full unconditionally.
void drawSemniCircleSegments(Semni b, float opacity);
