#pragma once

#include <windows.h>

#include "geometry.h"

// ID_SAVE_BUTTON (1001) used to be its own button in this control panel --
// removed; saving is now a single File > Save menu item (ID_SAVE, see
// ui_state.h and canvas.c's WM_COMMAND) that dispatches on
// editorModeState.currentMode/app->robotScene.activeKind, the same
// dispatch this button used to do.
#define ID_MIRROR_LEG_BUTTON 1002
#define ID_STANDING_POSITION_BUTTON 1003
#define ID_HOME_POSITION_BUTTON 1004
#define ID_SCALE_SLIDER 1005
#define ID_VIEW_SEGMENTS_BUTTON 1006
#define ID_DEBUG_LOG_BUTTON 1007
#define ID_SET_STANDING_BUTTON 1009
#define ID_SET_HOME_BUTTON 1010

// Mirrors Stilo's SECOND leg only (see robot.c's mirrorStiloLeg2) -- only
// meaningful while Stilo is the active robot (see app.h's Stilo comment on
// its Leg2 fields); harmlessly inert for Semni/Rocky, same convention as
// ID_VIEW_SEGMENTS_BUTTON being inert for Rocky/Stilo. ID_MIRROR_LEG_BUTTON
// above now only ever mirrors leg 1 (or Semni/Rocky's own single leg).
#define ID_MIRROR_LEG2_BUTTON 1011

// Body/Leg Weight edit boxes (see hBodyWeightEdit/hLegWeightEdit below) --
// read via GetWindowText from the File > Save handler (canvas.c) rather
// than driven by a WM_COMMAND notification, so these don't strictly need
// control IDs the
// way the buttons/combobox above do, but every other control here has one
// for consistency and in case EN_CHANGE handling is ever added.
#define ID_BODY_WEIGHT_EDIT 1012
#define ID_LEG_WEIGHT_EDIT 1013

// dropdown at the top of the control panel that picks which of the three
// robots (Semni/Rocky/Stilo) the Standing/Home/Mirror/Debug Log buttons
// below it act on, and which one File > Save (canvas.c) exports -- see
// input.c's WM_CREATE/WM_COMMAND.
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

    // continues the leg past the knee: footCircle is the next joint,
    // connected back to kneeCircle by two arcs, same pattern as the thigh
    PointF footCircle;
    float footRadius;

    // the two shin seams, both parameterized by the angle where they
    // attach to kneeCircle (same circleEdge/filletFromAttachAngle
    // convention as the thigh pair above), with the fillet's radius,
    // center, and other tangent point (on footCircle) derived from that
    // angle every frame. Same convex/concave split as thighArc1Angle/
    // thighArc2Angle: shinArc1Angle uses the usual convex construction
    // (filletFromAttachAngle) and bulges outward, away from the
    // knee-foot axis. shinArc2Angle uses the concave construction
    // (filletFromAttachAngleConcave) instead and pinches inward, toward
    // the knee-foot axis -- its safe range sits on the opposite side of
    // kneeCircle from shinArc1Angle's, so the two don't share a
    // degenerate point to stay clear of and drag independently with no
    // side-locking needed between them.
    float shinArc1Angle;
    float shinArc2Angle;

    float angle;      // whole-body rotation

    // hip joint rotation: rotates the whole leg chain (kneeCircle,
    // footCircle) around innerCircle, independent of the whole-body angle
    // above. thighArc1Angle/thighArc2Angle/shinArc1Angle/shinArc2Angle
    // don't need rotating themselves -- they're angles measured in their
    // own joint's local frame (around innerCircle or kneeCircle), so
    // hipAngle rotating that whole frame at render/hit-test time carries
    // them along automatically.
    float hipAngle;

    // knee joint rotation: rotates just the shin (footCircle) around
    // kneeCircle, independent of hipAngle/angle
    float kneeAngle;
} Semni;

// ---- robot model ("Rocky") ----
//
// A simpler two-part robot: a rectangular torso, plus a single leg that's
// otherwise IDENTICAL in construction to Semni's own knee-to-foot "shin"
// (kneeCircle -> two tangent-fillet arcs -> footCircle, see app.h's Semni
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

    PointF footCircle;
    float footRadius;

    float shinArc1Angle;
    float shinArc2Angle;

    float angle;      // whole-body rotation, around (bodyX, bodyY)
    float kneeAngle;  // swings the leg (footCircle) around kneeCircle

    // User-entered mass values for the Rob.txt/Arm.txt export (see
    // save.c's saveRockyAsRobArm and input.c's hBodyWeightEdit/
    // hLegWeightEdit) -- typed into the control panel rather than derived
    // from geometry, since there's no material/density model here to
    // compute a real mass from. Defaulted to 1.0f in app_init.c's
    // initRockyStandingPosition/initRockyHomePosition.
    float bodyWeight;
    float legWeight;
} Rocky;

// ---- robot model ("Stilo") ----
//
// Two independent, two-joint legs (hip -> feet, connected by a pair of
// thigh fillet arcs) hanging off one shared torso (head/butt circles + two
// seam arcs between them). There's no knee/shin/foot sub-stage any more --
// "feet" IS the end of each leg, directly attached to its own hip. Fields
// are named/numbered per leg throughout: hip1/feet1/thigh1 for the first
// leg, hip2/feet2/thigh2 for the second -- see input.c's hover labels
// ("Hip 1", "Feet 1", "Thigh 1 Arc 1", "Thigh 1 Arc 2", etc.) for the same
// naming applied to the UI.
typedef struct {
    float headX, buttX;
    float y;

    float headRadius;
    float buttRadius;

    float seamArc1Angle;
    float seamArc2Angle;

    float angle;      // whole-body rotation, shared by both legs

    // ---- leg 1 ----
    PointF hip1Circle;
    float hip1Radius;
    float hip1Angle;   // rotates leg 1 (feet1Circle, thigh1 arcs) around hip1Circle

    PointF feet1Circle;
    float feet1Radius;

    float thigh1Arc1Angle;   // leg 1's hip-to-feet arc 1 (convex)
    float thigh1Arc2Angle;   // leg 1's hip-to-feet arc 2 (concave)

    // ---- leg 2 ----
    //
    // Same construction as leg 1 above, fully independent so it can be
    // posed/dragged/mirrored separately -- see input.c's ROBOT_KIND_STILO
    // branches, renderer.c's drawStiloThigh2, and robot.c's
    // mirrorStiloLeg2/printStiloAsInit.
    PointF hip2Circle;
    float hip2Radius;
    float hip2Angle;   // rotates leg 2 (feet2Circle, thigh2 arcs) around hip2Circle

    PointF feet2Circle;
    float feet2Radius;

    float thigh2Arc1Angle;   // leg 2's hip-to-feet arc 1 (convex)
    float thigh2Arc2Angle;   // leg 2's hip-to-feet arc 2 (concave)
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

    HWND hMirrorButton;

    // Mirrors Stilo's leg 2 only (see ID_MIRROR_LEG2_BUTTON) -- created and
    // shown alongside hMirrorButton above, but its WM_COMMAND handler is a
    // no-op unless Stilo is the active robot.
    HWND hMirrorButton2;

    HWND hStandingPositionButton;
    HWND hHomePositionButton;

    // Captures the CURRENT pose as the new Standing/Home target for
    // whichever robot is active, persisted to disk (see save.h's
    // loadRobotPoseFromFile/loadRockyPoseFromFile/loadStiloPoseFromFile
    // and input.c's ID_SET_STANDING_BUTTON/ID_SET_HOME_BUTTON handling) --
    // Standing/Home above then load that custom file instead of the
    // hardcoded app_init.c default, falling back to the default if no
    // custom pose was ever saved.
    HWND hSetStandingButton;
    HWND hSetHomeButton;

    // Body/Leg Weight input boxes -- only meaningful for Rocky (see
    // Rocky's own bodyWeight/legWeight fields and save.c's
    // saveRockyAsRobArm, which reads them via canvas.c's File > Save
    // handler), same
    // "harmlessly inert for other robot kinds" convention as
    // hMirrorButton2/hViewSegmentsButton. Label + edit box per weight,
    // same pairing as hScaleLabel/hScaleSlider above.
    HWND hBodyWeightLabel;
    HWND hBodyWeightEdit;
    HWND hLegWeightLabel;
    HWND hLegWeightEdit;

    // Live "Size: W x H mm" readout for whichever robot kind is currently
    // active (see input.c's updateRobotSizeLabel/robotBoundingBoxLocal and
    // config.h's MM_PER_WORLD_UNIT) -- refreshed after every drag ends
    // (WM_LBUTTONUP), every wheel-resize (WM_MOUSEWHEEL), on robot-kind
    // switch, and once at WM_CREATE.
    HWND hRobotSizeLabel;

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
    int draggingFoot;
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
    // footCircle, see the Rocky struct comment) attaches to the
    // rectangle, same role Semni's own kneeCircle plays relative to its
    // hip. Mirrors Semni's hoverKnee/draggingKnee treatment for the
    // hover/scroll/Shift+scroll behaviors, but the drag itself is freer
    // than Semni's own axis-constrained hip->knee drag: it can be dropped
    // anywhere inside the rectangle, clamped so the knee circle's own
    // circumference never pokes outside the body. Plain scroll resizes
    // kneeRadius, Shift+scroll bends kneeAngle (swings footCircle around
    // the knee) -- see input.c's WM_LBUTTONDOWN/WM_MOUSEMOVE/
    // WM_MOUSEWHEEL ROBOT_KIND_ROCKY branches.
    int hoverRockyKnee;
    int draggingRockyKnee;

    // captured once, when a Rocky knee drag starts: footCircle's fixed
    // offset from kneeCircle at that moment, re-applied fresh every
    // WM_MOUSEMOVE -- same "pin the shin's shape, only the knee's own
    // distance from the body changes" idea as kneeDragFootOffset below.
    PointF rockyKneeDragFootOffset;

    // Rocky's foot handle -- mirrors Semni's own hoverFoot/draggingFoot:
    // hover to highlight, drag along the knee->foot axis (constrainToAxis,
    // same as Semni) to change the leg's length. No offset capture needed
    // -- unlike the knee/body handles, nothing hangs off the foot for a
    // drag to carry along.
    int hoverRockyFoot;
    int draggingRockyFoot;

    // Rocky's mass-center dot -- a THIRD kind of handle, unlike every
    // other one above: dragging it doesn't move a geometric field
    // (kneeCircle, footCircle, etc) at all, it moves bodyWeight/legWeight
    // (see this struct's own comment on those two fields, and input.c's
    // hBodyWeightEdit/hLegWeightEdit) by re-deriving them from wherever the
    // dot lands along the rect-centroid<->leg-centroid segment (the only
    // positions reachable by a non-negative weight ratio -- see renderer.c's
    // computeRockyMassCenterEndpointsWorld/computeRockyMassCenterWorld and
    // input.c's WM_MOUSEMOVE draggingRockyMassCenter branch). Hover-
    // highlights the same way hoverRockyKnee/hoverRockyFoot do.
    int hoverRockyMassCenter;
    int draggingRockyMassCenter;

    // captured once, when a mass-center drag starts: bodyWeight+legWeight
    // at that moment, held fixed for the whole drag so only the RATIO
    // between them changes as the dot moves -- not the total. Matters
    // because save.c's saveRockyAsRobArm writes bodyWeight itself out as a
    // literal mass value (Rob.txt's own third number on its first line),
    // not just a blend weight -- silently renormalizing the total every
    // frame (e.g. always resetting it to 1.0) would quietly overwrite
    // whatever absolute mass the user had actually typed in.
    float rockyMassCenterDragTotal;

    // Rocky's 2 shin connector-arc handles (the fillets between kneeCircle
    // and footCircle, shinArc1Angle/shinArc2Angle) -- mirrors Semni's own
    // draggingShin1/draggingShin2 treatment: drag-only highlight, no
    // separate hover-highlight state at all (Semni's own shin handles
    // don't track a hoverShin1/hoverShin2 either -- see renderer.c's
    // drawShinHandles, which only lights up on rs->draggingShin1/
    // draggingShin2, and input.c's hover-label block, which just runs
    // isNear directly against the handle position rather than reading a
    // stored flag). Kept as Rocky-specific fields rather than reusing
    // Semni's draggingShin1/draggingShin2, same "separate struct being
    // edited" reasoning as hoverRockyBody/draggingRockyBody above.
    int draggingRockyShin1;
    int draggingRockyShin2;

    // captured once, when a Rocky shin arc drag starts: the mouse's
    // perpendicular-to-axis offset (relative to the knee->foot axis) and
    // the arc's own angle at that moment, so WM_MOUSEMOVE can nudge the
    // angle incrementally from here -- same idea as shinArcDragStartPerp/
    // shinArcDragStartAngle below, just a separate copy since Rocky's
    // shin1/shin2 drags are independent of Semni's own (could in principle
    // be dragging one of each if the app allowed switching robots mid-drag,
    // which it doesn't, but keeping them separate costs nothing and
    // matches this file's existing per-robot-kind convention).
    float rockyShinArcDragStartPerp;
    float rockyShinArcDragStartAngle;

    // captured once, when a Rocky body drag starts: kneeCircle's and
    // footCircle's fixed offsets from (bodyX, bodyY) at that moment --
    // same "re-applied fresh every WM_MOUSEMOVE" pattern as
    // hipDragKneeOffset/hipDragFootOffset below, so dragging the
    // rectangle carries the whole leg along as one rigid piece instead of
    // leaving it behind.
    PointF rockyDragKneeOffset;
    PointF rockyDragFootOffset;

    // true while the mouse is merely hovering near a joint circle handle
    // (hip/knee/foot/head/butt) -- separate from the "dragging" flags
    // above, so those handles can flash yellow just from a hover, while
    // drag-only handles (thigh/shin bulges, seam attach points) keep
    // their dragging-only highlight
    int hoverHip;
    int hoverKnee;
    int hoverFoot;
    int hoverButt;
    int hoverHead;

    // captured once, when a knee drag starts: footCircle's fixed offset
    // from kneeCircle at that moment. Re-applied fresh every WM_MOUSEMOVE
    // (kneeCircle + offset) instead of nudging it frame by frame, so the
    // shin's length can't drift over a long drag -- it's pinned to exactly
    // what it was when the drag began. shinArc1Angle/shinArc2Angle need no
    // equivalent offset -- like thighArc1Angle/thighArc2Angle, they're
    // already knee-frame-relative angles, unaffected by moving kneeCircle
    // itself.
    PointF kneeDragFootOffset;

    // same idea, but for a hip drag: the whole leg (kneeCircle,
    // footCircle) hangs off innerCircle, so moving the hip needs to carry
    // all of it along as one rigid piece -- each point's fixed offset from
    // innerCircle is captured when the hip drag starts, then reapplied
    // fresh every WM_MOUSEMOVE (innerCircle + offset), so the whole leg's
    // shape/pose is preserved exactly instead of the rest of the leg
    // staying behind while just the hip circle moves. thighArc1Angle/
    // thighArc2Angle/shinArc1Angle/shinArc2Angle don't need an offset here
    // -- they're already relative to their own joint (hip or knee), so a
    // hip drag doesn't change any of them directly.
    PointF hipDragKneeOffset;
    PointF hipDragFootOffset;

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
    // against the knee->foot axis instead of hip->knee. Only one of
    // shinArc1Angle/shinArc2Angle can be dragged at a time, so they share
    // these two fields too.
    float shinArcDragStartPerp;
    float shinArcDragStartAngle;

    // Stilo's own per-joint dragging/hover state -- one hip and one feet
    // handle per leg, plus that leg's own two thigh arcs (no knee/shin/foot
    // sub-stage any more, see app.h's Stilo comment). Torso-level seam arc
    // dragging is shared between both legs (there's only one torso).
    int draggingStiloSeamArc1;
    int draggingStiloSeamArc2;

    int draggingStiloHip1;
    int draggingStiloFeet1;
    int draggingStiloThigh1Arc1;
    int draggingStiloThigh1Arc2;

    int hoverStiloHip1;
    int hoverStiloFeet1;
    int hoverStiloButt;
    int hoverStiloHead;

    // captured once, when a hip1 drag starts: feet1Circle's fixed offset
    // from hip1Circle at that moment, re-applied fresh every WM_MOUSEMOVE
    // -- same "pin the leg's shape, only the hip's own position changes"
    // idea as hipDragKneeOffset above -- so dragging the hip carries feet1
    // along as one rigid piece.
    PointF stiloHip1DragFeetOffset;

    // same drag-start-angle capture idea as arcDragStartMouseY/Angle and
    // thighArcDragStartPerp/Angle above, for Stilo's own seam/thigh1 arc
    // drags
    float stiloArcDragStartMouseY;
    float stiloArcDragStartAngle;
    float stiloThigh1ArcDragStartPerp;
    float stiloThigh1ArcDragStartAngle;

    // Stilo's SECOND leg -- same roles as draggingStiloHip1/draggingStiloFeet1/
    // draggingStiloThigh1Arc1/draggingStiloThigh1Arc2 and hoverStiloHip1/
    // hoverStiloFeet1 above, just against app->robotScene.stilo's leg 2
    // fields (see app.h's Stilo comment) instead of its first leg. The
    // torso-level seam arc dragging and head/butt hover aren't duplicated
    // -- both legs share the one torso.
    int draggingStiloHip2;
    int draggingStiloFeet2;
    int draggingStiloThigh2Arc1;
    int draggingStiloThigh2Arc2;

    int hoverStiloHip2;
    int hoverStiloFeet2;

    // same offset-capture idea as stiloHip1DragFeetOffset above, for leg
    // 2's own hip drag
    PointF stiloHip2DragFeetOffset;

    // same drag-start-angle capture idea as stiloThigh1ArcDragStartPerp/Angle
    // above, for leg 2's own thigh arc drags
    float stiloThigh2ArcDragStartPerp;
    float stiloThigh2ArcDragStartAngle;

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
    // always-visible body circle (head/butt/hip/knee/foot) whose
    // circumference the mouse is currently near, or -1 if none/not
    // applicable. Same View Segments gating as hoveredCircleSegment above
    // -- only updated while showCircleSegments is set (see input.c).
    int hoveredBodyCircle;

    // Index (0-3, see NUM_ROCKY_RECT_SEGMENTS in renderer.h) of Rocky's
    // rectangular body edge (left/top/right/bottom) currently under the
    // mouse, or -1 if none/not applicable/not Rocky. A THIRD category
    // alongside hoveredCircleSegment/hoveredBodyCircle above, needed
    // because Rocky's body is a straight-edged rectangle, not a circle --
    // neither of those two fits it. Same "always exists, only meaningful
    // for one robot kind" convention as hMirrorButton2/hViewSegmentsButton
    // (harmlessly -1 for Semni/Stilo). Same View Segments gating as the
    // other two -- only updated while showCircleSegments is set.
    int hoveredRectSegment;

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

    // GetTickCount() timestamp from the most recent time the "Robot Size"
    // slider moved (see input.c's WM_HSCROLL), or 0 if it's never been
    // touched / the box has fully faded back out. renderer.c's
    // drawRobotSizeBox reads this every frame to compute how visible the
    // real-world-size bounding box overlay should be right now (see
    // config.h's ROBOT_SIZE_BOX_HOLD_MS/FADE_MS) -- same "stateless fade
    // derived fresh from an elapsed-time snapshot" pattern as canvas.c's
    // gravityToastStartTick/gaitToastStartTick, just living on AppState
    // instead of as a canvas.c static since renderer.c (not canvas.c) is
    // what needs to read it.
    DWORD robotSizeBoxStartTick;
} AppState;

// The single running instance of the Semni app's state, defined in main.c.
// Declared here (rather than only ever passed around as a pointer) so
// editor_mode.c can show/hide Semni's own child controls (app.ui.*) when
// switching editor modes without needing an AppState* threaded through
// the whole call chain just for that.
extern AppState app;
