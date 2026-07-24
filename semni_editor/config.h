#pragma once

#define MIN_R 0.05f
#define MAX_R 0.35f

// Rocky's rectangular torso resize range (see its body handle in
// input.c's WM_MOUSEWHEEL) -- kept separate from MIN_R/MAX_R (Semni's
// joint-CIRCLE radius range) since this bounds a rectangle's half-extents
// instead, and Rocky's default half-height (see initRockyStandingPosition,
// app_init.c) is already bigger than MAX_R.
//
// Lowered from an earlier 0.10f floor -- that made the rectangle unable to
// go much thinner/shorter than roughly half its default size. Still kept
// strictly positive (and comfortably above 0) so the rectangle can never
// collapse to a sliver or flip inside-out, just a much lower floor than
// before.
#define MIN_ROCKY_BODY_HALF 0.02f
#define MAX_ROCKY_BODY_HALF 0.70f

// How close the mouse needs to be (in local, pre-rotation world units) to
// one of Rocky's rectangle edges to count as hovering/grabbing it for a
// resize drag -- see input.c's hitTestRockyEdge.
#define ROCKY_EDGE_HIT_TOLERANCE 0.03f

// all draggable handles are drawn semi-transparent so they read as
// lightweight overlays instead of solid shapes sitting on top of the robot
#define HANDLE_ALPHA 0.45f

// handle sizes taper down the leg (hip > knee > foot) and along each limb
// (thigh bulge handles > shin bulge handles), echoing the limb getting
// thinner toward the foot.
#define HIP_HANDLE_RADIUS   0.020f
#define KNEE_HANDLE_RADIUS  0.016f
#define FOOT_HANDLE_RADIUS 0.012f

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
#define ARC_ANGLE_MARGIN_DEG 1.0f

// SafeAngleRange's centerDeg (the point on the head circle farthest from
// the butt circle) is where a completely different degenerate case lives:
// at that exact angle, the arc's head tangent point, its bulge peak, and
// its butt tangent point all land exactly on the head-butt axis (bulge
// height goes to zero), and just past it the arc's bulge flips to the
// opposite side. This margin keeps the top/bottom seam handles from ever
// reaching or crossing centerDeg, so each stays on its own side (top
// bulges up, bottom bulges down) instead of flattening out and flipping
// into the other one's territory. Kept small (rather than 0) purely for
// numerical headroom right at the boundary -- loosened from an earlier,
// much more conservative value so the handle can be dragged in much
// closer to centerDeg before the clamp kicks in.
#define ARC_SIDE_MARGIN_DEG 1.0f

// the seam handle now sits at the arc's visible middle/bulge point rather
// than the head-circle tangent point, so dragging can no longer read the
// attach angle directly off the mouse's polar angle around the head
// circle (the handle isn't on that circle anymore). Instead it drags
// incrementally along one axis (vertical mouse movement): angle = (angle
// when the drag started) + (Y moved since then) * this sensitivity.
// Bulge height isn't a monotonic function of angle across the whole
// range (it peaks partway through, then falls back toward zero near
// SafeAngleRange's centerDeg), so an absolute mouse-position-to-angle
// solve can snap on click or feel non-linear near the edges --
// incremental dragging sidesteps that entirely, since it only ever asks
// "which way and how far did the mouse move", never "what angle does
// this exact position correspond to".
#define ARC_DRAG_SENSITIVITY_DEG_PER_UNIT 200.0f

// scroll handle sitting at the center of the head/butt circles, used to
// resize them (same radius for both -- they're the same kind of part)
#define HEAD_BUTT_HANDLE_RADIUS 0.018f

#define THIGH_HANDLE_RADIUS 0.014f
#define SHIN_HANDLE_RADIUS  0.010f

// Rocky's own shin1/shin2 handles (drawRockyLeg's drawHandle calls,
// input.c's Rocky-specific isNear hit-tests) -- deliberately bigger than
// SHIN_HANDLE_RADIUS above (which Semni's own shin handles still use), per
// explicit user request to make Rocky's easier to see/grab even though
// that means the two robots no longer match exactly.
#define ROCKY_SHIN_HANDLE_RADIUS 0.016f

// thigh arcs (between innerCircle/hip and kneeCircle/knee) use the same
// tangent-restricted-fillet construction as the head/butt seam arcs (see
// MIN_ARC_R/MAX_ARC_R above), but the hip/knee circles are much smaller
// and closer together than head/butt, so they get their own clamp/margin/
// sensitivity constants instead of sharing the head/butt-tuned ones.
// MAX_SEMNI_THIGH_ARC_R is picked with the same ratio to the hip-knee
// distance (~0.52 for the default pose) that MAX_ARC_R has to the
// head-butt distance (~1.2), so the "arc flattens past this size" cap
// scales with the limb instead of being tuned for a completely different
// part.
//
// Used ONLY by Semni's own thighArc1 (the convex fillet) -- thighArc2
// bulges the other way and is capped by its own MAX_THIGH_ARC2_CONCAVE_R
// below instead. filletSafeAngleRange's halfWidthDeg (the actual draggable
// angle range WM_MOUSEMOVE's draggingThigh1 clamps to, minus
// THIGH_ARC_ANGLE_MARGIN_DEG) shrinks as this cap gets smaller, so
// lowering it is how much thigh arc 1 specifically can be dragged, without
// touching thigh arc 2's own range at all. Lowered from an earlier 2.5f --
// that let thigh arc 1 swing much further than felt right; this cuts the
// draggable range down noticeably. Tune further by adjusting this same
// constant (up for more room, down for less).
//
// Named Semni-specific (rather than a plain MAX_THIGH_ARC_R) for
// historical reasons -- Stilo used to have a simpler hip-to-foot leg (no
// knee stage) that reused this same constant for a differently-scoped
// arc. Stilo now has the exact same hip->knee thigh-arc construction as
// Semni (see app.h's Stilo comment) and shares this constant for real --
// "exactly the same as Semni" for now includes the same draggable range.
#define MIN_THIGH_ARC_R 0.05f
#define MAX_SEMNI_THIGH_ARC_R 1.2f

// A per-arc MINIMUM radius floor (tried as MIN_SEMNI_THIGH_ARC1_R, briefly)
// is NOT a safe way to shrink thigh arc 1's draggable range, unlike the MAX
// cap above -- filletSafeAngleRange (which derives maxDelta, the range
// WM_MOUSEMOVE's draggingThigh1 actually clamps the ANGLE to) only takes a
// single maxRadius parameter and guarantees every angle inside its derived
// window already solves to a radius at or under that cap, so raising
// MAX_SEMNI_THIGH_ARC_R's cap never needs filletFromAttachAngle's own
// clamp to actually engage. There's no equivalent derivation for a MINIMUM
// -- nothing stops the angle from reaching a delta whose natural radius
// falls below a raised floor, so filletFromAttachAngle's minRadius clamp
// DOES engage there, recentering the fillet circle at a radius that no
// longer matches what internalTangentPoint computes assuming tangency --
// the arc visibly detaches from the knee circle instead of just going flat.
// Use THIGH_ARC_SIDE_MARGIN_DEG below (an ANGLE restriction, not a radius
// one) to shrink the near-center portion of the range instead -- it keeps
// every reachable angle's natural radius safely inside [MIN_THIGH_ARC_R,
// MAX_SEMNI_THIGH_ARC_R] with no clamp ever firing, so tangency can't break.

// same role as ARC_ANGLE_MARGIN_DEG/ARC_SIDE_MARGIN_DEG, kept as separate
// constants so the thigh arcs' feel can be tuned independently of the seam
// arcs'. THIGH_ARC_ANGLE_MARGIN_DEG is shared with thighArc2's own maxDelta
// calc; THIGH_ARC_SIDE_MARGIN_DEG is used ONLY by thighArc1 (thighArc2's
// concave range doesn't need a one-sided lock -- see the comment below).
// Both are shared with Stilo's own thighArc1Angle drag, same reasoning as
// MAX_SEMNI_THIGH_ARC_R above.
//
// THIGH_ARC_SIDE_MARGIN_DEG is the SAFE way to shrink thigh arc 1's
// draggable range (see the MIN-radius-floor warning above for why a
// radius floor isn't): it's a pure ANGLE restriction on the near-center end
// of the range WM_MOUSEMOVE's draggingThigh1 clamps delta to, and every
// angle in the range still solves to its own natural, un-clamped radius --
// no tangency-breaking possible. Raised from 1.0f (which let the drag reach
// almost all the way to the degenerate centerDeg point) to noticeably eat
// into the near-center portion of the range, on top of the far-end trim
// MAX_SEMNI_THIGH_ARC_R above already makes -- between the two, the total
// draggable span is now roughly half of the original.
#define THIGH_ARC_ANGLE_MARGIN_DEG 2.0f
#define THIGH_ARC_SIDE_MARGIN_DEG 30.0f

// thigh arc 2 is built with filletFromAttachAngleConcave instead of the
// usual filletFromAttachAngle -- it bulges INWARD (toward the hip-knee
// axis) rather than outward, so it needs its own radius cap: the concave
// construction's radius grows toward infinity much faster as the attach
// angle nears its own singularity than the convex one does for the same
// hip/knee layout (the singularity itself is a fixed ~85.6 degrees from
// this cap's center, set by hip/knee geometry alone), so reusing
// MAX_SEMNI_THIGH_ARC_R here would let it swing almost all the way to that
// singularity before clamping kicks in. Kept deliberately tight (~51
// degree safe half-width, vs the ~85.6 degree singularity) so the pinch
// stays shallow and well clear of the point where the math breaks down --
// verified numerically that the default thighArc2Angle (-130, a 40
// degree offset from this cap's center) still lands comfortably inside.
// Reuses MIN_THIGH_ARC_R/THIGH_ARC_ANGLE_MARGIN_DEG/
// THIGH_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT for everything else -- thigh
// arc 2 doesn't need its own SIDE_MARGIN either, since its concave range
// sits on the opposite side of the hip circle from thigh arc 1's convex
// one, so there's no shared degenerate point the two need to be kept
// apart from anymore.
#define MAX_THIGH_ARC2_CONCAVE_R 1.0f

// same role as ARC_DRAG_SENSITIVITY_DEG_PER_UNIT -- the thigh handle also
// drags incrementally rather than off an absolute mouse-to-angle solve,
// for the same reason (bulge height isn't monotonic across the whole
// angle range). The one difference: since the hip->knee axis isn't fixed
// horizontal like the head-butt axis, the drag reads the mouse's
// perpendicular-to-axis movement (geometry.h's perpOffsetOnAxis) instead
// of raw vertical screen movement, so it feels the same regardless of
// which way the leg is currently posed/rotated.
#define THIGH_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT 200.0f

// the shin (kneeCircle -> footCircle) arcs use the exact same tangent-
// restricted-fillet construction as the thigh arcs above, just one joint
// further down the chain -- same MIN/MAX_ARC_R + margin + sensitivity
// pattern, kept as separate constants so the shin's feel can be tuned
// independently. MAX_SHIN_ARC_R is picked with the same ratio to the
// knee-foot distance (~0.30 for the default pose) that MAX_ARC_R has to
// the head-butt distance, same reasoning as MAX_THIGH_ARC_R.
//
// MAX_SHIN_ARC_R and SHIN_ARC_SIDE_MARGIN_DEG below are also read by
// Rocky (adjustRockyShinArcs / drawRockyLeg's shin1Fillet) and by Stilo
// (its own knee->foot shin arcs, same construction as Semni's) -- no
// split needed for Rocky because its own knee/foot layout is deliberately
// near-identical to Semni's (same 0.08/0.05 radii, ~0.30 vs ~0.2986
// knee-foot distance), so tightening
// these for Semni's interactive shin arc 1 drag leaves Rocky's shared
// default shinArc1Angle (169.821564 deg, ~79.8 deg off its own centerDeg)
// comfortably inside its own safe range too -- verified numerically for
// both robots before lowering the cap. Rocky has no shin-arc-1 drag
// handle at all, so SHIN_ARC_SIDE_MARGIN_DEG (interactive-drag-only,
// same as THIGH_ARC_SIDE_MARGIN_DEG's exclusivity to thigh arc 1) never
// touches Rocky's behavior regardless.
//
// Range reduced the same safe way as thigh arc 1 (see the MIN-radius-
// floor warning above -- never touch MIN_SHIN_ARC_R for this): lowering
// MAX_SHIN_ARC_R alone barely shrinks the practical range since the
// fillet's natural radius grows fast near the singularity, so it's
// paired with a substantial SHIN_ARC_SIDE_MARGIN_DEG increase (a pure
// angle-window restriction) to actually cut the draggable span -- from
// ~86.8 deg down to ~49.6 deg (~57%, same ratio as thigh arc 1's cut),
// leaving ~4.7 deg of clearance around the default pose's own angle so
// it never gets nudged by adjustShinArcs's clampToSafeAngleRange.
#define MIN_SHIN_ARC_R 0.05f
#define MAX_SHIN_ARC_R 1.0f
#define SHIN_ARC_ANGLE_MARGIN_DEG 2.0f
#define SHIN_ARC_SIDE_MARGIN_DEG 35.0f
#define SHIN_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT 200.0f

// same role as MAX_THIGH_ARC2_CONCAVE_R above, for shin arc 2 -- it's
// also built with filletFromAttachAngleConcave instead of the usual
// filletFromAttachAngle, bulging inward toward the knee-foot axis
// instead of outward. Verified numerically: 0.5 gives a ~69 degree safe
// half-width around its own center, comfortably clear of the singularity
// for this knee/foot layout. Reuses MIN_SHIN_ARC_R/
// SHIN_ARC_ANGLE_MARGIN_DEG/SHIN_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT for
// everything else -- no separate SIDE_MARGIN needed, same reasoning as
// thigh arc 2 (its concave range doesn't overlap shin arc 1's convex one).
#define MAX_SHIN_ARC2_CONCAVE_R 0.5f

// minimum thigh/shin length, so dragging the knee or foot circle along
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

// Robot "size" slider (top-right corner, Semni mode only -- see the
// trackbar created in input.c's WM_CREATE). A separate, permanent
// multiplier layered on top of the view zoom above (graphicsZoom/
// MIN_ZOOM/MAX_ZOOM), so setting a robot size doesn't get disturbed by
// mouse-wheel navigation zoom and vice versa. Capped at 1.0 (never
// bigger than the default size) since "zoom in past normal" is already
// covered by MAX_ZOOM -- this slider is only for shrinking the robot
// down, not magnifying it further.
#define ROBOT_SCALE_MIN 0.25f
#define ROBOT_SCALE_MAX 1.0f

// Real-world size conversion: a full 16:9 screen is defined as representing
// a 1200mm x 675mm physical area, laid out on a 32 x 18 unit grid (32:18
// reduces to 16:9, same ratio) -- 1200/32 == 675/18 == 37.5, so that's a
// single uniform scale, independent of X vs Y. This does NOT change any
// existing world-unit coordinate (robot/environment geometry, camera
// zoom/pan, hit-test radii, etc. are all untouched) -- it's purely a
// read-only conversion factor. Currently only used by input.c's live robot
// size readout (hRobotSizeLabel) -- NOT used by save.c's saveRockyAsRobArm
// Rob.txt/Arm.txt export any more (see ROCKY_EXPORT_SCALE below, a
// separate, unrelated conversion for that specific external consumer).
#define MM_PER_WORLD_UNIT 37.5f

// Rob.txt/Arm.txt export scale (see save.c's saveRockyAsRobArm) -- converts
// our world units into whatever unit the specific external "his program"
// consumer expects, so a shape sized to look right in OUR editor doesn't
// come out "way smaller" once loaded there. This is NOT derived from any
// documented spec (unlike MM_PER_WORLD_UNIT above) -- it's purely
// empirical, calibrated from one side-by-side comparison: a rectangle
// eyeballed to visually match between the two editors measured 0.312224 x
// 1.169654 in our world units, and 0.825 x 3.25 in his program's own
// units, giving a ratio of 2.6423 (width) vs 2.7786 (height) -- about 5%
// apart, most likely just eyeballing imprecision rather than a real
// non-uniform scale, since both axes should logically share one scale.
// This constant is their average (2.7105); recalibrate if a more precise
// reference measurement becomes available.
//
// Applied to BOTH files' lengths (mass centers, joint, corners/arc
// points) -- NOT weight. Originally Rob.txt-only (named ROB_EXPORT_SCALE),
// but Rob.txt and Arm.txt share one joint/unit system by construction (the
// joint is the literal same physical point in both), so using a different
// scale for each would leave the leg wildly mismatched in size relative to
// the body once his program reassembles them -- renamed and extended to
// Arm.txt once that mismatch showed up.
#define ROCKY_EXPORT_SCALE 2.7105f

// Env.txt export scale (see canvas.c's saveEnvironmentSegmentsAsTxt) --
// same idea as ROCKY_EXPORT_SCALE above (converts our world units into
// "his program"'s own units for that same external consumer), but
// calibrated differently and independently: rather than eyeballing a
// shape, this compares two "full screen" reference exports directly --
// a straight line traced across our FULL canvas width came out
// 3.876643 world units wide (Env.txt: -1.941355 to 1.935288), while his
// program's own full-screen boundary (its two vertical screen-edge
// lines) is 31.9 units wide x 18 tall (a 16:9-ish frame, so both axes
// share one scale, same reasoning as ROCKY_EXPORT_SCALE). 31.9 /
// 3.876643 = 8.228769 -- recalibrate if a more precise reference
// measurement becomes available.
#define ENV_EXPORT_SCALE 8.228769f

// Both editor subsystems (ArcSpline canvas, Semni robot editor) now draw
// into the same shared window every frame instead of only one being drawn
// at a time -- switching Design Mode just decides which one is fully
// opaque and interactive vs. which one gets this dim overlay on top of it,
// so the inactive one stays visible as background context rather than
// vanishing outright. 0 = invisible overlay (no dimming), 1 = fully black.
#define INACTIVE_MODE_DIM_ALPHA 0.65f

// Simulation mode: pressing G nudges the whole robot straight down by this
// much (world units, same translateRobot used by the whole-robot drag --
// see canvas.c's WM_KEYDOWN). "Holding G repeats it" needs no extra
// bookkeeping here -- Windows' own key-repeat keeps re-sending WM_KEYDOWN
// for as long as the key is held, at whatever repeat rate the user's
// keyboard settings specify, so each repeat is just another ordinary
// single press of this same step. Kept in the same ballpark as input.c's
// arrow-key nudge step (0.05f) so it reads as a gentle, controllable pull
// rather than an instant drop.
#define SIMULATION_GRAVITY_STEP 0.02f

// Simulation mode's ground-collision check (canvas.c's
// robotCollidesWithEnvironment) tests the robot's 5 body circles PLUS its
// 6 connecting fillet arcs (seam/thigh/shin) against the drawn
// environment. The circles have a real radius to test against, but the
// arcs are drawn as bare curves with no inherent thickness of their own --
// this is the fallback "how wide is a limb" approximation used for those,
// in the robot's own world units (same space SIMULATION_GRAVITY_STEP is
// in). Deliberately on the slim side: too generous and the robot would
// visibly stop falling while still floating above the drawn line.
#define SIMULATION_ARC_COLLISION_THICKNESS 0.03f

// applyGravityStep (canvas.c) used to just undo a WHOLE gravity step the
// instant it detected a collision -- simple, but it means the robot always
// stops up to one full step's worth of world units short of actually
// touching the ground, not right at contact. That gap is a fixed number of
// WORLD units, so it stayed sub-pixel and invisible at normal zoom, but
// zooming in close enough makes those same world units cover many more
// screen pixels, turning it into a visible floating gap -- worse still
// once auto gravity's step size grew for the acceleration fix
// (SIMULATION_AUTO_GRAVITY_MAX_VELOCITY_PER_MS). Fix: once a step
// collides, binary-search within it for how far it can actually go before
// touching, this many iterations deep -- each iteration halves the
// remaining uncertainty, so this many fully collapses even the largest
// plausible step this app can produce to a gap far smaller than a screen
// pixel at any zoom level actually reachable.
#define GRAVITY_CONTACT_SEARCH_ITERATIONS 12

// Shift+G toggles "auto gravity" in Simulation mode on/off -- while on, a
// dedicated timer (see canvas.c's AUTO_GRAVITY_TIMER_ID) nudges things
// along on its own, until it's toggled off again (or canvas.c's mode-switch
// handling turns it off for you, leaving Simulation). This is just the
// FALLBACK cadence for when nothing else is happening -- WM_TIMER messages
// are low-priority in Windows (only synthesized once the queue is
// otherwise empty), so a steady stream of WM_MOUSEMOVE while the user is
// moving the cursor can starve it and stall the fall entirely. canvas.c's
// advanceAutoGravity is real-time based (GetTickCount deltas, not "one
// step per tick"), and is ALSO called directly from WM_MOUSEMOVE, so mouse
// movement drives the fall forward itself instead of blocking it -- this
// interval only matters for keeping things moving while the mouse is
// perfectly still. Matches the hot-zone UI's own 16ms tick
// (UI_HOTZONE_INTERVAL_MS, canvas.c).
#define SIMULATION_AUTO_GRAVITY_INTERVAL_MS 16

// Auto gravity accelerates instead of falling at one flat speed -- two
// flat-speed attempts (0.02/40ms, then 0.02/16ms) both still read as
// floaty "slow motion" no matter how fast, because constant-velocity
// motion always looks artificial for something that's supposedly falling;
// real gravity noticeably speeds up as it goes, and that ramp-up is what
// actually reads as "falling" rather than "gliding." Expressed as a real
// acceleration (world units / ms^2) rather than "per tick" -- since
// advanceAutoGravity (canvas.c) now scales by the ACTUAL elapsed time
// between calls (GetTickCount deltas), not an assumed fixed tick length,
// it stays correct however often/rarely it actually gets called. Numerically
// equivalent to the old 0.004-per-16ms-tick value (0.004 / 16).
#define SIMULATION_AUTO_GRAVITY_ACCEL_PER_MS2 0.00025f

// Terminal velocity (world units / ms) the acceleration above builds up
// to and caps at, reached after ~240ms of continuous falling -- quick
// enough to feel like it's at full speed almost immediately, not a slow
// gradual creep. Reset back to 0 the instant it lands on something
// (robotCollidesWithEnvironment), so the next fall also starts from rest
// instead of carrying over speed through solid ground. Numerically
// equivalent to the old 0.08-per-16ms-tick cap (0.08 / 16).
#define SIMULATION_AUTO_GRAVITY_MAX_VELOCITY_PER_MS 0.005f

// advanceAutoGravity (canvas.c) can be called from arbitrarily far-apart
// real-time gaps -- e.g. the window was blocked on something else for a
// while, or WM_TIMER got badly starved for longer than usual -- so the
// elapsed time it scales by is capped at this many milliseconds per call,
// otherwise a long-enough gap would make the robot teleport a large
// distance downward in one jump instead of visibly falling through it.
#define SIMULATION_AUTO_GRAVITY_MAX_DT_MS 100

// Bottom-left "AUTO GRAVITY ON"/"AUTO GRAVITY OFF" HUD toast (canvas.c's
// canvasRenderFrame), shown for a brief hold at full opacity then faded
// out, every time Shift+G toggles auto gravity. Both durations are in
// milliseconds, measured from GetTickCount() at the moment of the toggle.
#define SIMULATION_GRAVITY_TOAST_HOLD_MS 900
#define SIMULATION_GRAVITY_TOAST_FADE_MS 500

// Slope response: once applyGravityStep (canvas.c) lands the robot on
// something, it also nudges the whole-body angle (Semni.angle, the same
// field rotatePoint applies around getCenter() for every body part) toward
// the tilt of whichever environment edge it's actually resting on, instead
// of always staying bolt upright the way flat-ground-only physics would.
// It's a RIGID whole-body lean (same simplification the rest of Simulation's
// physics already makes -- one rigid translate/rotate, no per-joint IK), so
// on a sloped arc segment the robot settles lying flush against it rather
// than standing perfectly vertical on an incline.
//
// Closed as a FRACTION of the remaining angle gap per landed tick (rather
// than jumping straight to the target), so it reads as settling into the
// slope over a few ticks instead of snapping instantly -- most visible right
// after first touchdown, converges to ~0 remaining diff within well under a
// second at SIMULATION_AUTO_GRAVITY_INTERVAL_MS's cadence.
#define SIMULATION_SLOPE_ALIGN_RATE 0.18f

// Upper bound on how many degrees SIMULATION_SLOPE_ALIGN_RATE is allowed to
// rotate the robot in a single landed tick, independent of how large the
// remaining angle gap is -- keeps a big gap (e.g. the robot landing at a
// very different angle than the slope, like right after a fall) from
// visibly snapping/spinning in one frame instead of settling smoothly.
#define SIMULATION_SLOPE_ALIGN_MAX_STEP_DEG 4.0f

// After nudging the angle above, the robot's limbs can end up very slightly
// inside the ground (rotating a rigid body around its torso-level center
// can push a foot/knee down a hair, even though the body as a whole was
// already resting on the surface) -- resolveUpwardIfPenetrating (canvas.c)
// searches for the smallest upward correction, up to this many world units,
// that clears it again. Kept small on purpose: the rotation step per tick
// is already small (see MAX_STEP_DEG above), so any resulting penetration
// is always tiny -- this only ever needs to cover that, not a whole
// gravity-step's worth of overlap.
#define SIMULATION_SLOPE_CORRECTION_MAX 0.05f

// "Slow Motion" toggle button (Simulation mode only -- see canvas.c's
// hSlowMotionBtn/ID_SLOW_MOTION). While checked, simTimeScale is set to
// this instead of 1.0, and both advanceAutoGravity's elapsed-time
// integration AND applyGravityStep's slope-alignment step (see
// SIMULATION_SLOPE_ALIGN_RATE above) are scaled by it -- so both the fall
// itself and the tipping-into-a-slope settle noticeably slower, without
// changing anything about the underlying physics (same acceleration curve,
// same terminal velocity, same alignment behavior, just stretched out over
// more real time so it's easier to watch closely).
#define SIMULATION_SLOW_MOTION_SCALE 0.2f

// Simulation mode: hovering the hip or knee's own visible body circle and
// Shift+scrolling the wheel rotates that joint's angle (hipAngle/kneeAngle)
// directly -- see canvas.c's simHoveredJoint/findHoveredJointSim and its
// WM_MOUSEWHEEL handling. Shift-gated for the same reason Design > Robot
// mode's own hip/knee scroll-rotate is (input.c): a plain scroll while the
// cursor happens to be over a joint should still zoom, not surprise-rotate
// a limb. Degrees per wheel notch, same value Design mode's own
// Shift+scroll joint rotate uses, so the feel is consistent between the
// two places it's available.
#define SIMULATION_JOINT_ROTATE_STEP_DEG 2.0f

// Simulation mode: plain Left/Right (no Shift) rotates the WHOLE robot
// (Semni.angle) -- see canvas.c's WM_KEYDOWN. Same value (and Left =
// positive / Right = negative sign convention) as Design > Robot mode's
// own Left/Right nudge (input.c's WM_KEYDOWN, `angle += 2.0f` / `-=
// 2.0f`), so a press feels identical between the two editors. Joint
// rotation (hip/knee) is the Shift-gated action instead -- see
// SIMULATION_JOINT_ROTATE_STEP_DEG and WM_MOUSEWHEEL's Shift check.
#define SIMULATION_WHOLE_BODY_ROTATE_STEP_DEG 2.0f

// ---- Scripted gait ("Walk" toggle, Shift+W -- see canvas.c's gaitActive/
// advanceGait/gaitCycle) ----
//
// Semni has one leg, not two (no second innerCircle/kneeCircle/footCircle
// to alternate onto) -- so this isn't a classic alternating bipedal walk,
// it's a single-leg hop/pivot cycle: crouch, push off, swing the leg
// forward through the air, land, repeat. canvas.c's gaitCycle table drives
// hipAngle/kneeAngle/whole-body angle as DELTAS from whatever pose the
// robot was in the moment Walk was toggled on (gaitBaseline), plus a
// vertical "hop" bob and continuous forward world-X translation -- so
// whatever pose you've already dialed in (Design > Robot, or Simulation's
// own Shift+scroll/arrow-key posing) becomes the neutral stance the gait
// cycles around, rather than assuming any one fixed geometry.
//
// This is a first-pass, purely SCRIPTED cycle -- it does not look at the
// drawn environment at all (no foot-placement/terrain-following yet, see
// the "what's next" discussion this came out of), so the numbers below are
// a reasonable starting guess, not a tuned result: expect to adjust them
// (including possibly FLIPPING a sign, if a swing reads as going the wrong
// direction once you can actually see it) after watching it run once.

// How long one full crouch -> push -> swing -> land cycle takes, in real
// milliseconds, before Slow Motion's simTimeScale scales it down the same
// way it scales auto-gravity's fall speed.
#define SIMULATION_GAIT_CYCLE_MS 1200.0f

// Forward world-X distance covered per FULL cycle (continuous, not a
// discrete jump at the loop boundary -- see advanceGait's phaseUnwrapped).
#define SIMULATION_GAIT_STEP_LENGTH 0.18f

// Peak vertical "hop" bob, world units, relative to gaitBaseline's Y --
// purely oscillatory (returns to 0 every cycle), separate from the
// continuously-accumulating forward step length above.
#define SIMULATION_GAIT_HOP_HEIGHT 0.06f

// How far the hip swings the leg forward/back from the baseline pose over
// the course of a cycle, in degrees.
#define SIMULATION_GAIT_HIP_SWING_DEG 24.0f

// How far the knee bends (relative to baseline) during the crouch and the
// forward swing (bent to clear the ground), in degrees.
#define SIMULATION_GAIT_KNEE_BEND_DEG 30.0f

// Small whole-body forward lean during the push/swing portion of the
// cycle, in degrees -- purely cosmetic (sells the "leaning into the hop"
// look), independent of hipAngle/kneeAngle.
#define SIMULATION_GAIT_BODY_LEAN_DEG 6.0f
