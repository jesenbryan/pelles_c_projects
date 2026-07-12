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

typedef struct {
    float centerDeg;    // angle (around c1) of the point on c1 farthest from c2 -- the "safest" attach point, always valid
    float halfWidthDeg; // the safe range extends this far to either side of centerDeg
} SafeAngleRange;

// Computes the range of attach angles (around c1, same convention as
// circleEdge/filletFromAttachAngle) for which filletFromAttachAngle(c1, r1,
// c2, r2, angle, ..., maxRadius) stays EXACTLY tangent to both circles --
// i.e. its unclamped radius never needs to exceed maxRadius. Two things can
// break that: the radius blowing all the way up to infinity (the true
// common-tangent LINE angle, then flipping to the wrong side past it), or --
// usually the tighter bound -- the radius simply exceeding maxRadius.
// Clamping the DRAG ANGLE into this range (instead of just letting the
// radius get clamped after the fact) is what stops the seam handle from
// ever being dragged far enough to flatten the arc into a line or flip it
// to the other side.
SafeAngleRange filletSafeAngleRange(Point c1, float r1, Point c2, float r2, float maxRadius);

// Clamps angleDeg into the given safe range, pulled in by marginDeg extra
// on each side for numerical headroom right at the boundary.
float clampToSafeAngleRange(float angleDeg, SafeAngleRange range, float marginDeg);

// The point on the fillet circle (from filletFromAttachAngle(c1, r1, c2,
// r2, angleDeg, ...)) nearest to "target" -- i.e. the arc's visible
// "bulge"/middle point. Combines filletFromAttachAngle + circleTowardPoint,
// since callers always want this pairing together. Note: this is only an
// approximation of "the middle of the arc" -- it's the closest point to
// target, which generally does NOT land at target's own X coordinate
// (see circleAtX below for a handle that needs to sit at an exact X).
Point filletBulgePoint(Point c1, float r1, Point c2, float r2, float angleDeg, float minRadius, float maxRadius, Point target);

// The point where the circle (center, radius) crosses the vertical line
// x == targetX -- there are generally two such points (above and below
// center), and this returns whichever one is closer to "preferNear", so
// the caller lands on the correct (visible) side instead of the far side
// of the circle. Degenerates to the circle's closest approach to that
// line if targetX is outside the circle's horizontal reach entirely.
// Used to pin a handle's display position to an exact X (e.g. the
// head-butt midpoint) while keeping it genuinely ON the circle, instead
// of just using filletBulgePoint's "nearest point" (which is close to,
// but not exactly at, that X).
Point circleAtX(Point center, float radius, float targetX, Point preferNear);
