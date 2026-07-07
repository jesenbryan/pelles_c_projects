#pragma once

#include <windows.h>

#include "geometry.h"

#define ID_SAVE_BUTTON 1001

// ---- robot model (the "Semni") ----

typedef struct {
    float headX, buttX;
    float y;

    float headRadius;
    float buttRadius;

    // top/bottom seams, each a circular arc internally tangent to both
    // the head and butt circles (distance between centers == radius
    // difference) -- crease-free by construction instead of by manual
    // placement. Each is parameterized by the ANGLE where it attaches to
    // the head circle (same convention as circleEdge) -- the fillet's
    // radius, center, and its other tangent point on the butt circle are
    // all derived from that angle via a closed-form solve every frame
    // (see geometry.h's filletFromAttachAngle()/internalTangentPoint()).
    // Driving this by the attach angle instead of the radius directly
    // means the drag handle sits exactly at circleEdge(head, headRadius,
    // angle) -- the point IS the parameter, so it tracks the mouse
    // exactly with no drift.
    float topArcAngle;
    float bottomArcAngle;

    Point innerCircle;
    float innerRadius;

    // second joint circle + the two side-arc handles connecting it back
    // to innerCircle, forming a thigh-like limb (like innerCircle is the
    // hip, kneeCircle is the knee)
    Point kneeCircle;
    float kneeRadius;

    Point thighCtrl1;
    Point thighCtrl2;

    // continues the leg past the knee: ankleCircle is the next joint,
    // connected back to kneeCircle by two arcs, same pattern as the thigh
    Point ankleCircle;
    float ankleRadius;

    Point shinCtrl1;
    Point shinCtrl2;

    float angle;      // whole-body rotation

    // hip joint rotation: rotates the whole leg chain (kneeCircle,
    // thighCtrl1/2, ankleCircle, shinCtrl1/2) around innerCircle,
    // independent of the whole-body angle above
    float hipAngle;

    // knee joint rotation: rotates just the shin (ankleCircle,
    // shinCtrl1/2) around kneeCircle, independent of hipAngle/angle
    float kneeAngle;
} Semni;

typedef struct {
    Semni robot;
} RobotScene;

// ---- UI ----

typedef struct {
    HWND hSaveButton;
} UIState;

// ---- application state ----

typedef struct {
    // window + scene
    HWND hwndMain;
    RobotScene robotScene;
    UIState ui;

    // handle interaction: which handle is active, which are being
    // dragged, and which are merely being hovered over
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

    // true while the mouse is merely hovering near a joint circle handle
    // (hip/knee/ankle/head/butt) -- separate from the "dragging" flags
    // above, so those handles can flash yellow just from a hover, while
    // drag-only handles (thigh/shin bulges, seam attach points) keep
    // their dragging-only highlight
    int hoverHip;
    int hoverKnee;
    int hoverAnkle;
    int hoverHead;
    int hoverButt;

    // captured once, when a knee drag starts: ankleCircle/shinCtrl1/2's
    // fixed offset from kneeCircle at that moment. Re-applied fresh every
    // WM_MOUSEMOVE (kneeCircle + offset) instead of nudging them frame by
    // frame, so the shin's length/shape can't drift over a long drag --
    // it's pinned to exactly what it was when the drag began.
    Point kneeDragAnkleOffset;
    Point kneeDragShinCtrl1Offset;
    Point kneeDragShinCtrl2Offset;

    Point mouseGL;
    DWORD lastLogTime;
} AppState;
