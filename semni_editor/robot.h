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

// Mirrors the leg (hip through ankle, plus the four thigh/shin arcs)
// across the body's own center line -- flips which side it bends toward,
// leaving the torso (head/butt/top-bottom seams) and the whole-body angle
// untouched. See robot.c for the derivation of why each field transforms
// the way it does.
void mirrorHipLeg(Semni* b);

// Rigidly translates the WHOLE robot by (dx, dy) in world units --
// shifts every position-defining field (headX/buttX/y, innerCircle,
// kneeCircle, ankleCircle) by the same amount, so the pose itself
// (all angles/radii) is left completely untouched, only where it sits
// in the world changes. Used to drag the robot into a starting position
// in Simulation mode (see canvas.c's WM_LBUTTONDOWN/WM_MOUSEMOVE).
void translateRobot(Semni* b, float dx, float dy);

// TRUE if the world-space point (wx, wy) falls genuinely INSIDE any of the
// robot's 5 body circles (head/butt/hip/knee/ankle -- see
// computeSemniBodyCircles in renderer.h), i.e. distance to that circle's
// center is <= its radius. Unlike the hover-ghost-circle tolerance check
// in input.c (which only fires near the CIRCUMFERENCE, as an edge-hover
// highlight), this is a true "is the cursor over the robot's body" test --
// used in Simulation mode (canvas.c's WM_SETCURSOR/WM_MOUSEMOVE/
// WM_LBUTTONDOWN) to only show the move cursor and only start a whole-robot
// drag when the mouse is actually over the robot, not anywhere on the
// canvas.
BOOL isPointInsideRobotBody(Semni b, float wx, float wy);
