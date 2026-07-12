#pragma once

#include <windows.h>
#include "app.h"

typedef struct {
    int activeHandle;
    int draggingTopArc;
    int draggingBottomArc;
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

    // hover + Shift held -- true exactly when a scroll would rotate that
    // joint, drives the rotating-part (thigh/shin) preview highlight
    int hoverHipRotate;
    int hoverKneeRotate;
} RenderState;

void renderApp(AppState* app, HDC hdc);

// Same scene, minus the draggable handles, and without swapping buffers --
// for exporting a frame without the editor's UI overlay. See definition
// in renderer.c for details.
void renderAppForSave(AppState* app);

void drawSemni(Semni b, RenderState* rs, int includeHandles);
