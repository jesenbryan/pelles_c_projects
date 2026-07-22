#pragma once

#include <windows.h>
#include "app.h"

typedef struct {
    int activeHandle;
    int draggingSeamArc1;
    int draggingSeamArc2;
    int draggingInner;
    int draggingKnee;
    int draggingThigh1;
    int draggingThigh2;
    int draggingFoot;
    int draggingShin1;
    int draggingShin2;

    // Rocky's rectangular torso handle -- see app.h's hoverRockyBody/
    // draggingRockyBody, mirrored here the same way the Semni-specific
    // fields above are, so drawRocky can highlight it the same way
    // drawSemniBody highlights the hip circle.
    int hoverRockyBody;
    int draggingRockyBody;

    // Which edge of Rocky's rectangle (see app.h's ROCKY_EDGE_* constants)
    // is hovered/being dragged for a resize -- mirrors app->hoverRockyEdge/
    // draggingRockyEdge.
    int hoverRockyEdge;
    int draggingRockyEdge;

    // Rocky's knee handle -- mirrors app->hoverRockyKnee/draggingRockyKnee,
    // same role as Semni's own hoverKnee/draggingKnee below.
    int hoverRockyKnee;
    int draggingRockyKnee;

    // Rocky's foot handle -- mirrors app->hoverRockyFoot/draggingRockyFoot,
    // same role as Semni's own hoverFoot/draggingFoot below.
    int hoverRockyFoot;
    int draggingRockyFoot;

    // hover-only state for the joint circle handles (hip/knee/foot/
    // head/butt), so they can highlight from a hover, not just a drag
    int hoverHip;
    int hoverKnee;
    int hoverFoot;
    int hoverHead;
    int hoverButt;

    // Shift held during the most recent mouse move -- combined with
    // hoverHip/hoverKnee to preview the shift+scroll joint rotation's
    // reach (thigh/knee, or shin/foot) in blue before any rotation
    // actually happens. Sampled live every frame (see renderRobot), not
    // cached from a mouse message, so pressing/releasing Shift while the
    // cursor sits still over a joint toggles the preview immediately.
    // Used identically in both Design > Robot mode and Simulation mode --
    // canvas.c's WM_MOUSEWHEEL Shift-gates the Simulation-mode joint
    // rotate the same way input.c's own version does, so this same field
    // means the same thing in both places.
    int shiftHeld;

    // "View Segments" toggle (app->showCircleSegments) -- when set,
    // drawSemni also overlays the full circle each fillet arc (seam/
    // thigh/shin) was trimmed from. See drawSemniCircleSegments below.
    int showSegments;

    // Index (0-5, see NUM_ROBOT_CIRCLE_SEGMENTS below) of the circle
    // segment currently under the mouse (app->hoveredCircleSegment), or -1
    // if none -- highlights that one solid/bright instead of dashed, same
    // idea as the ArcSpline canvas's hoveredSegment.
    int hoveredCircleSegment;

    // Index (0-4, see NUM_ROBOT_BODY_CIRCLES below) of the always-visible
    // body circle (head/butt/hip/knee/foot) whose CIRCUMFERENCE the mouse
    // is currently near (app->hoveredBodyCircle), or -1 if none. Separate
    // from hoverHip/hoverKnee/etc above, which only fire near the
    // center-point handle for dragging -- this fires anywhere along the
    // circle's edge, purely as hover feedback (see drawSemniBodyCircleHover).
    int hoveredBodyCircle;

    // TRUE while the user is actively dragging the whole robot in
    // Simulation mode (app->draggingRobotSim, set by canvas.c's
    // WM_LBUTTONDOWN/WM_MOUSEMOVE) -- setColor (renderer.c) checks this
    // first, before hoveringWhole/active below, so every line of the
    // robot's own outline renders blue for as long as a drag is in
    // progress, reverting to normal per-part coloring the instant it's
    // released, as feedback that it's being moved.
    int draggingWhole;

    // TRUE while the cursor is over the robot's body in Simulation mode,
    // whether or not a drag is actually in progress (app->hoveringRobotSim,
    // kept current by canvas.c's WM_MOUSEMOVE) -- setColor renders the
    // robot's outline yellow while this is true, UNLESS draggingWhole above
    // is also true, in which case dragging wins and it stays blue (this
    // flag is deliberately left stale/TRUE for the whole duration of a drag
    // -- see app.h's comment on hoveringRobotSim -- so draggingWhole taking
    // priority is what keeps the color from flickering between blue and
    // yellow mid-drag).
    int hoveringWhole;
} RenderState;

void renderApp(AppState* app, HDC hdc);

// Same scene, minus the draggable handles, and without swapping buffers --
// for exporting a frame without the editor's UI overlay. See definition
// in renderer.c for details.
void renderAppForSave(AppState* app);

// Draws the robot scene (with handles) without clearing the color buffer
// or swapping buffers, so it can be composited into a single shared frame
// alongside the ArcSpline canvas -- see main.c/canvas.c's renderCombinedFrame.
// Assumes the caller has already set Semni's own projection (graphicsOnResize)
// and enabled GL_BLEND. dimAmount scales every draw call's own alpha (0 =
// full opacity, up to 1 = heavily dimmed) so this scene can be shown as
// dimmed background context when Semni isn't the currently active editor
// mode, without a full-screen overlay that would also darken the shared
// white canvas background underneath it.
void renderRobotScene(AppState* app, float dimAmount);

void drawSemni(Semni b, RenderState* rs, int includeHandles, float opacity);

// Rocky/Stilo (see app.h) -- switcher + default poses only for now, so
// unlike drawSemni these don't draw a View Segments overlay yet regardless
// of rs->showSegments (that's a planned follow-up, see input.c's
// WM_LBUTTONDOWN/WM_MOUSEMOVE/WM_MOUSEWHEEL comments). Rocky's rectangular
// torso IS draggable/resizable now though (rs->hoverRockyBody/
// draggingRockyBody, same hip-like handle interaction as Semni's own hip
// circle) -- Stilo still has none. includeHandles gates both.
void drawRocky(Rocky b, RenderState* rs, int includeHandles, float opacity);
void drawStilo(Stilo b, RenderState* rs, int includeHandles, float opacity);

// The 6 fillet circles View Segments reveals: seam1/2 (head-butt),
// thigh1/2 (hip-knee), shin1/2 (knee-foot), in that fixed order --
// indices into computeSemniCircleSegments' output and into
// app->hoveredCircleSegment.
#define NUM_ROBOT_CIRCLE_SEGMENTS 6

typedef struct {
    PointF center; // world space
    float radius;
} CircleSegment;

// Computes the 6 fillet circles' world-space center + radius for the
// robot's CURRENT pose. Shared by drawSemniCircleSegments (rendering) and
// input.c's hover hit-test, so the two can never disagree about where
// these circles actually are -- same reasoning as reusing jointToWorld/
// rotatePoint for both drawing and hit-testing elsewhere in this codebase.
void computeSemniCircleSegments(Semni b, CircleSegment out[NUM_ROBOT_CIRCLE_SEGMENTS]);

// Matches drawArc's own 40-segment sweep sampling (renderer.c) -- 41
// points covering both endpoints. Also large enough to hold its 3-point
// straight-line fallback (nearly-collinear p0/p1/p2), which uses far fewer.
#define ARC_SAMPLE_COUNT 41

// Computes a world-space poly-line approximation of each of the 6 fillet
// ARCS -- the actual TRIMMED curve drawSemniBody/drawThigh/drawShin render
// via drawArc, not the full circle computeSemniCircleSegments above
// returns -- in the same seam1/seam2/thigh1/thigh2/shin1/shin2 order.
// outCounts[i] is how many of out[i]'s ARC_SAMPLE_COUNT slots are actually
// filled (3 in the rare nearly-collinear fallback case, ARC_SAMPLE_COUNT
// otherwise). Used by canvas.c's ground-collision check so gravity can
// stop the robot's LIMBS (not just its 5 base circles) from sinking into
// drawn ground -- the full-circle version would be far too generous, since
// a fillet's own circle is often much bigger than the trimmed arc actually
// visible on screen.
void computeSemniArcPoints(Semni b, PointF out[NUM_ROBOT_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT], int outCounts[NUM_ROBOT_CIRCLE_SEGMENTS]);

// Overlays the full circle behind each fillet arc (seam1/2, thigh1/2,
// shin1/2) -- every curve on Semni is genuinely just an arc trimmed from
// some circle (see app.h's seamArc1Angle comment), so this makes that
// literally visible, the same idea as the ArcSpline canvas's own View
// Segments overlay (canvas.c). The five body circles (head, butt, hip,
// knee, foot) aren't included here since drawSemniBody/drawThigh/drawShin
// already draw those in full unconditionally. hoveredIndex (see
// app->hoveredCircleSegment) draws that one circle solid/bright/thicker
// instead of dashed -- same idea as the ArcSpline canvas's hoveredSegment.
void drawSemniCircleSegments(Semni b, int hoveredIndex, float opacity);

// The 5 always-visible body circles, in a fixed order: head, butt,
// hip (innerCircle), knee, foot -- indices into computeSemniBodyCircles'
// output and into app->hoveredBodyCircle.
#define NUM_ROBOT_BODY_CIRCLES 5

// Computes the 5 body circles' world-space center + radius for the
// robot's CURRENT pose. Same sharing rationale as
// computeSemniCircleSegments -- used by both the hover ghost-highlight
// below and input.c's hover hit-test.
void computeSemniBodyCircles(Semni b, CircleSegment out[NUM_ROBOT_BODY_CIRCLES]);

// Draws a bright/solid "ghost circle" highlight ring on top of whichever
// body circle (see computeSemniBodyCircles) is currently hovered near its
// circumference, or does nothing if hoveredIndex is -1. Part of the same
// View Segments feature as drawSemniCircleSegments' fillet ghosts (the
// caller gates both on the same toggle) -- the body circles are already
// drawn in full unconditionally (drawSemniBody/drawThigh/drawShin), so
// this only adds hover feedback confirming "this outline is genuinely a
// full circle," once View Segments is turned on.
void drawSemniBodyCircleHover(Semni b, int hoveredIndex, float opacity);
