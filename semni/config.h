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
// outward peak point; dragging it changes that arc's fillet radius
#define ARC_HANDLE_RADIUS 0.018f

// clamp range for the seam fillet radius. Must stay strictly positive
// (0 isn't a circle); the upper bound is kept modest on purpose -- past a
// certain size the arc reads as a straight line instead of a curve given
// the head/butt scale, so this is a deliberate cap, not just headroom.
// filletSafeAngleRange() uses this same constant to work out how far the
// seam handle can be dragged before hitting it, so the two stay in sync.
#define MIN_ARC_R 0.05f
#define MAX_ARC_R 6.0f

// extra angular headroom (in degrees) kept between a dragged seam handle
// and the exact edge of its valid range (see filletSafeAngleRange) -- just
// numerical safety margin, since the radius-cap bound already does the
// real work of keeping the arc from flattening out or flipping
#define ARC_ANGLE_MARGIN_DEG 2.0f

// SafeAngleRange's centerDeg (the point on the head circle farthest from
// the butt circle) is where a completely different degenerate case lives:
// at that exact angle, the arc's head tangent point, its bulge peak, and
// its butt tangent point all land exactly on the head-butt axis (bulge
// height goes to zero), and just past it the arc's bulge flips to the
// opposite side. This margin keeps the top/bottom seam handles from ever
// reaching or crossing centerDeg, so each stays on its own side (top
// bulges up, bottom bulges down) instead of flattening out and flipping
// into the other one's territory.
#define ARC_SIDE_MARGIN_DEG 10.0f

// scroll handle sitting at the center of the head/butt circles, used to
// resize them (same radius for both -- they're the same kind of part)
#define HEAD_BUTT_HANDLE_RADIUS 0.018f

#define THIGH_HANDLE_RADIUS 0.014f
#define SHIN_HANDLE_RADIUS  0.010f

// minimum thigh/shin length, so dragging the knee or ankle circle along
// its constrained axis can't collapse the limb to zero or flip it
// through the pivot
#define MIN_LIMB_LENGTH 0.1f
