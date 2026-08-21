#pragma once

#include <windows.h>

#include "geometry.h"

#define ID_SAVE_BUTTON 1001
#define ID_MIRROR_LEG_BUTTON 1002
#define ID_STANDING_POSITION_BUTTON 1003
#define ID_HOME_POSITION_BUTTON 1004

// ---- robot model (the "Semni") ----

typedef struct {
    float headX, buttX;
    float y;

    float headRadius;
    float buttRadius;

    // seam arc 1/2 (formerly "top"/"bottom"): each a bezier curve between
    // the head and butt circles, with NO tangency constraint at all. Each
    // circle's attach point is fixed at exactly a quarter turn (90
    // degrees) around from the head-butt axis (see geometry.h's
    // circleHalfPoint) -- seam arc 1 uses one perpendicular side, seam arc
    // 2 the other, so together the two arcs split each circle into exact
    // halves. The only free parameter left is where the curve's control
    // point sits: seamArc1Bulge/seamArc2Bulge is that control point's
    // signed perpendicular distance from the head-butt axis midpoint (see
    // geometry.h's axisBulgePoint) -- positive/negative picks which side,
    // clamped by config.h's MIN_ARC_BULGE/MAX_ARC_BULGE so the drag handle
    // can't flatten the seam to a line or flip it across to the other
    // arc's side.
    //
    // Named seamArc1Bulge/seamArc2Bulge to match the thighArc1Bulge/
    // thighArc2Bulge/shinArc1Bulge/shinArc2Bulge convention below
    // ([BodyPart]Arc[N]Bulge, "Arc" trailing), rather than arcSeam1/2.
    float seamArc1Bulge;
    float seamArc2Bulge;

    Point innerCircle;
    float innerRadius;

    // second joint circle + the two side-arc handles connecting it back
    // to innerCircle, forming a thigh-like limb (like innerCircle is the
    // hip, kneeCircle is the knee)
    Point kneeCircle;
    float kneeRadius;

    // the two thigh seams, same fixed-attach-point construction as
    // seamArc1Bulge/seamArc2Bulge above, just between innerCircle and
    // kneeCircle instead of head/butt: each circle's attach point sits a
    // fixed quarter turn around from the hip-knee axis (circleHalfPoint),
    // and thighArc1Bulge/thighArc2Bulge is the curve's control point's
    // signed perpendicular distance from the hip-knee axis midpoint
    // (axisBulgePoint). thighArc1Bulge bulges outward, away from the
    // body, continuing the leg's outer silhouette (same role seam arc 1/2
    // play for the torso); thighArc2Bulge sits on the opposite
    // perpendicular side and pinches inward, toward the crotch. Each is
    // clamped to its own sign (config.h's MIN/MAX_THIGH_ARC_BULGE for arc
    // 1, MIN/MAX_THIGH_ARC2_BULGE for arc 2) so dragging one can't cross
    // over into the other's side.
    float thighArc1Bulge;
    float thighArc2Bulge;

    // continues the leg past the knee: ankleCircle is the next joint,
    // connected back to kneeCircle by two arcs, same pattern as the thigh
    Point ankleCircle;
    float ankleRadius;

    // the two shin seams, same fixed-attach-point construction as the
    // thigh pair above, just between kneeCircle and ankleCircle instead
    // of innerCircle/kneeCircle. shinArc1Bulge bulges outward, away from
    // the knee-ankle axis (the shin's outer silhouette); shinArc2Bulge
    // sits on the opposite perpendicular side and pinches inward. Same
    // sign-locked clamping as the thigh pair (config.h's MIN/
    // MAX_SHIN_ARC_BULGE and MIN/MAX_SHIN_ARC2_BULGE).
    float shinArc1Bulge;
    float shinArc2Bulge;

    float angle;      // whole-body rotation

    // hip joint rotation: rotates the whole leg chain (kneeCircle,
    // ankleCircle) around innerCircle, independent of the whole-body angle
    // above. thighArc1Bulge/thighArc2Bulge/shinArc1Bulge/shinArc2Bulge
    // don't need rotating themselves -- they're perpendicular distances
    // measured in their own joint's local frame (around the
    // innerCircle<->kneeCircle or kneeCircle<->ankleCircle axis), so
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
    HWND hStandingPositionButton;
    HWND hHomePositionButton;

    // bottom-left status label -- shows the friendly name of whichever
    // handle the mouse is currently hovering (e.g. "Thigh Arc 1"), blank
    // when nothing is hovered. Updated every WM_MOUSEMOVE.
    HWND hHoverLabel;
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

    int draggingSeamArc1;
    int draggingSeamArc2;
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
    int hoverButt;
    int hoverHead;

    // captured once, when a knee drag starts: ankleCircle's fixed offset
    // from kneeCircle at that moment. Re-applied fresh every WM_MOUSEMOVE
    // (kneeCircle + offset) instead of nudging it frame by frame, so the
    // shin's length can't drift over a long drag -- it's pinned to exactly
    // what it was when the drag began. shinArc1Bulge/shinArc2Bulge need no
    // equivalent offset -- like thighArc1Bulge/thighArc2Bulge, they're
    // already knee-frame-relative distances, unaffected by moving
    // kneeCircle itself.
    Point kneeDragAnkleOffset;

    // same idea, but for a hip drag: the whole leg (kneeCircle,
    // ankleCircle) hangs off innerCircle, so moving the hip needs to carry
    // all of it along as one rigid piece -- each point's fixed offset from
    // innerCircle is captured when the hip drag starts, then reapplied
    // fresh every WM_MOUSEMOVE (innerCircle + offset), so the whole leg's
    // shape/pose is preserved exactly instead of the rest of the leg
    // staying behind while just the hip circle moves. thighArc1Bulge/
    // thighArc2Bulge/shinArc1Bulge/shinArc2Bulge don't need an offset here
    // -- they're already relative to their own joint (hip or knee), so a
    // hip drag doesn't change any of them directly.
    Point hipDragKneeOffset;
    Point hipDragAnkleOffset;

    // captured once, when a seam arc drag starts: the mouse's perpendicular
    // offset from the head-butt axis (geometry.h's perpOffsetOnAxis) and
    // the arc's bulge distance at that moment. Each WM_MOUSEMOVE then sets
    // the bulge to (start bulge + perp offset moved since the drag began),
    // a plain 1:1 world-unit delta -- no angle/sensitivity conversion
    // needed now that bulge IS the world-unit quantity being dragged, not
    // a derived angle. Only one arc can be dragged at a time, so seam arc
    // 1 and seam arc 2 share these two fields.
    float arcDragStartPerp;
    float arcDragStartBulge;

    // same idea as arcDragStartPerp/arcDragStartBulge, for the thigh arcs
    // -- perpendicular offset measured against the hip->knee axis instead
    // of head-butt. Only one of thighArc1Bulge/thighArc2Bulge can be
    // dragged at a time, so they share these two fields too.
    float thighArcDragStartPerp;
    float thighArcDragStartBulge;

    // same idea again, for the shin arcs -- perpendicular offset measured
    // against the knee->ankle axis instead of hip->knee. Only one of
    // shinArc1Bulge/shinArc2Bulge can be dragged at a time, so they share
    // these two fields too.
    float shinArcDragStartPerp;
    float shinArcDragStartBulge;

    Point mouseGL;
    DWORD lastLogTime;
} AppState;
