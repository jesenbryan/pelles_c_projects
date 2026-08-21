#pragma once

#define MIN_R 0.05f
#define MAX_R 0.35f

// all draggable handles are drawn semi-transparent so they read as
// lightweight overlays instead of solid shapes sitting on top of the robot
#define HANDLE_ALPHA 0.45f

// handle sizes taper down the leg (hip > knee > ankle) and along each limb
// (thigh bulge handles > shin bulge handles), echoing the limb getting
// thinner toward the foot.
#define HIP_HANDLE_RADIUS   0.020f
#define KNEE_HANDLE_RADIUS  0.016f
#define ANKLE_HANDLE_RADIUS 0.012f

// handle for the top/bottom seam fillet arcs -- sits at each arc's
// outward peak point; dragging it changes that arc's bulge distance
#define ARC_HANDLE_RADIUS 0.018f

// clamp range (world units) for seamArc1Bulge/seamArc2Bulge -- the
// perpendicular distance the fillet's bezier control point sits from the
// head-butt axis midpoint (see geometry.h's axisBulgePoint). No tangency
// solve is involved anymore, so there's no singularity to avoid -- this
// is just a plain magnitude clamp, kept strictly positive (0 would flatten
// the seam into a straight line) with a modest upper bound (past a
// certain size the curve reads as an exaggerated loop rather than a
// smooth seam, given the head/butt scale). seamArc1Bulge is clamped to
// [MIN_ARC_BULGE, MAX_ARC_BULGE] and seamArc2Bulge to the negated range
// -- that sign lock is what keeps the top seam bulging up and the bottom
// seam bulging down instead of one drifting across into the other's side.
#define MIN_ARC_BULGE 0.05f
#define MAX_ARC_BULGE 0.90f

// scroll handle sitting at the center of the head/butt circles, used to
// resize them (same radius for both -- they're the same kind of part)
#define HEAD_BUTT_HANDLE_RADIUS 0.018f

#define THIGH_HANDLE_RADIUS 0.014f
#define SHIN_HANDLE_RADIUS  0.010f

// thigh arcs (between innerCircle/hip and kneeCircle/knee) use the same
// fixed-attach-point construction as the head/butt seam arcs above, just
// with their own clamp range since the hip/knee circles are much smaller
// and closer together than head/butt. thighArc1Bulge is the outer
// (silhouette) side, clamped to the NEGATIVE range [-MAX_THIGH_ARC_BULGE,
// -MIN_THIGH_ARC_BULGE] -- which side ends up negative vs. positive here
// just falls out of the hip->knee axis's own direction (see
// circleHalfPoint's side convention), not a meaningful convex/concave
// distinction anymore, since both thigh arcs now use the exact same
// fixed-attach + bulge construction.
#define MIN_THIGH_ARC_BULGE 0.03f
#define MAX_THIGH_ARC_BULGE 0.40f

// thighArc2Bulge is the inner (crotch-side pinch) arc, clamped to the
// POSITIVE range [MIN_THIGH_ARC2_BULGE, MAX_THIGH_ARC2_BULGE] -- kept
// tighter than thighArc1's range since a deep pinch here reads as a much
// more exaggerated crease than the same bulge distance does on the outer
// silhouette side.
#define MIN_THIGH_ARC2_BULGE 0.02f
#define MAX_THIGH_ARC2_BULGE 0.30f

// the shin (kneeCircle -> ankleCircle) arcs use the exact same
// fixed-attach + bulge construction as the thigh arcs above, just one
// joint further down the chain -- same outer/inner clamp pattern, kept as
// separate constants so the shin's feel can be tuned independently.
#define MIN_SHIN_ARC_BULGE 0.02f
#define MAX_SHIN_ARC_BULGE 0.25f
#define MIN_SHIN_ARC2_BULGE 0.015f
#define MAX_SHIN_ARC2_BULGE 0.20f

// minimum thigh/shin length, so dragging the knee or ankle circle along
// its constrained axis can't collapse the limb to zero or flip it
// through the pivot
#define MIN_LIMB_LENGTH 0.1f

// canvas view zoom (scroll wheel over empty space, i.e. not over a
// handle). Bounds are kept modest so the robot can't be scrolled away to
// an invisible speck or blown up past where the geometry is meaningful.
// STEP is multiplicative per wheel notch so repeated scrolling feels
// smooth rather than jumpy.
#define MIN_ZOOM 0.25f
#define MAX_ZOOM 8.0f
#define ZOOM_STEP 1.1f
