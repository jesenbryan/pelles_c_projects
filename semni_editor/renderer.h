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
    int draggingAnkle;
    int draggingShin1;
    int draggingShin2;

    // hover-only state for the joint circle handles (hip/knee/ankle/
    // head/butt), so they can highlight from a hover, not just a drag
    int hoverHip;
    int hoverKnee;
    int hoverAnkle;
    int hoverHead;
    int hoverButt;

    // Shift held during the most recent mouse move -- combined with
    // hoverHip to preview the shift+scroll hip rotation's reach (thigh,
    // knee, shin, foot) in blue before any rotation actually happens
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
    // body circle (head/butt/hip/knee/ankle) whose CIRCUMFERENCE the mouse
    // is currently near (app->hoveredBodyCircle), or -1 if none. Separate
    // from hoverHip/hoverKnee/etc above, which only fire near the
    // center-point handle for dragging -- this fires anywhere along the
    // circle's edge, purely as hover feedback (see drawSemniBodyCircleHover).
    int hoveredBodyCircle;

    // TRUE while the user is actively dragging the whole robot in
    // Simulation mode (app->draggingRobotSim, set by canvas.c's
    // WM_LBUTTONDOWN/WM_MOUSEMOVE) -- setColor (renderer.c) ORs this into
    // its "active" check so every line of the robot's own outline renders
    // in the hover/drag blue while a drag is in progress, reverting to
    // normal per-part coloring the instant it's released, as feedback that
    // it's being moved.
    int draggingWhole;
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

// The 6 fillet circles View Segments reveals: seam1/2 (head-butt),
// thigh1/2 (hip-knee), shin1/2 (knee-ankle), in that fixed order --
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

// Overlays the full circle behind each fillet arc (seam1/2, thigh1/2,
// shin1/2) -- every curve on Semni is genuinely just an arc trimmed from
// some circle (see app.h's seamArc1Angle comment), so this makes that
// literally visible, the same idea as the ArcSpline canvas's own View
// Segments overlay (canvas.c). The five body circles (head, butt, hip,
// knee, ankle) aren't included here since drawSemniBody/drawThigh/drawShin
// already draw those in full unconditionally. hoveredIndex (see
// app->hoveredCircleSegment) draws that one circle solid/bright/thicker
// instead of dashed -- same idea as the ArcSpline canvas's hoveredSegment.
void drawSemniCircleSegments(Semni b, int hoveredIndex, float opacity);

// The 5 always-visible body circles, in a fixed order: head, butt,
// hip (innerCircle), knee, ankle -- indices into computeSemniBodyCircles'
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
