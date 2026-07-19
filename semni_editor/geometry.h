#pragma once

#include <math.h>

// Previously 64 - too tight for a long, genuinely straight line. Every
// failed circle-fit on a (near-)collinear run forces another bisection
// (see recursiveArcFit in geometry.c), and a long straight stroke can need
// well over 100 tiny leaf segments before settling - hitting the old cap
// mid-bisection silently dropped the rest of the stroke instead of just
// producing more (later merged, see buildSegments) segments.
#define MAX_ARC_SEGMENTS 256

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point* pts;
    int count;
    int capacity;
} Path;

typedef struct {
    float cx, cy, r;
} Circle;

typedef struct {
    Point* pts;
    int count;
    double avgCurvature;

	// local
	int start;
    int end;
    Circle circle;

    // Average local stroke radius (in SOURCE IMAGE pixels), measured from
    // the pre-thinning binary raster at each of this segment's skeleton
    // points (see pipeline.c's measureLocalRadiusPx/runPipelineOnImage) --
    // thinningZhangSuen collapses every stroke down to a 1px-wide
    // centerline before arc-fitting ever runs, which is what the fit
    // needs, but it also destroys the original stroke's width in the
    // process. This is how that width gets recovered, so the "reconstructed"
    // arcs (canvas.c's segment ghost overlay, render.c's
    // renderSegmentsToImage) can be drawn at roughly the same thickness as
    // the drawing they were fit from instead of one hardcoded flat width.
    // Set to 0 for segments this wasn't computed for (e.g. a segment built
    // directly rather than via runPipelineOnImage) -- callers should treat
    // 0 as "use a default thickness."
    float avgRadiusPx;

} ArcSegment;

double curvature(Point a, Point b, Point c);

int fitCircleToPoints(Point* pts, int n, Circle* out);   // NEW

int segmentArcs(Point* path, int n, double tolerance, ArcSegment* segments, int maxSegments);

int segmentPath(Point* path, int n, int* segmentId, double threshold);

int buildSegments(Point* path, int n, ArcSegment* out);

// ---- Robot joint / limb geometry (float, normalized GL-space coords) ----
//
// Merged in from what used to be a separate geometry(1).c/.h. This uses
// PointF/CircleF rather than Point/Circle: those names are already taken
// above by the int-based pixel-path types (traced strokes, arc-spline
// fitting), and the two modules operate in completely different
// coordinate domains -- traced pixel paths vs. the robot's normalized
// [-1,1] GL-space joints -- so they were never meant to share a type,
// they just happened to both be called "geometry".

typedef struct {
    float x;
    float y;
} PointF;

typedef struct {
    PointF center;
    float radius;
    int valid; // 0 if the 3 points were (nearly) collinear and no circle exists
} CircleF;

PointF rotatePoint(PointF p, PointF center, float angleDeg);
PointF inverseRotate(PointF p, PointF center, float angleDeg);

// Rotates a point that lives in a joint-local frame (around "pivot" by
// "jointAngle") and then places the result in world space (around
// "center" by "bodyAngle"). Used so a joint (e.g. the hip) can rotate
// independently of the whole-body rotation.
PointF jointToWorld(PointF local, PointF pivot, float jointAngle, PointF center, float bodyAngle);

// Same idea as jointToWorld, but for a point that hangs off two nested
// joints (e.g. the shin, which hangs off the knee, which itself hangs
// off the hip). Rotates around "innerPivot" by "innerAngle" first (the
// knee), then around "outerPivot" by "outerAngle" (the hip), then into
// world space around "center" by "bodyAngle".
PointF nestedJointToWorld(PointF local, PointF innerPivot, float innerAngle, PointF outerPivot, float outerAngle, PointF center, float bodyAngle);

// Projects "target" onto the ray from "pivot" through "axisRef", so the
// result always lies on that fixed line. Used so dragging a joint (e.g.
// the knee or ankle circle) can only change a limb's length, not the
// direction it points in -- rotation is handled separately by the joint
// angle. "minLength" stops the limb from collapsing to zero or flipping
// through the pivot.
PointF constrainToAxis(PointF pivot, PointF axisRef, PointF target, float minLength);

// Repositions "p" so its along-axis component sits exactly at the
// midpoint of the axisStart->axisEnd segment, while keeping whatever
// perpendicular (sideways) offset "p" currently has from that axis.
// Used to keep an arc's bulge handle centered on the arc -- both when
// the user drags the handle itself (only the sideways offset should
// move) and when the limb it belongs to changes length (the handle
// should follow the new midpoint).
PointF recenterOnAxis(PointF p, PointF axisStart, PointF axisEnd);

// The signed perpendicular (sideways) distance of "p" from the
// axisStart->axisEnd line -- the same "how far to the side" quantity
// recenterOnAxis computes internally, exposed on its own. Used to turn
// mouse movement into an incremental drag delta for a handle that's
// locked to the middle of an axis but otherwise angle-driven (e.g. a
// tangent-restricted arc between two joint circles whose connecting
// axis can point any direction, so a plain vertical-Y delta -- fine
// when the axis is fixed and horizontal, like the head-butt seam --
// wouldn't track "how far the user dragged sideways" correctly).
float perpOffsetOnAxis(PointF p, PointF axisStart, PointF axisEnd);

PointF circleEdge(PointF center, float radius, float angleDeg);

int isNear(PointF a, PointF b, float radius);

CircleF circumcircle(PointF p0, PointF p1, PointF p2);

// Returns the point on the circle (center, radius) that's nearest to
// "target" -- i.e. straight out from the center, through target, to the
// circle's edge. Degenerates to an arbitrary point on the circle if
// target sits exactly on the center (no well-defined direction).
PointF circleTowardPoint(PointF center, float radius, PointF target);

// Given two circles that are (at least approximately) internally tangent --
// i.e. one sits inside the other, touching at exactly one point -- returns
// that touching point. Works out which circle is the "containing" one
// itself (whichever radius is bigger), so it doesn't matter which order
// the two circles are passed in.
PointF internalTangentPoint(PointF c1, float r1, PointF c2, float r2);

typedef struct {
    PointF center;
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
Fillet filletFromAttachAngle(PointF c1, float r1, PointF c2, float r2, float angleDeg, float minRadius, float maxRadius);

// Same idea as filletFromAttachAngle, but for a CONCAVE connection: the
// fillet circle sits in the gap between c1 and c2, externally tangent to
// both (|C-c1| == R+r1, |C-c2| == R+r2) instead of wrapping around them
// the way the convex/internal version does -- so the connecting arc
// curves INWARD, toward the c1-c2 axis, rather than bulging outward.
// Still parameterized by the exact attach angle on c1 (same circleEdge
// convention -- the tangent point on c1 is still exactly c1 + r1*u), and
// still closed-form: the second tangency constraint (to c2) reduces to
// the identical k formula filletFromAttachAngle uses, just with the
// opposite sign relating k back to the radius (R = k - r1 here, vs
// R = r1 - k there) -- so the two share nearly all their math, just
// interpreted on the other side.
Fillet filletFromAttachAngleConcave(PointF c1, float r1, PointF c2, float r2, float angleDeg, float minRadius, float maxRadius);

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
SafeAngleRange filletSafeAngleRange(PointF c1, float r1, PointF c2, float r2, float maxRadius);

// Concave counterpart to filletSafeAngleRange, for
// filletFromAttachAngleConcave -- see its .c comment for the derivation.
// centerDeg here is 180 degrees from filletSafeAngleRange's (the concave
// fillet's natural resting point faces c2, not away from it), but it's
// the same kind of range: clamping the drag angle into it keeps
// filletFromAttachAngleConcave's radius from needing to exceed maxRadius.
SafeAngleRange filletSafeAngleRangeConcave(PointF c1, float r1, PointF c2, float r2, float maxRadius);

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
PointF filletBulgePoint(PointF c1, float r1, PointF c2, float r2, float angleDeg, float minRadius, float maxRadius, PointF target);

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
PointF circleAtX(PointF center, float radius, float targetX, PointF preferNear);

// Generalization of circleAtX for an axis that isn't fixed horizontal:
// returns the point on the circle (center, radius) that lies exactly on
// the line through the axisStart->axisEnd segment's midpoint,
// perpendicular to that segment -- i.e. "the point on the circle
// directly out from the true middle of the axis", whichever direction
// the axis happens to point. circleAtX is the special case where the
// axis is horizontal (so the perpendicular line is vertical, x ==
// targetX); this version works for an axis at any angle, which the
// hip->knee (and knee->ankle) limb axis needs since it rotates with the
// joint angles. Like circleAtX, degenerates to the closest approach to
// that line if the line doesn't actually reach the circle, and picks
// whichever of the two crossing points is nearer "preferNear" so the
// caller lands on the visible side.
PointF circleAtAxisMid(PointF center, float radius, PointF axisStart, PointF axisEnd, PointF preferNear);
