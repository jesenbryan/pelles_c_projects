// robot.h
#pragma once

#include "geometry.h"
#include "app.h"

Point getCenter(Semni b);

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
