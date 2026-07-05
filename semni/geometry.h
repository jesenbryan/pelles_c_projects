// geometry.h
#pragma once

typedef struct {
    float x;
    float y;
} Point;

typedef struct {
    Point center;
    float radius;
    int valid; // 0 if the 3 points were (nearly) collinear and no circle exists
} Circle;

Point rotatePoint(Point p, Point center, float angleDeg);
Point inverseRotate(Point p, Point center, float angleDeg);

// Rotates a point that lives in a joint-local frame (around "pivot" by
// "jointAngle") and then places the result in world space (around
// "center" by "bodyAngle"). Used so a joint (e.g. the hip) can rotate
// independently of the whole-body rotation.
Point jointToWorld(Point local, Point pivot, float jointAngle, Point center, float bodyAngle);

// Same idea as jointToWorld, but for a point that hangs off two nested
// joints (e.g. the shin, which hangs off the knee, which itself hangs
// off the hip). Rotates around "innerPivot" by "innerAngle" first (the
// knee), then around "outerPivot" by "outerAngle" (the hip), then into
// world space around "center" by "bodyAngle".
Point nestedJointToWorld(Point local, Point innerPivot, float innerAngle, Point outerPivot, float outerAngle, Point center, float bodyAngle);

// Projects "target" onto the ray from "pivot" through "axisRef", so the
// result always lies on that fixed line. Used so dragging a joint (e.g.
// the knee or ankle circle) can only change a limb's length, not the
// direction it points in -- rotation is handled separately by the joint
// angle. "minLength" stops the limb from collapsing to zero or flipping
// through the pivot.
Point constrainToAxis(Point pivot, Point axisRef, Point target, float minLength);

// Repositions "p" so its along-axis component sits exactly at the
// midpoint of the axisStart->axisEnd segment, while keeping whatever
// perpendicular (sideways) offset "p" currently has from that axis.
// Used to keep an arc's bulge handle centered on the arc -- both when
// the user drags the handle itself (only the sideways offset should
// move) and when the limb it belongs to changes length (the handle
// should follow the new midpoint).
Point recenterOnAxis(Point p, Point axisStart, Point axisEnd);

Point circleEdge(Point center, float radius, float angleDeg);

int isNear(Point a, Point b, float radius);

Circle circumcircle(Point p0, Point p1, Point p2);
