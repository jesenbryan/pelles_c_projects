#pragma once

#include <windows.h>

#include "geometry.h"

#define ID_SAVE_BUTTON 1001
#define ID_MIRROR_LEG_BUTTON 1002

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

    // the two thigh seams, both parameterized by the angle where they
    // attach to innerCircle (same circleEdge/filletFromAttachAngle
    // convention as topArcAngle/bottomArcAngle), with the fillet's
    // radius, center, and other tangent point (on kneeCircle) derived
    // from that angle every frame. They're NOT the same kind of curve,
    // though: thighArc1Angle uses the usual convex construction
    // (filletFromAttachAngle) and bulges outward, away from the hip-knee
    // axis, same as top/bottom. thighArc2Angle uses the concave
    // construction (filletFromAttachAngleConcave) instead and pinches
    // inward, toward the hip-knee axis -- its safe range sits on the
    // opposite side of innerCircle from thighArc1Angle's (facing
    // kneeCircle rather than away from it), so the two no longer share a
    // degenerate point to stay clear of and don't need the "each stays on
    // its own side" locking top/bottom and thighArc1Angle still use (see
    // THIGH_ARC_SIDE_MARGIN_DEG) -- they already can't collide.
    float thighArc1Angle;
    float thighArc2Angle;

    // continues the leg past the knee: ankleCircle is the next joint,
    // connected back to kneeCircle by two arcs, same pattern as the thigh
    Point ankleCircle;
    float ankleRadius;

    // the two shin seams, both parameterized by the angle where they
    // attach to kneeCircle (same circleEdge/filletFromAttachAngle
    // convention as the thigh pair above), with the fillet's radius,
    // center, and other tangent point (on ankleCircle) derived from that
    // angle every frame. Same convex/concave split as thighArc1Angle/
    // thighArc2Angle: shinArc1Angle uses the usual convex construction
    // (filletFromAttachAngle) and bulges outward, away from the
    // knee-ankle axis. shinArc2Angle uses the concave construction
    // (filletFromAttachAngleConcave) instead and pinches inward, toward
    // the knee-ankle axis -- its safe range sits on the opposite side of
    // kneeCircle from shinArc1Angle's, so the two don't share a
    // degenerate point to stay clear of and drag independently with no
    // side-locking needed between them.
    float shinArc1Angle;
    float shinArc2Angle;

    float angle;      // whole-body rotation

    // hip joint rotation: rotates the whole leg chain (kneeCircle,
    // ankleCircle) around innerCircle, independent of the whole-body angle
    // above. thighArc1Angle/thighArc2Angle/shinArc1Angle/shinArc2Angle
    // don't need rotating themselves -- they're angles measured in their
    // own joint's local frame (around innerCircle or kneeCircle), so
    // hipAngle rotating that whole frame at render/hit-test time carries
    // them along automatically.
    float hipAngle;

    // knee joint rotation: rotates just the shin (ankleCircle) around
    // kneeCircle, independent of hipAngle/angle
    float kneeAngle;
} Semni;

typedef struct {
    Semni robot;
} RobotScene;

// ---- UI ----

typedef struct {
    HWND hSaveButton;
    HWND hMirrorButton;
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

    // true only while hovering the hip/knee handle AND Shift is held --
    // i.e. exactly when a scroll would actually rotate that joint. Drives
    // the "rotating part" (thigh/shin) preview highlight, kept separate
    // from hoverHip/hoverKnee above so the handle's own drag-affordance
    // highlight still lights up on a plain hover, without Shift
    int hoverHipRotate;
    int hoverKneeRotate;

    // captured once, when a knee drag starts: ankleCircle's fixed offset
    // from kneeCircle at that moment. Re-applied fresh every WM_MOUSEMOVE
    // (kneeCircle + offset) instead of nudging it frame by frame, so the
    // shin's length can't drift over a long drag -- it's pinned to exactly
    // what it was when the drag began. shinArc1Angle/shinArc2Angle need no
    // equivalent offset -- like thighArc1Angle/thighArc2Angle, they're
    // already knee-frame-relative angles, unaffected by moving kneeCircle
    // itself.
    Point kneeDragAnkleOffset;

    // same idea, but for a hip drag: the whole leg (kneeCircle,
    // ankleCircle) hangs off innerCircle, so moving the hip needs to carry
    // all of it along as one rigid piece -- each point's fixed offset from
    // innerCircle is captured when the hip drag starts, then reapplied
    // fresh every WM_MOUSEMOVE (innerCircle + offset), so the whole leg's
    // shape/pose is preserved exactly instead of the rest of the leg
    // staying behind while just the hip circle moves. thighArc1Angle/
    // thighArc2Angle/shinArc1Angle/shinArc2Angle don't need an offset here
    // -- they're already relative to their own joint (hip or knee), so a
    // hip drag doesn't change any of them directly.
    Point hipDragKneeOffset;
    Point hipDragAnkleOffset;

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

    // same idea again, for the shin arcs -- perpendicular offset measured
    // against the knee->ankle axis instead of hip->knee. Only one of
    // shinArc1Angle/shinArc2Angle can be dragged at a time, so they share
    // these two fields too.
    float shinArcDragStartPerp;
    float shinArcDragStartAngle;

    Point mouseGL;
    DWORD lastLogTime;
} AppState;
