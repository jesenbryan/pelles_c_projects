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
} RenderState;

void renderApp(AppState* app, HDC hdc);

// Same scene, minus the draggable handles, and without swapping buffers --
// for exporting a frame without the editor's UI overlay. See definition
// in renderer.c for details.
void renderAppForSave(AppState* app);

void drawSemni(Semni b, RenderState* rs, int includeHandles);
