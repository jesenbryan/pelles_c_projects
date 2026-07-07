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
// (0 isn't a circle); the upper bound is just generous headroom -- there's
// no structural reason to cap it tighter than this
#define MIN_ARC_R 0.05f
#define MAX_ARC_R 10.0f

// scroll handle sitting at the center of the head/butt circles, used to
// resize them (same radius for both -- they're the same kind of part)
#define HEAD_BUTT_HANDLE_RADIUS 0.018f

#define THIGH_HANDLE_RADIUS 0.014f
#define SHIN_HANDLE_RADIUS  0.010f

// minimum thigh/shin length, so dragging the knee or ankle circle along
// its constrained axis can't collapse the limb to zero or flip it
// through the pivot
#define MIN_LIMB_LENGTH 0.1f
