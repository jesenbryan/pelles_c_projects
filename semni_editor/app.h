#pragma once

#include <windows.h>

#include "geometry.h"

#define ID_SAVE_BUTTON 1001
#define ID_MIRROR_LEG_BUTTON 1002
#define ID_STANDING_POSITION_BUTTON 1003
#define ID_HOME_POSITION_BUTTON 1004
#define ID_SCALE_SLIDER 1005
#define ID_VIEW_SEGMENTS_BUTTON 1006
#define ID_DEBUG_LOG_BUTTON 1007

// dropdown at the top of the control panel that picks which of the three
// robots (Semni/Rocky/Stilo) the Standing/Home/Save/Mirror/Debug Log
// buttons below it act on -- see input.c's WM_CREATE/WM_COMMAND.
#define ID_ROBOT_SELECTOR 1008

// Which side of Rocky's rectangle the mouse is near/dragging (see
// input.c's hitTestRockyEdge and app->hoverRockyEdge/draggingRockyEdge
// below) -- 0 means "not near any edge". Dragging LEFT/RIGHT changes
// bodyHalfWidth, dragging TOP/BOTTOM changes bodyHalfHeight, same
// fixed-center symmetric resize as the body handle's own scroll-to-resize
// (see WM_MOUSEWHEEL), just driven by absolute mouse position instead of
// wheel notches.
#define ROCKY_EDGE_NONE   0
#define ROCKY_EDGE_LEFT   1
#define ROCKY_EDGE_RIGHT  2
#define ROCKY_EDGE_TOP    3
#define ROCKY_EDGE_BOTTOM 4

// ---- robot model (the "Semni") ----

typedef struct {
    float headX, buttX;
    float y;

    float headRadius;
    float buttRadius;

    // seam arc 1/2 (formerly "top"/"bottom"), each a circular arc
    // internally tangent to both the head and butt circles (distance
    // between centers == radius difference) -- crease-free by
    // construction instead of by manual placement. Each is parameterized
    // by the ANGLE where it attaches to the head circle (same convention
    // as circleEdge) -- the fillet's radius, center, and its other
    // tangent point on the butt circle are all derived from that angle
    // via a closed-form solve every frame (see geometry.h's
    // filletFromAttachAngle()/internalTangentPoint()). Driving this by
    // the attach angle instead of the radius directly means the drag
    // handle sits exactly at circleEdge(head, headRadius, angle) -- the
    // point IS the parameter, so it tracks the mouse exactly with no
    // drift.
    //
    // Named seamArc1Angle/seamArc2Angle to match the thighArc1Angle/
    // thighArc2Angle/shinArc1Angle/shinArc2Angle convention below
    // ([BodyPart]Arc[N]Angle, "Arc" trailing), rather than arcSeam1/2.
    float seamArc1Angle;
    float seamArc2Angle;

    PointF innerCircle;
    float innerRadius;

    // second joint circle + the two side-arc handles connecting it back
    // to innerCircle, forming a thigh-like limb (like innerCircle is the
    // hip, kneeCircle is the knee)
    PointF kneeCircle;
    float kneeRadius;

    // the two thigh seams, both parameterized by the angle where they
    // attach to innerCircle (same circleEdge/filletFromAttachAngle
    // convention as seamArc1Angle/seamArc2Angle), with the fillet's
    // radius, center, and other tangent point (on kneeCircle) derived
    // from that angle every frame. They're NOT the same kind of curve,
    // though: thighArc1Angle uses the usual convex construction
    // (filletFromAttachAngle) and bulges outward, away from the hip-knee
    // axis, same as seam arc 1/2. thighArc2Angle uses the concave
    // construction (filletFromAttachAngleConcave) instead and pinches
    // inward, toward the hip-knee axis -- its safe range sits on the
    // opposite side of innerCircle from thighArc1Angle's (facing
    // kneeCircle rather than away from it), so the two no longer share a
    // degenerate point to stay clear of and don't need the "each stays on
    // its own side" locking seam arc 1/2 and thighArc1Angle still use
    // (see THIGH_ARC_SIDE_MARGIN_DEG) -- they already can't collide.
    float thighArc1Angle;
    float thighArc2Angle;

    // continues the leg past the knee: ankleCircle is the next joint,
    // connected back to kneeCircle by two arcs, same pattern as the thigh
    PointF ankleCircle;
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

// ---- robot model ("Rocky") ----
//
// A simpler two-part robot: a rectangular torso, plus a single leg that's
// otherwise IDENTICAL in construction to Semni's own knee-to-ankle "shin"
// (kneeCircle -> two tangent-fillet arcs -> ankleCircle, see app.h's Semni
// comment and renderer.c's drawShin) -- it just hangs directly off the
// rectangle instead of off a hip/thigh stage. There's no equivalent of
// Semni's hipAngle here: kneeCircle is rigidly attached to the torso (it
// only turns with the whole-body angle below, the same way Semni's own
// innerCircle/hip is rigidly attached to ITS torso), and kneeAngle alone
// swings the leg, exactly like Semni's kneeAngle swings its shin around
// the knee.
typedef struct {
    // rectangular torso: a local (pre-rotation) center + half-extents,
    // same "local, rotated at render time by angle" convention as Semni's
    // headX/buttX/y.
    float bodyX, bodyY;
    float bodyHalfWidth, bodyHalfHeight;

    PointF kneeCircle;
    float kneeRadius;

    PointF ankleCircle;
    float ankleRadius;

    float shinArc1Angle;
    float shinArc2Angle;

    float angle;      // whole-body rotation, around (bodyX, bodyY)
    float kneeAngle;  // swings the leg (ankleCircle) around kneeCircle
} Rocky;

// ---- robot model ("Stilo") ----
//
// Same torso as Semni (head/butt circles + two seam arcs between them,
// identical fields/construction -- see drawSemniBody), but the leg has no
// knee stage: it goes straight from the hip (innerCircle) to the foot
// (ankleCircle) via ONE pair of tangent-fillet arcs instead of two, so
// thighArc1Angle/thighArc2Angle here connect innerCircle directly to
// ankleCircle (same convex/concave split as Semni's own thigh arcs, just
// spanning the whole leg instead of stopping at a knee).
typedef struct {
    float headX, buttX;
    float y;

    float headRadius;
    float buttRadius;

    float seamArc1Angle;
    float seamArc2Angle;

    PointF innerCircle;
    float innerRadius;

    PointF ankleCircle;
    float ankleRadius;

    float thighArc1Angle;   // hip-to-ankle arc 1 (convex)
    float thighArc2Angle;   // hip-to-ankle arc 2 (concave)

    float angle;      // whole-body rotation
    float hipAngle;   // rotates the leg (ankleCircle) around innerCircle
} Stilo;

// Which of the three robot models the Robot editor (input.c) is currently
// showing/editing -- picked via the hRobotSelector dropdown in the
// control panel. Semni keeps its existing name ("robot") in RobotScene
// below for backward compatibility with the ~300 existing references to
// robotScene.robot across canvas.c (Simulation mode), renderer.c, robot.c,
// save.c, and app_init.c -- rocky/stilo are purely additive fields.
typedef enum {
    ROBOT_KIND_SEMNI = 0,
    ROBOT_KIND_ROCKY = 1,
    ROBOT_KIND_STILO = 2,
    ROBOT_KIND_COUNT = 3
} RobotKind;

typedef struct {
    Semni robot;
    Rocky rocky;
    Stilo stilo;
    RobotKind activeKind;
} RobotScene;

// ---- UI ----

typedef struct {
    // dropdown at the very top of the control panel that picks the active
    // robot (Semni/Rocky/Stilo, see RobotScene.activeKind above) -- every
    // other control in this struct acts on whichever one this currently
    // selects.
    HWND hRobotSelector;

    HWND hSaveButton;
    HWND hMirrorButton;
    HWND hStandingPositionButton;
    HWND hHomePositionButton;

    // top-right robot "size" slider (0.25 - 1.0, see ROBOT_SCALE_MIN/MAX
    // in config.h) + its static label, a row below the buttons above.
    // Drives graphicsSetRobotScale on WM_HSCROLL (see input.c).
    HWND hScaleLabel;
    HWND hScaleSlider;

    // "View Segments" toggle -- same idea as the ArcSpline canvas's own
    // View Segments button (ui.c), reveals the full circle each fillet
    // arc (seam/thigh/shin) was trimmed from. Drawn checkbox-as-button
    // (BS_AUTOCHECKBOX | BS_PUSHLIKE), so the button IS the toggle state
    // -- see WM_COMMAND's ID_VIEW_SEGMENTS_BUTTON handling in input.c.
    HWND hViewSegmentsButton;

    // Dumps the current robot pose as app_init.c-style assignments to the
    // console (see printRobotAsInit in robot.c) on demand -- used to be a
    // continuous "print every 1s while the mouse moves" background log in
    // WM_MOUSEMOVE, replaced with this explicit button so the console only
    // gets a dump when you actually want one (e.g. to copy a hand-posed
    // starting pose back into app_init.c).
    HWND hDebugLogButton;

    // bottom-left status label -- shows the friendly name of whichever
    // handle the mouse is currently hovering (e.g. "Thigh Arc 1"), blank
    // when nothing is hovered. Updated every WM_MOUSEMOVE.
    HWND hHoverLabel;

    // Solid-backed backdrop panels so the controls above read as a real
    // toolbar/status strip instead of floating raw over the OpenGL-
    // rendered robot. hControlPanel + hPanelTitle sit behind/above the
    // top-right button cluster; hHoverPanel sits behind hHoverLabel.
    // All three are created before the controls they back so the
    // controls' own z-order (later CreateWindow = higher/on top) draws
    // them on top of the panel automatically -- see input.c's WM_CREATE.
    HWND hControlPanel;
    HWND hPanelTitle;
    HWND hHoverPanel;
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

    // Rocky's rectangular torso handle (see input.c's ROBOT_KIND_ROCKY
    // branches) -- same interaction as Semni's own hip circle: hover to
    // highlight, drag to move the whole robot (rectangle + leg together),
    // plain scroll to resize. Kept separate from the Semni fields above
    // (rather than reusing draggingInner/hoverHip) since a completely
    // different struct (Rocky, not Semni) is what's actually being
    // edited, even though only one of the two is ever active at once.
    int hoverRockyBody;
    int draggingRockyBody;

    // Which edge of Rocky's rectangle (see the ROCKY_EDGE_* constants
    // above) the mouse is near, or is currently being dragged -- a
    // second, independent handle from hoverRockyBody/draggingRockyBody
    // above (only one of the two is ever active at a time, same
    // mutual-exclusion WM_LBUTTONDOWN already gives the body handle vs.
    // everything else).
    int hoverRockyEdge;
    int draggingRockyEdge;

    // Rocky's knee handle -- the joint where the leg (kneeCircle->
    // ankleCircle, see the Rocky struct comment) attaches to the
    // rectangle, same role Semni's own kneeCircle plays relative to its
    // hip. Mirrors Semni's hoverKnee/draggingKnee treatment for the
    // hover/scroll/Shift+scroll behaviors, but the drag itself is freer
    // than Semni's own axis-constrained hip->knee drag: it can be dropped
    // anywhere inside the rectangle, clamped so the knee circle's own
    // circumference never pokes outside the body. Plain scroll resizes
    // kneeRadius, Shift+scroll bends kneeAngle (swings ankleCircle around
    // the knee) -- see input.c's WM_LBUTTONDOWN/WM_MOUSEMOVE/
    // WM_MOUSEWHEEL ROBOT_KIND_ROCKY branches.
    int hoverRockyKnee;
    int draggingRockyKnee;

    // captured once, when a Rocky knee drag starts: ankleCircle's fixed
    // offset from kneeCircle at that moment, re-applied fresh every
    // WM_MOUSEMOVE -- same "pin the shin's shape, only the knee's own
    // distance from the body changes" idea as kneeDragAnkleOffset below.
    PointF rockyKneeDragAnkleOffset;

    // captured once, when a Rocky body drag starts: kneeCircle's and
    // ankleCircle's fixed offsets from (bodyX, bodyY) at that moment --
    // same "re-applied fresh every WM_MOUSEMOVE" pattern as
    // hipDragKneeOffset/hipDragAnkleOffset below, so dragging the
    // rectangle carries the whole leg along as one rigid piece instead of
    // leaving it behind.
    PointF rockyDragKneeOffset;
    PointF rockyDragAnkleOffset;

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
    // what it was when the drag began. shinArc1Angle/shinArc2Angle need no
    // equivalent offset -- like thighArc1Angle/thighArc2Angle, they're
    // already knee-frame-relative angles, unaffected by moving kneeCircle
    // itself.
    PointF kneeDragAnkleOffset;

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
    PointF hipDragKneeOffset;
    PointF hipDragAnkleOffset;

    // captured once, when a seam arc drag starts: the mouse's local Y and
    // the arc's angle at that moment. Each WM_MOUSEMOVE then sets the
    // angle to (start angle + scaled Y delta since the drag began)
    // instead of solving for an exact angle from the absolute mouse
    // position -- see ARC_DRAG_SENSITIVITY_DEG_PER_UNIT's comment in
    // config.h for why. Only one arc can be dragged at a time, so seam
    // arc 1 and seam arc 2 share these two fields.
    float arcDragStartMouseY;
    float arcDragStartAngle;

    // same idea as arcDragStartMouseY/arcDragStartAngle, for the thigh
    // arcs -- except the hip->knee axis isn't fixed horizontal like the
    // butt-head axis (it rotates with hipAngle and the user can pose it
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

    PointF mouseGL;

    // "View Segments" state for the Semni robot editor (see
    // hViewSegmentsButton above) -- when set, drawSemniCircleSegments
    // (renderer.c) overlays the full circle behind every fillet arc.
    int showCircleSegments;

    // Index (0-5, see NUM_ROBOT_CIRCLE_SEGMENTS in renderer.h) of the
    // circle segment currently under the mouse, or -1 if none/not
    // applicable. Updated every WM_MOUSEMOVE while showCircleSegments is
    // set (see input.c), read back by renderRobot to highlight it.
    int hoveredCircleSegment;

    // Index (0-4, see NUM_ROBOT_BODY_CIRCLES in renderer.h) of the
    // always-visible body circle (head/butt/hip/knee/ankle) whose
    // circumference the mouse is currently near, or -1 if none/not
    // applicable. Same View Segments gating as hoveredCircleSegment above
    // -- only updated while showCircleSegments is set (see input.c).
    int hoveredBodyCircle;

    // TRUE while the user is actively dragging the whole robot into a
    // starting position in Simulation mode (see canvas.c's
    // WM_LBUTTONDOWN/WM_MOUSEMOVE, which set this and call
    // translateRobot). Lives here rather than as a canvas.c-local static
    // so renderRobot (renderer.c) can read it too, threading it into
    // RenderState.draggingWhole so the robot's outline renders blue for as
    // long as this is true, and back to normal the instant it's released.
    int draggingRobotSim;

    // TRUE while the cursor is over the robot's body in Simulation mode,
    // whether or not a drag is actually in progress (see
    // isPointInsideRobotBody, robot.c, and canvas.c's WM_MOUSEMOVE, which
    // keeps this current every move). Used for two things in canvas.c:
    // showing the move cursor and gating whether a click starts a drag --
    // and, via RenderState.hoveringWhole (renderer.c), for turning the
    // robot's outline yellow as hover feedback. draggingRobotSim above
    // takes priority over this for color purposes (dragging stays blue),
    // since this flag is deliberately left stale/TRUE for the whole
    // duration of a drag (WM_MOUSEMOVE's drag branch returns before
    // reaching the code that updates this).
    int hoveringRobotSim;
} AppState;

// The single running instance of the Semni app's state, defined in main.c.
// Declared here (rather than only ever passed around as a pointer) so
// editor_mode.c can show/hide Semni's own child controls (app.ui.*) when
// switching editor modes without needing an AppState* threaded through
// the whole call chain just for that.
extern AppState app;
