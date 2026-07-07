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

// Returns the point on the circle (center, radius) that's nearest to
// "target" -- i.e. straight out from the center, through target, to the
// circle's edge. Degenerates to an arbitrary point on the circle if
// target sits exactly on the center (no well-defined direction).
Point circleTowardPoint(Point center, float radius, Point target);

// Given two circles that are (at least approximately) internally tangent --
// i.e. one sits inside the other, touching at exactly one point -- returns
// that touching point. Works out which circle is the "containing" one
// itself (whichever radius is bigger), so it doesn't matter which order
// the two circles are passed in.
Point internalTangentPoint(Point c1, float r1, Point c2, float r2);

typedef struct {
    Point center;
    float radius;
} Fillet;

// Solves for the circle that's internally tangent to c1 AT EXACTLY the
// point given by "angleDeg" (an angle around c1's own center -- the same
// convention as circleEdge), and also internally tangent to c2 somewhere.
// This is the inverse of the old "pick a radius, derive the tangent
// points" approach: here you pick WHERE it touches c1 and the radius +
// center + the other tangent point all fall out of a closed-form solve
// (no iteration, no ambiguity) -- which means a handle placed at that
// exact point on c1 tracks the mouse exactly, with no drift, since the
// point IS the parameter instead of something derived from it.
//
// Not every angle has a sane solution (some angles imply a radius of
// infinity -- the exact attachment point of the true common tangent LINE
// between c1 and c2 -- and just past that the math wants a negative
// radius). The result's radius is clamped to [minRadius, maxRadius], and
// the center is recomputed from the clamped radius so the circle stays
// self-consistent and exactly tangent to c1 at the requested angle even
// when clamped -- only the tangency to c2 becomes approximate in that
// case, never c1.
Fillet filletFromAttachAngle(Point c1, float r1, Point c2, float r2, float angleDeg, float minRadius, float maxRadius);
