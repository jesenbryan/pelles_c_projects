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

    // the two thigh seams: same construction as topArcAngle/bottomArcAngle
    // above, just between innerCircle (hip) and kneeCircle (knee) instead
    // of head and butt -- each is a circular arc internally tangent to
    // both joint circles, parameterized by the angle where it attaches to
    // innerCircle (same convention as circleEdge/filletFromAttachAngle),
    // with the fillet's radius, center, and other tangent point (on
    // kneeCircle) all derived from that angle every frame. Unlike
    // top/bottom, these two aren't kept mirrored -- thighArc1Angle stays
    // on its own fixed side of the hip-knee axis (see THIGH_ARC_SIDE_
    // MARGIN_DEG) and thighArc2Angle the opposite side, but each drags
    // independently instead of updating in lockstep, since the thigh's
    // two sides aren't meant to bulge symmetrically.
    float thighArc1Angle;
    float thighArc2Angle;

    // continues the leg past the knee: ankleCircle is the next joint,
    // connected back to kneeCircle by two arcs, same pattern as the thigh
    Point ankleCircle;
    float ankleRadius;

    Point shinCtrl1;
    Point shinCtrl2;

    float angle;      // whole-body rotation

    // hip joint rotation: rotates the whole leg chain (kneeCircle,
    // ankleCircle, shinCtrl1/2) around innerCircle, independent of the
    // whole-body angle above. thighArc1Angle/thighArc2Angle don't need
    // rotating themselves -- they're angles measured in the hip's own
    // local frame (around innerCircle), so hipAngle rotating that whole
    // frame at render/hit-test time carries them along automatically.
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

    // same idea, but for a hip drag: the whole leg (kneeCircle,
    // ankleCircle, shinCtrl1/2) hangs off innerCircle, so moving the hip
    // needs to carry all of it along as one rigid piece -- each point's
    // fixed offset from innerCircle is captured when the hip drag starts,
    // then reapplied fresh every WM_MOUSEMOVE (innerCircle + offset), so
    // the whole leg's shape/pose is preserved exactly instead of the rest
    // of the leg staying behind while just the hip circle moves.
    // thighArc1Angle/thighArc2Angle don't need an offset here -- they're
    // already hip-frame-relative angles, so a hip drag doesn't change them
    // at all.
    Point hipDragKneeOffset;
    Point hipDragAnkleOffset;
    Point hipDragShinCtrl1Offset;
    Point hipDragShinCtrl2Offset;

    // captured once, when a seam arc drag starts: the mouse's local Y and
    // the arc's angle at that moment. Each WM_MOUSEMOVE then sets the
    // angle to (start angle + scaled Y delta since the drag began)
    // instead of solving for an exact angle from the absolute mouse
    // position -- see ARC_DRAG_SENSITIVITY_DEG_PER_UNIT's comment in
    // config.h for why. Only one arc can be dragged at a time, so top and
    // bottom share these two fields.
    float arcDragStartMouseY;
    float arcDragStartAngle;

    // same idea as arcDragStartMouseY/arcDragStartAngle, for the thigh
    // arcs -- except the hip->knee axis isn't fixed horizontal like the
    // head-butt axis (it rotates with hipAngle and the user can pose it
    // any direction), so the drag reads the mouse's perpendicular-to-axis
    // offset (geometry.h's perpOffsetOnAxis) instead of raw screen Y. Only
    // one of thighArc1Angle/thighArc2Angle can be dragged at a time, so
    // they share these two fields too.
    float thighArcDragStartPerp;
    float thighArcDragStartAngle;

    Point mouseGL;
    DWORD lastLogTime;
} AppState;
