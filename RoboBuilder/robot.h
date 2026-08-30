// robot.h
#pragma once

#include "geometry.h"
#include "app.h"

PointF getCenter(Semni b);

// Prints the current robot pose to the console as a series of
// "app->robotScene.robot.field = value;" assignments, matching the style
// used in app_init.c. Meant for shaping the robot by hand with the
// handles, then copying this output straight into initAppState() as the
// new starting pose.
void printRobotAsInit(Semni b);

// Mirrors the leg (hip through foot, plus the four thigh/shin arcs)
// across the body's own center line -- flips which side it bends toward,
// leaving the torso (head/butt/top-bottom seams) and the whole-body angle
// untouched. See robot.c for the derivation of why each field transforms
// the way it does.
void mirrorHipLeg(Semni* b);

// Rigidly translates the WHOLE robot by (dx, dy) in world units --
// shifts every position-defining field (headX/buttX/y, innerCircle,
// kneeCircle, footCircle) by the same amount, so the pose itself
// (all angles/radii) is left completely untouched, only where it sits
// in the world changes. Used to drag the robot into a starting position
// in Simulation mode (see canvas.c's WM_LBUTTONDOWN/WM_MOUSEMOVE).
void translateRobot(Semni* b, float dx, float dy);

// TRUE if the world-space point (wx, wy) falls genuinely INSIDE any of the
// robot's 5 body circles (head/butt/hip/knee/foot -- see
// computeSemniBodyCircles in renderer.h), i.e. distance to that circle's
// center is <= its radius. Unlike the hover-ghost-circle tolerance check
// in input.c (which only fires near the CIRCUMFERENCE, as an edge-hover
// highlight), this is a true "is the cursor over the robot's body" test --
// used in Simulation mode (canvas.c's WM_SETCURSOR/WM_MOUSEMOVE/
// WM_LBUTTONDOWN) to only show the move cursor and only start a whole-robot
// drag when the mouse is actually over the robot, not anywhere on the
// canvas.
BOOL isPointInsideRobotBody(Semni b, float wx, float wy);

// ---- Rocky ----

// Rocky's rotation pivot is just its own rectangle center -- no head/butt
// pair to average the way Semni's getCenter does.
PointF getRockyCenter(Rocky r);

// Mirrors Rocky's leg bend direction (kneeAngle + its shin arcs) -- same
// idea as mirrorHipLeg's shin-mirroring half, but there's no hip/thigh
// stage above it and no torso to reflect (a rectangle has no natural
// left/right split the way two circles do), so this only flips the one
// joint Rocky actually has.
void mirrorRockyLeg(Rocky* r);

// Same idea as printRobotAsInit, for Rocky's own fields.
void printRockyAsInit(Rocky r);

// Same idea as translateRobot above, for Rocky: shifts bodyX/bodyY (the
// rectangular torso's local center) plus kneeCircle/footCircle by (dx,
// dy), leaving angle/kneeAngle/radii untouched. Used by Simulation mode's
// gravity/drag code (canvas.c) whenever Rocky is the active robot.
void translateRocky(Rocky* r, float dx, float dy);

// ---- Stilo ----

// Same torso as Semni, so the same head/butt-midpoint pivot.
PointF getStiloCenter(Stilo s);

// Mirrors Stilo's FIRST leg only (hip1 + feet1, plus its two thigh1 arcs)
// across the torso's own center line -- same reflect-then-solve-backward
// construction as mirrorHipLeg (see robot.c for the full derivation), just
// a shorter chain (hip1 -> feet1 directly, no knee/shin/foot sub-stage).
// Leg 2 is untouched -- see mirrorStiloLeg2 below for its own, independent
// mirror, since the two legs need to be mirrorable one at a time.
void mirrorStiloLeg(Stilo* s);

// Same construction as mirrorStiloLeg above, for Stilo's SECOND leg (its
// own hip2/feet2/thigh2 fields, see app.h's Stilo comment) -- leaves leg 1
// and the shared torso untouched. Kept as a separate function (rather than
// a parameter on mirrorStiloLeg) so each leg has its own dedicated UI
// button (input.c's ID_MIRROR_LEG_BUTTON/ID_MIRROR_LEG2_BUTTON) and either
// leg can be mirrored independently of the other.
void mirrorStiloLeg2(Stilo* s);

// Same idea as printRobotAsInit, for Stilo's own fields.
void printStiloAsInit(Stilo s);

// Same idea as translateRobot above, for Stilo: Stilo's torso uses the
// exact same headX/buttX/y convention as Semni's, so this shifts those
// plus hip1Circle/feet1Circle/hip2Circle/feet2Circle by (dx, dy), leaving
// every angle/radius untouched. Used by Simulation mode's gravity/drag
// code (canvas.c) whenever Stilo is the active robot.
void translateStilo(Stilo* s, float dx, float dy);
