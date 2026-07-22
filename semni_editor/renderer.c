#include "renderer.h"
#include <GL/gl.h>
#include <math.h>
#include "geometry.h"
#include "robot.h"
#include "config.h"
#include "graphics.h"
#include "editor_mode.h"
#include <stdio.h>

// opacity multiplies every color's alpha (1.0 = fully opaque, down toward
// 0.0 = nearly invisible) -- this is how the whole Semni scene gets
// "dimmed" when it isn't the currently active editor mode (see
// renderRobotScene), instead of a post-hoc full-screen overlay. A
// full-screen black quad would also darken the shared white canvas
// background underneath it, making an otherwise-empty scene look grayed
// out everywhere rather than just its own strokes/lines fading -- scaling
// each draw's own alpha keeps the shared background untouched and only
// fades this subsystem's actual ink.
// rs->draggingWhole/hoveringWhole (Simulation mode -- see app->
// draggingRobotSim/hoveringRobotSim) force every line blue while actively
// dragging the robot, or yellow while just hovering it (dragging takes
// priority over hovering -- see hoveringWhole's comment in renderer.h for
// why). `active` (the per-part hover/drag highlight Design mode's own
// handles use, e.g. hipRotateHint/shinAffected below) now comes BEFORE
// hoveringWhole's generic yellow, though, not after -- Simulation mode
// feeds those same per-part conditions too now (Shift+hovering hip/knee
// there arms hipRotateHint/shinAffected exactly like Design mode, see
// canvas.c's WM_MOUSEWHEEL and renderRobot's rs.shiftHeld), so holding
// Shift over a joint shows exactly which limb is about to rotate, the
// same precise feedback Design mode gives, rather than the whole robot
// just uniformly flashing yellow the way a plain hover elsewhere on the
// body still does. draggingWhole still wins over everything -- an active
// whole-robot drag should never be second-guessed by a stale per-part
// hover highlight.
static void setColor(RenderState* rs, int active, float r, float g, float b, float opacity)
{
    if (rs->draggingWhole)
        glColor4f(0.2f, 0.4f, 1.0f, opacity);        // Simulation: grabbed and moving
    else if (active)
        glColor4f(r, g, b, opacity);
    else if (rs->hoveringWhole)
        glColor4f(1.0f, 0.82f, 0.0f, opacity);       // Simulation: hovered, not yet grabbed
    else
        glColor4f(0.0f, 0.0f, 0.0f, opacity);
}

void drawCircle(PointF c, float r)
{
    glBegin(GL_LINE_LOOP);
    for (float t = 0; t < 6.28318f; t += 0.05f)
    {
        glVertex2f(
            c.x + cosf(t) * r,
            c.y + sinf(t) * r
        );
    }
    glEnd();
}

// Computes the same circumcircle + sweep-direction curve drawArc renders
// between p0 and p2 (sweeping the short way around, through p1 -- p1 is
// the "bulge" handle, same role it played as the bezier control point),
// as a poly-line written into out (up to ARC_SAMPLE_COUNT points -- see
// renderer.h). Returns how many points were written: 3 if p0/p1/p2 are
// (nearly) collinear (out just holds p0, p1, p2 directly, matching
// drawArc's own straight-line fallback), ARC_SAMPLE_COUNT otherwise.
// Factored out of drawArc so other code (canvas.c's ground-collision
// check) can walk the exact same curve without duplicating this math --
// same "shared computation" reasoning as computeSemniCircleSegments/
// computeSemniBodyCircles elsewhere in this file.
static int computeArcPoints(PointF p0, PointF p1, PointF p2, PointF out[ARC_SAMPLE_COUNT])
{
    CircleF circle = circumcircle(p0, p1, p2);

    if (!circle.valid)
    {
        // p0, p1, p2 are (nearly) collinear -- fall back to straight lines
        out[0] = p0;
        out[1] = p1;
        out[2] = p2;
        return 3;
    }

    float a0 = atan2f(p0.y - circle.center.y, p0.x - circle.center.x);
    float a1 = atan2f(p1.y - circle.center.y, p1.x - circle.center.x);
    float a2 = atan2f(p2.y - circle.center.y, p2.x - circle.center.x);

    const float TWO_PI = 2.0f * 3.1415926f;

    // sweep from a0 to a2, normalized into (-PI, PI]
    float sweep = a2 - a0;
    while (sweep <= -3.1415926f) sweep += TWO_PI;
    while (sweep > 3.1415926f) sweep -= TWO_PI;

    // where does a1 fall relative to a0, using the same normalization?
    float toMid = a1 - a0;
    while (toMid <= -3.1415926f) toMid += TWO_PI;
    while (toMid > 3.1415926f) toMid -= TWO_PI;

    // if a1 isn't on the short way from a0 to a2, go the long way around
    int midOnShortPath = (sweep >= 0.0f)
        ? (toMid >= 0.0f && toMid <= sweep)
        : (toMid <= 0.0f && toMid >= sweep);

    if (!midOnShortPath)
        sweep = (sweep >= 0.0f) ? sweep - TWO_PI : sweep + TWO_PI;

    const int segments = ARC_SAMPLE_COUNT - 1;

    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;
        float a = a0 + sweep * t;

        out[i].x = circle.center.x + cosf(a) * circle.radius;
        out[i].y = circle.center.y + sinf(a) * circle.radius;
    }

    return ARC_SAMPLE_COUNT;
}

// Draws a circular arc that passes through p0, p1, and p2 (p1 is the
// "bulge" handle, same role it played as the bezier control point).
void drawArc(PointF p0, PointF p1, PointF p2)
{
    PointF pts[ARC_SAMPLE_COUNT];
    int count = computeArcPoints(p0, p1, p2, pts);

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < count; i++)
        glVertex2f(pts[i].x, pts[i].y);
    glEnd();
}

void drawHandle(PointF p, int selected, float radius, float opacity)
{
    const int segments = 64;

    // drawn semi-transparent (HANDLE_ALPHA) so handles sit lightly on top
    // of the robot instead of as solid opaque shapes -- opacity further
    // scales that down when this whole scene is the dimmed (inactive)
    // editor mode
    if (selected)
        glColor4f(1.0f, 0.85f, 0.35f, HANDLE_ALPHA * opacity);
    else
        glColor4f(1.0f, 0.0f, 0.0f, HANDLE_ALPHA * opacity);

    glBegin(GL_TRIANGLE_FAN);

    glVertex2f(p.x, p.y);

    for (int i = 0; i <= segments; i++)
    {
        float t = 2.0f * 3.1415926f * i / segments;

        glVertex2f(
            p.x + cosf(t) * radius,
            p.y + sinf(t) * radius
        );
    }

    glEnd();
}

// Draws a dashed horizontal reference line at the given Y coordinate,
// spanning the full visible viewport width. Used to help the user
// position the robot horizontally (e.g., laying down).
void drawDashedHorizontalLine(float y, float viewportHalfWidth, float opacity)
{
    glColor4f(0.6f, 0.6f, 0.6f, opacity); // medium gray, subtle

    const float dashLength = 0.08f;  // shorter dashes for finer appearance
    const float gapLength = 0.06f;   // smaller gaps
    const float segmentLength = dashLength + gapLength;

    // start from the left edge and draw dashes across
    for (float x = -viewportHalfWidth; x < viewportHalfWidth; x += segmentLength)
    {
        float dashEnd = x + dashLength;
        if (dashEnd > viewportHalfWidth)
            dashEnd = viewportHalfWidth;

        glBegin(GL_LINES);
        glVertex2f(x, y);
        glVertex2f(dashEnd, y);
        glEnd();
    }
}

void drawSemniBody(Semni b, RenderState* rs, float opacity)
{
    PointF center = getCenter(b);
    float angle = b.angle;

    PointF headCenter = rotatePoint((PointF){b.headX, b.y}, center, angle);
    PointF buttCenter = rotatePoint((PointF){b.buttX, b.y}, center, angle);
    PointF inner = rotatePoint(b.innerCircle, center, angle);

	// ---- HEAD circle ----
    // sliders are gone now -- the resize handle's hover state takes over
    // this highlight instead
    setColor(rs, rs->hoverHead, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(headCenter, b.headRadius);

	// ---- BUTT circle ----
    setColor(rs, rs->hoverButt, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(buttCenter, b.buttRadius);

	// ---- INNER circle ----
    // same "compute into a named local first" pattern seamActive uses
    // below, rather than inlining the OR directly into the setColor call
    //int innerActive = rs->draggingInner || rs->hoverHip;
    //
    // hoverHip WHILE HOLDING SHIFT is excluded here -- shift arms the
    // hip-rotate scroll, which swings the whole leg rather than the hip
    // itself, so that combo highlights the leg instead (drawThigh/
    // drawShin's hipRotateHint) and leaves the hip circle unhighlighted.
    // A plain hover (no shift) still means "scroll resizes this circle" in
    // DESIGN mode, so it keeps this blue highlight there.
    //
    // "&& !rs->hoveringWhole" excludes this in SIMULATION mode though --
    // rs->hoveringWhole is only ever set there (app->hoveringRobotSim,
    // Design mode never touches it), and there's no resize action to
    // preview in Simulation at all (scrolling a joint only ever rotates
    // it, and only with Shift held -- see canvas.c's WM_MOUSEWHEEL), so a
    // plain hover on the hip circle there should read exactly like
    // hovering anywhere else on the body: setColor's own generic yellow
    // (hoveringWhole), not this blue "you can resize this" hint that
    // doesn't apply.
    setColor(rs, rs->draggingInner || (rs->hoverHip && !rs->shiftHeld && !rs->hoveringWhole), 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(inner, b.innerRadius);

    // seam arc 1/2 (formerly "top"/"bottom"): each is a circular arc
    // internally tangent to both the head and butt circles, parameterized
    // by the angle where it attaches to the head circle (seamArc1Angle/
    // seamArc2Angle) -- the fillet's radius, center, and its other
    // tangent point (on butt) all fall out of a closed-form solve. Done
    // in local (unrotated) space, then rotated into world space at the
    // end, same pattern used elsewhere for local-frame points.
    PointF headLocal = { b.headX, b.y };
    PointF buttLocal = { b.buttX, b.y };
    PointF bodyMidLocal = { (headLocal.x + buttLocal.x) * 0.5f, (headLocal.y + buttLocal.y) * 0.5f };

    Fillet seamArc1Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
    PointF seamArc1HeadTangentLocal = circleEdge(headLocal, b.headRadius, b.seamArc1Angle); // exact, by definition of the attach angle
    PointF seamArc1ButtTangentLocal = internalTangentPoint(seamArc1Fillet.center, seamArc1Fillet.radius, buttLocal, b.buttRadius);
    // hint point for drawArc: the point on the fillet circle nearest the
    // body's midline, so it sweeps the near/visible arc instead of the
    // far side of a possibly-huge circle
    PointF seamArc1NearLocal = circleTowardPoint(seamArc1Fillet.center, seamArc1Fillet.radius, bodyMidLocal);

    Fillet seamArc2Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);
    PointF seamArc2HeadTangentLocal = circleEdge(headLocal, b.headRadius, b.seamArc2Angle);
    PointF seamArc2ButtTangentLocal = internalTangentPoint(seamArc2Fillet.center, seamArc2Fillet.radius, buttLocal, b.buttRadius);
    PointF seamArc2NearLocal = circleTowardPoint(seamArc2Fillet.center, seamArc2Fillet.radius, bodyMidLocal);

    // seamArc1HeadTangentLocal/seamArc1NearLocal/seamArc1ButtTangentLocal
    // all sit exactly on the same known circle by construction, so
    // feeding them through the existing 3-point drawArc reuses its
    // circumcircle + sweep-direction logic for free -- the near-pole
    // point just tells it which way (short or long way around) to sweep
    //
    // dragging either seam handle updates BOTH seamArc1Angle and
    // seamArc2Angle (see WM_MOUSEMOVE's mirrored assignment), so both
    // arcs highlight together too -- otherwise only the one under the
    // cursor would turn blue even though the other is visibly moving too
    int seamActive = rs->draggingSeamArc1 || rs->draggingSeamArc2;

    PointF seamArc1P0 = rotatePoint(seamArc1HeadTangentLocal, center, angle);
    PointF seamArc1P1 = rotatePoint(seamArc1NearLocal, center, angle);
    PointF seamArc1P2 = rotatePoint(seamArc1ButtTangentLocal, center, angle);

    PointF seamArc2P0 = rotatePoint(seamArc2HeadTangentLocal, center, angle);
    PointF seamArc2P1 = rotatePoint(seamArc2NearLocal, center, angle);
    PointF seamArc2P2 = rotatePoint(seamArc2ButtTangentLocal, center, angle);

    setColor(rs, seamActive, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(seamArc1P0, seamArc1P1, seamArc1P2);

    setColor(rs, seamActive, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(seamArc2P0, seamArc2P1, seamArc2P2);
}

// Draws the knee joint and the two arcs connecting it back to the inner
// joint (innerCircle), so together they read as a thigh: hip circle ->
// two tapered arcs -> knee circle.
//
// The knee/thigh points first get rotated by hipAngle around innerCircle
// (the hip joint's own rotation) and only then by the whole-body angle
// around center, so scrolling on the hip handle swings the leg without
// touching the rest of the body.
static void drawThigh(Semni b, RenderState* rs, float opacity)
{
    PointF center = getCenter(b);
    float angle = b.angle;

    PointF kneeWorld = jointToWorld(b.kneeCircle, b.innerCircle, b.hipAngle, center, angle);

    // the whole thigh (knee circle + both connecting arcs) swings with the
    // hip, so hovering the hip handle WHILE HOLDING SHIFT previews that
    // reach in blue -- shift is what actually arms the hip-rotate scroll
    // (see WM_MOUSEWHEEL), so the preview is scoped to match: a plain
    // hover (no shift) only highlights the hip's own handle.
    // Actively dragging the hip (draggingInner) also carries the whole leg
    // along rigidly (see WM_MOUSEMOVE's hipDragKneeOffset/hipDragAnkleOffset
    // handling), so that gets the same blue highlight, unconditionally.
    int hipRotateHint = (rs->hoverHip && rs->shiftHeld) || rs->draggingInner;

    // knee's own highlight mirrors the hip circle's rule in drawSemniBody:
    // hoverKnee WHILE HOLDING SHIFT arms the knee-rotate scroll (see
    // WM_MOUSEWHEEL), which swings the shin/ankle rather than the knee
    // itself, so that combo is excluded here -- it highlights the shin
    // instead (drawShin's shinAffected already includes hoverKnee && shift).
    // A plain hover (no shift) keeps the highlight.
    //
    // draggingKnee is deliberately left out here too: actively dragging the
    // knee stretches the thigh arcs (they're what visibly reacts -- see the
    // draggingKnee highlight added below), not the knee circle itself, so
    // the circle stays unhighlighted for the duration of that drag. The
    // hoverKnee term also needs !draggingKnee -- the cursor is still
    // sitting on the handle for the whole drag (that's what started it),
    // so without this the hover term alone would keep re-lighting the
    // circle every frame regardless of the exclusion above.
    //
    // "&& !rs->hoveringWhole" excludes the plain-hover resize hint in
    // Simulation mode, same reasoning as the hip circle's own version in
    // drawSemniBody -- there's no resize action there, so it should fall
    // through to setColor's generic yellow instead of this blue hint.
    setColor(rs, hipRotateHint || (rs->hoverKnee && !rs->shiftHeld && !rs->draggingKnee && !rs->hoveringWhole), 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(kneeWorld, b.kneeRadius);

    // the two thigh arcs: same tangent-fillet construction as the
    // head/butt seams, just between innerCircle and kneeCircle -- worked
    // out in the leg's own local (pre-hipAngle) frame, then rotated into
    // world space at the end, same pattern drawSemniBody uses for the
    // head/butt seams (which work in pre-body-angle local space)
    PointF axisMidLocal = { (b.innerCircle.x + b.kneeCircle.x) * 0.5f, (b.innerCircle.y + b.kneeCircle.y) * 0.5f };

    Fillet thigh1Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    PointF thigh1InnerTangentLocal = circleEdge(b.innerCircle, b.innerRadius, b.thighArc1Angle);
    PointF thigh1KneeTangentLocal = internalTangentPoint(thigh1Fillet.center, thigh1Fillet.radius, b.kneeCircle, b.kneeRadius);
    PointF thigh1NearLocal = circleTowardPoint(thigh1Fillet.center, thigh1Fillet.radius, axisMidLocal);

    // thighArc2Angle uses the concave construction (bulges inward instead
    // of outward -- see app.h's comment). The knee-side tangent point for
    // an externally-tangent fillet is just circleTowardPoint(fillet,
    // kneeCircle) instead of internalTangentPoint -- externally tangent
    // circles touch exactly on the line between their centers, which is
    // exactly what circleTowardPoint finds.
    Fillet thigh2Fillet = filletFromAttachAngleConcave(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
    PointF thigh2InnerTangentLocal = circleEdge(b.innerCircle, b.innerRadius, b.thighArc2Angle);
    PointF thigh2KneeTangentLocal = circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, b.kneeCircle);
    PointF thigh2NearLocal = circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, axisMidLocal);

    // dragging the knee circle also stretches/shrinks both thigh arcs
    // (they attach to kneeCircle, which just moved along the hip->knee
    // axis -- see WM_MOUSEMOVE's draggingKnee/constrainToAxis handling),
    // so that gets the same blue highlight as actually dragging an arc's
    // own handle, same "carries other parts along" idea as hipRotateHint
    setColor(rs, rs->draggingThigh1 || rs->draggingKnee || hipRotateHint, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(jointToWorld(thigh1InnerTangentLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh1NearLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh1KneeTangentLocal, b.innerCircle, b.hipAngle, center, angle));

    setColor(rs, rs->draggingThigh2 || rs->draggingKnee || hipRotateHint, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(jointToWorld(thigh2InnerTangentLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh2NearLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh2KneeTangentLocal, b.innerCircle, b.hipAngle, center, angle));
}

static void drawThighHandles(Semni b, RenderState* rs, float opacity)
{
    PointF center = getCenter(b);
    float angle = b.angle;

    PointF kneeWorld = jointToWorld(b.kneeCircle, b.innerCircle, b.hipAngle, center, angle);

    // thigh arc handles: pinned to the exact middle of the hip->knee axis
    // (circleAtAxisMid), same idea as the head/butt seam handles' use of
    // circleAtX -- except the axis here can point any direction, so the
    // generalized version is needed instead of pinning to a fixed X
    PointF axisMidLocal = { (b.innerCircle.x + b.kneeCircle.x) * 0.5f, (b.innerCircle.y + b.kneeCircle.y) * 0.5f };

    Fillet thigh1Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    PointF thigh1NearLocal = circleTowardPoint(thigh1Fillet.center, thigh1Fillet.radius, axisMidLocal);
    PointF thigh1MidLocal = circleAtAxisMid(thigh1Fillet.center, thigh1Fillet.radius, b.innerCircle, b.kneeCircle, thigh1NearLocal);

    Fillet thigh2Fillet = filletFromAttachAngleConcave(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
    PointF thigh2NearLocal = circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, axisMidLocal);
    PointF thigh2MidLocal = circleAtAxisMid(thigh2Fillet.center, thigh2Fillet.radius, b.innerCircle, b.kneeCircle, thigh2NearLocal);

    PointF thigh1World = jointToWorld(thigh1MidLocal, b.innerCircle, b.hipAngle, center, angle);
    PointF thigh2World = jointToWorld(thigh2MidLocal, b.innerCircle, b.hipAngle, center, angle);

    drawHandle(kneeWorld, rs->draggingKnee || rs->hoverKnee, KNEE_HANDLE_RADIUS, opacity);
    drawHandle(thigh1World, rs->draggingThigh1, THIGH_HANDLE_RADIUS, opacity);
    drawHandle(thigh2World, rs->draggingThigh2, THIGH_HANDLE_RADIUS, opacity);
}

// Continues the leg past the knee: draws the ankle joint and the two arcs
// connecting it back to kneeCircle, same tangent-fillet construction as
// drawThigh -- the shin arcs are parameterized by the angle where they
// attach to kneeCircle (shinArc1Angle/shinArc2Angle), just one joint
// further down the chain. The ankle and shin points first rotate around
// kneeCircle by kneeAngle (the knee's own rotation), then follow the
// hip/body transforms like the rest of the leg -- so scrolling on the
// knee handle swings the shin without touching the thigh, hip, or body.
static void drawShin(Semni b, RenderState* rs, float opacity)
{
    PointF center = getCenter(b);
    float angle = b.angle;

    PointF ankleWorld = nestedJointToWorld(b.ankleCircle, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

    // the shin (ankle/foot circle + both connecting arcs) swings whenever
    // either the hip OR the knee rotates -- both of those rotations are
    // now Shift-gated scrolls (see WM_MOUSEWHEEL), so the preview matches:
    // hovering either handle WHILE HOLDING SHIFT highlights the shin/foot,
    // same idea as drawThigh's hipRotateHint.
    // Actively dragging the hip (draggingInner) carries the ankle/foot
    // along too (hipDragAnkleOffset), so it gets the same highlight,
    // unconditionally, same as drawThigh's hipRotateHint.
    int shinAffected = ((rs->hoverKnee || rs->hoverHip) && rs->shiftHeld) || rs->draggingInner;

    // draggingAnkle is deliberately left out here, same reasoning as
    // draggingKnee being left out of the knee circle's own highlight in
    // drawThigh: actively dragging the ankle stretches the shin arcs
    // (they're what visibly reacts -- see the draggingAnkle highlight
    // added below), not the ankle/foot circle itself, so the circle stays
    // unhighlighted for the duration of that drag.
    //
    // hoverAnkle previews the plain-scroll resize (see WM_MOUSEWHEEL's
    // ankleWorld branch) -- no shift check needed here, unlike hoverHip/
    // hoverKnee's "&& !shiftHeld" terms, since the ankle has no Shift-
    // gated rotate action to disambiguate from (same as head/butt's
    // unconditional hover highlight in drawSemniBody). It does still need
    // "&& !draggingAnkle" though, same reasoning as the knee circle's
    // hoverKnee term above: the cursor sits on the handle for the whole
    // drag, so without the exclusion the hover term alone would keep
    // re-lighting the circle every frame despite draggingAnkle being left
    // out on purpose.
    setColor(rs, shinAffected || (rs->hoverAnkle && !rs->draggingAnkle), 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(ankleWorld, b.ankleRadius);

    // worked out in the shin's own local (pre-kneeAngle) frame, then
    // carried through the nested knee->hip->body transforms at the end,
    // same pattern drawThigh uses for its pre-hipAngle local frame
    PointF axisMidLocal = { (b.kneeCircle.x + b.ankleCircle.x) * 0.5f, (b.kneeCircle.y + b.ankleCircle.y) * 0.5f };

    Fillet shin1Fillet = filletFromAttachAngle(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
    PointF shin1KneeTangentLocal = circleEdge(b.kneeCircle, b.kneeRadius, b.shinArc1Angle);
    PointF shin1AnkleTangentLocal = internalTangentPoint(shin1Fillet.center, shin1Fillet.radius, b.ankleCircle, b.ankleRadius);
    PointF shin1NearLocal = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, axisMidLocal);

    // shinArc2Angle uses the concave construction (bulges inward instead
    // of outward -- see app.h's comment), same as thighArc2Angle. The
    // ankle-side tangent point for an externally-tangent fillet is just
    // circleTowardPoint(fillet, ankleCircle) instead of
    // internalTangentPoint -- externally tangent circles touch exactly on
    // the line between their centers, which is exactly what
    // circleTowardPoint finds.
    Fillet shin2Fillet = filletFromAttachAngleConcave(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
    PointF shin2KneeTangentLocal = circleEdge(b.kneeCircle, b.kneeRadius, b.shinArc2Angle);
    PointF shin2AnkleTangentLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, b.ankleCircle);
    PointF shin2NearLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, axisMidLocal);

    // dragging the ankle circle also stretches/shrinks both shin arcs
    // (they attach to ankleCircle, which just moved along the knee->ankle
    // axis -- see WM_MOUSEMOVE's draggingAnkle/constrainToAxis handling),
    // same "carries the other visibly-reacting parts along" idea as
    // draggingKnee getting added to the thigh arcs in drawThigh
    setColor(rs, rs->draggingShin1 || rs->draggingAnkle || shinAffected, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(nestedJointToWorld(shin1KneeTangentLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin1NearLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin1AnkleTangentLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle));

    setColor(rs, rs->draggingShin2 || rs->draggingAnkle || shinAffected, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(nestedJointToWorld(shin2KneeTangentLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin2NearLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin2AnkleTangentLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle));
}

static void drawShinHandles(Semni b, RenderState* rs, float opacity)
{
    PointF center = getCenter(b);
    float angle = b.angle;

    PointF ankleWorld = nestedJointToWorld(b.ankleCircle, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

    // shin arc handles: pinned to the exact middle of the knee->ankle axis
    // (circleAtAxisMid), same idea as the thigh handles' use of it
    PointF axisMidLocal = { (b.kneeCircle.x + b.ankleCircle.x) * 0.5f, (b.kneeCircle.y + b.ankleCircle.y) * 0.5f };

    Fillet shin1Fillet = filletFromAttachAngle(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
    PointF shin1NearLocal = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, axisMidLocal);
    PointF shin1MidLocal = circleAtAxisMid(shin1Fillet.center, shin1Fillet.radius, b.kneeCircle, b.ankleCircle, shin1NearLocal);

    Fillet shin2Fillet = filletFromAttachAngleConcave(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
    PointF shin2NearLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, axisMidLocal);
    PointF shin2MidLocal = circleAtAxisMid(shin2Fillet.center, shin2Fillet.radius, b.kneeCircle, b.ankleCircle, shin2NearLocal);

    PointF shin1World = nestedJointToWorld(shin1MidLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    PointF shin2World = nestedJointToWorld(shin2MidLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

    drawHandle(ankleWorld, rs->draggingAnkle || rs->hoverAnkle, ANKLE_HANDLE_RADIUS, opacity);
    drawHandle(shin1World, rs->draggingShin1, SHIN_HANDLE_RADIUS, opacity);
    drawHandle(shin2World, rs->draggingShin2, SHIN_HANDLE_RADIUS, opacity);
}

void drawSemniHandles(Semni b, RenderState* rs, float opacity)
{
    PointF center = getCenter(b);
    float angle = b.angle;

    PointF inner        = rotatePoint(b.innerCircle, center, angle);
    PointF headHandle   = rotatePoint((PointF){b.headX, b.y}, center, angle);
    PointF buttHandle    = rotatePoint((PointF){b.buttX, b.y}, center, angle);

    // seam attach handles: pinned to the exact midpoint between head and
    // butt on X, with Y solved from the arc's actual fillet circle at
    // that exact X (circleAtX) so the handle sits genuinely ON the
    // visible curve AND in the true middle -- filletBulgePoint alone only
    // gives the closest point to the midline, which is usually close but
    // not exactly at bodyMid.x
    PointF headLocal = { b.headX, b.y };
    PointF buttLocal = { b.buttX, b.y };
    PointF bodyMidLocal = { (headLocal.x + buttLocal.x) * 0.5f, (headLocal.y + buttLocal.y) * 0.5f };

    Fillet seamArc1Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
    Fillet seamArc2Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);

    PointF seamArc1NearLocal = circleTowardPoint(seamArc1Fillet.center, seamArc1Fillet.radius, bodyMidLocal);
    PointF seamArc2NearLocal = circleTowardPoint(seamArc2Fillet.center, seamArc2Fillet.radius, bodyMidLocal);

    PointF seamArc1MidLocal = circleAtX(seamArc1Fillet.center, seamArc1Fillet.radius, bodyMidLocal.x, seamArc1NearLocal);
    PointF seamArc2MidLocal = circleAtX(seamArc2Fillet.center, seamArc2Fillet.radius, bodyMidLocal.x, seamArc2NearLocal);

    PointF seamArc1Handle = rotatePoint(seamArc1MidLocal, center, angle);
    PointF seamArc2Handle = rotatePoint(seamArc2MidLocal, center, angle);

    drawHandle(seamArc1Handle, rs->draggingSeamArc1, ARC_HANDLE_RADIUS, opacity);
    drawHandle(seamArc2Handle, rs->draggingSeamArc2, ARC_HANDLE_RADIUS, opacity);

    // joint circle handles: highlight on hover too, not just while dragging
    drawHandle(inner,
               rs->draggingInner || rs->hoverHip,
               HIP_HANDLE_RADIUS, opacity);

    drawHandle(headHandle,
               rs->hoverHead,
               HEAD_BUTT_HANDLE_RADIUS, opacity);

    drawHandle(buttHandle,
               rs->hoverButt,
               HEAD_BUTT_HANDLE_RADIUS, opacity);
}

void computeSemniCircleSegments(Semni b, CircleSegment out[NUM_ROBOT_CIRCLE_SEGMENTS])
{
    PointF center = getCenter(b);
    float angle = b.angle;

    // seam arcs 1/2: fillets between the head and butt circles, computed
    // in the same pre-body-angle local frame drawSemniBody uses -- only
    // the fillet's CENTER needs rotating into world space (rotation
    // preserves distances, so the radius itself is unaffected).
    PointF headLocal = { b.headX, b.y };
    PointF buttLocal = { b.buttX, b.y };

    Fillet seamArc1Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
    Fillet seamArc2Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);

    out[0].center = rotatePoint(seamArc1Fillet.center, center, angle);
    out[0].radius = seamArc1Fillet.radius;

    out[1].center = rotatePoint(seamArc2Fillet.center, center, angle);
    out[1].radius = seamArc2Fillet.radius;

    // thigh arcs 1/2: fillets between innerCircle (hip) and kneeCircle,
    // computed in the hip-local (pre-hipAngle) frame, same as drawThigh --
    // jointToWorld carries the center through hipAngle, then bodyAngle.
    Fillet thigh1Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    Fillet thigh2Fillet = filletFromAttachAngleConcave(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);

    out[2].center = jointToWorld(thigh1Fillet.center, b.innerCircle, b.hipAngle, center, angle);
    out[2].radius = thigh1Fillet.radius;

    out[3].center = jointToWorld(thigh2Fillet.center, b.innerCircle, b.hipAngle, center, angle);
    out[3].radius = thigh2Fillet.radius;

    // shin arcs 1/2: fillets between kneeCircle and ankleCircle, computed
    // in the knee-local (pre-kneeAngle) frame, same as drawShin --
    // nestedJointToWorld carries the center through kneeAngle, then
    // hipAngle, then bodyAngle.
    Fillet shin1Fillet = filletFromAttachAngle(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
    Fillet shin2Fillet = filletFromAttachAngleConcave(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);

    out[4].center = nestedJointToWorld(shin1Fillet.center, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    out[4].radius = shin1Fillet.radius;

    out[5].center = nestedJointToWorld(shin2Fillet.center, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    out[5].radius = shin2Fillet.radius;
}

// Computes each of the 6 fillet arcs' actual TRIMMED curve (not the full
// circle computeSemniCircleSegments above returns) as a world-space
// poly-line, in the same seam1/seam2/thigh1/thigh2/shin1/shin2 order. The
// p0 (first-circle tangent point) / p1 (near/bulge point) / p2 (second-
// circle tangent point) triples computed here are the exact same points
// drawSemniBody/drawThigh/drawShin feed into drawArc for rendering -- see
// each of those for the derivation of which tangent-point function
// (internalTangentPoint vs circleTowardPoint) applies to which arc, and
// why. Duplicated here (rather than having those draw functions call this
// instead) so this stays a pure, side-effect-free query usable from
// outside renderer.c, same relationship computeSemniCircleSegments/
// computeSemniBodyCircles already have with their own drawSemni* callers.
void computeSemniArcPoints(Semni b, PointF out[NUM_ROBOT_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT], int outCounts[NUM_ROBOT_CIRCLE_SEGMENTS])
{
    PointF center = getCenter(b);
    float angle = b.angle;

    // seam arcs 1/2 -- same local-frame math as drawSemniBody
    PointF headLocal = { b.headX, b.y };
    PointF buttLocal = { b.buttX, b.y };
    PointF bodyMidLocal = { (headLocal.x + buttLocal.x) * 0.5f, (headLocal.y + buttLocal.y) * 0.5f };

    Fillet seamArc1Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
    PointF seamArc1P0 = rotatePoint(circleEdge(headLocal, b.headRadius, b.seamArc1Angle), center, angle);
    PointF seamArc1P1 = rotatePoint(circleTowardPoint(seamArc1Fillet.center, seamArc1Fillet.radius, bodyMidLocal), center, angle);
    PointF seamArc1P2 = rotatePoint(internalTangentPoint(seamArc1Fillet.center, seamArc1Fillet.radius, buttLocal, b.buttRadius), center, angle);
    outCounts[0] = computeArcPoints(seamArc1P0, seamArc1P1, seamArc1P2, out[0]);

    Fillet seamArc2Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);
    PointF seamArc2P0 = rotatePoint(circleEdge(headLocal, b.headRadius, b.seamArc2Angle), center, angle);
    PointF seamArc2P1 = rotatePoint(circleTowardPoint(seamArc2Fillet.center, seamArc2Fillet.radius, bodyMidLocal), center, angle);
    PointF seamArc2P2 = rotatePoint(internalTangentPoint(seamArc2Fillet.center, seamArc2Fillet.radius, buttLocal, b.buttRadius), center, angle);
    outCounts[1] = computeArcPoints(seamArc2P0, seamArc2P1, seamArc2P2, out[1]);

    // thigh arcs 1/2 -- same local-frame math as drawThigh
    PointF thighAxisMidLocal = { (b.innerCircle.x + b.kneeCircle.x) * 0.5f, (b.innerCircle.y + b.kneeCircle.y) * 0.5f };

    Fillet thigh1Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    PointF thigh1P0 = jointToWorld(circleEdge(b.innerCircle, b.innerRadius, b.thighArc1Angle), b.innerCircle, b.hipAngle, center, angle);
    PointF thigh1P1 = jointToWorld(circleTowardPoint(thigh1Fillet.center, thigh1Fillet.radius, thighAxisMidLocal), b.innerCircle, b.hipAngle, center, angle);
    PointF thigh1P2 = jointToWorld(internalTangentPoint(thigh1Fillet.center, thigh1Fillet.radius, b.kneeCircle, b.kneeRadius), b.innerCircle, b.hipAngle, center, angle);
    outCounts[2] = computeArcPoints(thigh1P0, thigh1P1, thigh1P2, out[2]);

    Fillet thigh2Fillet = filletFromAttachAngleConcave(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
    PointF thigh2P0 = jointToWorld(circleEdge(b.innerCircle, b.innerRadius, b.thighArc2Angle), b.innerCircle, b.hipAngle, center, angle);
    PointF thigh2P1 = jointToWorld(circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, thighAxisMidLocal), b.innerCircle, b.hipAngle, center, angle);
    PointF thigh2P2 = jointToWorld(circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, b.kneeCircle), b.innerCircle, b.hipAngle, center, angle);
    outCounts[3] = computeArcPoints(thigh2P0, thigh2P1, thigh2P2, out[3]);

    // shin arcs 1/2 -- same local-frame math as drawShin
    PointF shinAxisMidLocal = { (b.kneeCircle.x + b.ankleCircle.x) * 0.5f, (b.kneeCircle.y + b.ankleCircle.y) * 0.5f };

    Fillet shin1Fillet = filletFromAttachAngle(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
    PointF shin1P0 = nestedJointToWorld(circleEdge(b.kneeCircle, b.kneeRadius, b.shinArc1Angle), b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    PointF shin1P1 = nestedJointToWorld(circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, shinAxisMidLocal), b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    PointF shin1P2 = nestedJointToWorld(internalTangentPoint(shin1Fillet.center, shin1Fillet.radius, b.ankleCircle, b.ankleRadius), b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    outCounts[4] = computeArcPoints(shin1P0, shin1P1, shin1P2, out[4]);

    Fillet shin2Fillet = filletFromAttachAngleConcave(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
    PointF shin2P0 = nestedJointToWorld(circleEdge(b.kneeCircle, b.kneeRadius, b.shinArc2Angle), b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    PointF shin2P1 = nestedJointToWorld(circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, shinAxisMidLocal), b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    PointF shin2P2 = nestedJointToWorld(circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, b.ankleCircle), b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    outCounts[5] = computeArcPoints(shin2P0, shin2P1, shin2P2, out[5]);
}

// One distinguishable color per circle segment, same palette values as
// the ArcSpline canvas's own segmentGhostColor (canvas.c) for a consistent
// look between the two modes' View Segments overlays.
static void circleSegmentColor(int index, float* r, float* g, float* b)
{
    static const float palette[NUM_ROBOT_CIRCLE_SEGMENTS][3] = {
        {0.85f, 0.20f, 0.20f}, {0.20f, 0.55f, 0.85f}, {0.20f, 0.75f, 0.35f},
        {0.85f, 0.55f, 0.15f}, {0.60f, 0.30f, 0.80f}, {0.20f, 0.75f, 0.75f}
    };
    int i = index % NUM_ROBOT_CIRCLE_SEGMENTS;
    *r = palette[i][0];
    *g = palette[i][1];
    *b = palette[i][2];
}

void drawSemniCircleSegments(Semni b, int hoveredIndex, float opacity)
{
    CircleSegment segs[NUM_ROBOT_CIRCLE_SEGMENTS];
    computeSemniCircleSegments(b, segs);

    for (int i = 0; i < NUM_ROBOT_CIRCLE_SEGMENTS; i++)
    {
        float r, g, bl;
        circleSegmentColor(i, &r, &g, &bl);

        if (i == hoveredIndex)
        {
            // solid, thicker, full opacity -- reads as "this one" the same
            // way the ArcSpline canvas's hovered segment does
            glDisable(GL_LINE_STIPPLE);
            glLineWidth(2.5f);
            glColor4f(r, g, bl, 1.0f * opacity);
        }
        else
        {
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(1, 0x00FF);
            glLineWidth(1.0f);
            glColor4f(r, g, bl, 0.6f * opacity);
        }

        drawCircle(segs[i].center, segs[i].radius);
    }

    glLineWidth(1.0f);
    glDisable(GL_LINE_STIPPLE);
}

// Computes the 5 body circles' world-space center + radius for the
// robot's CURRENT pose -- same head/butt/hip/knee/ankle points
// drawSemniBody/drawThigh/drawShin already compute for their own
// rendering, factored out here so the hover ghost-highlight below and
// input.c's hit-test share the exact same geometry.
void computeSemniBodyCircles(Semni b, CircleSegment out[NUM_ROBOT_BODY_CIRCLES])
{
    PointF center = getCenter(b);
    float angle = b.angle;

    out[0].center = rotatePoint((PointF){b.headX, b.y}, center, angle);
    out[0].radius = b.headRadius;

    out[1].center = rotatePoint((PointF){b.buttX, b.y}, center, angle);
    out[1].radius = b.buttRadius;

    out[2].center = rotatePoint(b.innerCircle, center, angle);
    out[2].radius = b.innerRadius;

    out[3].center = jointToWorld(b.kneeCircle, b.innerCircle, b.hipAngle, center, angle);
    out[3].radius = b.kneeRadius;

    out[4].center = nestedJointToWorld(b.ankleCircle, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    out[4].radius = b.ankleRadius;
}

// One distinguishable color per body circle -- deliberately a different
// palette from circleSegmentColor's so a hovered body circle never reads
// as "one of the View Segments fillets" even if both happen to be
// on-screen at once.
static void bodyCircleColor(int index, float* r, float* g, float* b)
{
    static const float palette[NUM_ROBOT_BODY_CIRCLES][3] = {
        {0.95f, 0.75f, 0.15f}, {0.15f, 0.85f, 0.95f}, {0.85f, 0.25f, 0.65f},
        {0.55f, 0.85f, 0.20f}, {0.95f, 0.45f, 0.15f}
    };
    int i = index % NUM_ROBOT_BODY_CIRCLES;
    *r = palette[i][0];
    *g = palette[i][1];
    *b = palette[i][2];
}

void drawSemniBodyCircleHover(Semni b, int hoveredIndex, float opacity)
{
    if (hoveredIndex < 0 || hoveredIndex >= NUM_ROBOT_BODY_CIRCLES)
        return;

    CircleSegment segs[NUM_ROBOT_BODY_CIRCLES];
    computeSemniBodyCircles(b, segs);

    float r, g, bl;
    bodyCircleColor(hoveredIndex, &r, &g, &bl);

    // solid, thicker, full opacity, drawn on top of the circle's own
    // already-solid outline -- same "this one" visual language as
    // drawSemniCircleSegments' hovered fillet, no dashed base needed
    // since the circle itself is already always visible.
    glDisable(GL_LINE_STIPPLE);
    glLineWidth(2.5f);
    glColor4f(r, g, bl, 1.0f * opacity);
    drawCircle(segs[hoveredIndex].center, segs[hoveredIndex].radius);
    glLineWidth(1.0f);
}

void drawSemni(Semni b, RenderState* rs, int includeHandles, float opacity)
{
    drawSemniBody(b, rs, opacity);
    drawThigh(b, rs, opacity);
    drawShin(b, rs, opacity);

    // Both of these are Robot-mode editor UI overlays, not part of the
    // robot's own design, so both are hard-gated on actually being in the
    // Semni editor right now -- otherwise they'd have no business
    // appearing in the OTHER mode's rendering of this scene (e.g. the
    // dimmed background copy drawn behind the ArcSpline canvas -- see
    // renderCombinedFrame). Both are also gated on the View Segments
    // toggle itself (rs->showSegments) -- the body-circle hover highlight
    // is part of the same "reveal the circles this robot is built from"
    // feature as the fillet ghosts, so it only shows once that's turned
    // on, not unconditionally.
    BOOL segmentsVisible = rs->showSegments && (editorModeState.currentMode == EDITOR_MODE_SEMNI);

    if (segmentsVisible)
    {
        drawSemniCircleSegments(b, rs->hoveredCircleSegment, opacity);
        drawSemniBodyCircleHover(b, rs->hoveredBodyCircle, opacity);
    }

    // The draggable handles are editor UI, not part of the robot itself --
    // skip them when rendering a frame that's about to be exported, and
    // hard-gate on actually being in the Semni editor right now (same
    // reasoning as segmentsVisible above): in Simulation mode the small
    // drag-handle dots would be misleading (you can't drag an individual
    // joint there, only the whole robot -- see canvas.c's WM_LBUTTONDOWN),
    // even though hip/knee CAN be rotated there now via Shift+hover+scroll
    // (WM_MOUSEWHEEL) -- that feedback comes from drawSemniBody/drawThigh/
    // drawShin's own hover coloring above instead (rs.shiftHeld, see
    // renderRobot below), which isn't gated by handlesVisible at all. In
    // Design > Environment mode this is just the dimmed background copy of
    // the robot, not something being actively edited -- either way, the
    // handles would be misleading clutter rather
    // than useful UI.
    BOOL handlesVisible = includeHandles && (editorModeState.currentMode == EDITOR_MODE_SEMNI);

    if (handlesVisible)
    {
        drawSemniHandles(b, rs, opacity);
        drawThighHandles(b, rs, opacity);
        drawShinHandles(b, rs, opacity);
    }
}

// ---- Rocky ----
//
// Rectangular torso (a plain rotated quad outline) + a leg that's
// identical in construction to Semni's own knee-to-ankle "shin" (see
// drawShin), just hanging directly off the rectangle -- kneeCircle only
// rotates with the whole-body angle (rigidly attached to the torso, same
// as Semni's own hip/innerCircle), and kneeAngle alone swings ankleCircle
// around it (one jointToWorld nesting instead of drawShin's two, since
// there's no hip stage above it).
static void drawRockyBodyRect(Rocky b, RenderState* rs, float opacity)
{
    PointF center = getRockyCenter(b);
    float angle = b.angle;

    PointF c0 = rotatePoint((PointF){ b.bodyX - b.bodyHalfWidth, b.bodyY - b.bodyHalfHeight }, center, angle);
    PointF c1 = rotatePoint((PointF){ b.bodyX + b.bodyHalfWidth, b.bodyY - b.bodyHalfHeight }, center, angle);
    PointF c2 = rotatePoint((PointF){ b.bodyX + b.bodyHalfWidth, b.bodyY + b.bodyHalfHeight }, center, angle);
    PointF c3 = rotatePoint((PointF){ b.bodyX - b.bodyHalfWidth, b.bodyY + b.bodyHalfHeight }, center, angle);

    // highlights the same way Semni's hip circle does in drawSemniBody --
    // dragging (actively resizing/moving, whether via the body handle or
    // one of the 4 edges) or a plain hover both light it up, since
    // (unlike hoverHip) there's no Shift-gated rotate on this handle to
    // disambiguate from.
    BOOL rockyBodyActive = rs->draggingRockyBody || rs->hoverRockyBody
                         || rs->draggingRockyEdge != ROCKY_EDGE_NONE
                         || rs->hoverRockyEdge != ROCKY_EDGE_NONE;
    setColor(rs, rockyBodyActive, 0.2f, 0.4f, 1.0f, opacity);
    glBegin(GL_LINE_LOOP);
    glVertex2f(c0.x, c0.y);
    glVertex2f(c1.x, c1.y);
    glVertex2f(c2.x, c2.y);
    glVertex2f(c3.x, c3.y);
    glEnd();
}

static void drawRockyLeg(Rocky b, RenderState* rs, float opacity)
{
    PointF center = getRockyCenter(b);
    float angle = b.angle;

    PointF kneeWorld = rotatePoint(b.kneeCircle, center, angle);
    PointF ankleWorld = jointToWorld(b.ankleCircle, b.kneeCircle, b.kneeAngle, center, angle);

    // Knee circle highlights on a plain hover (no Shift, not mid-drag) --
    // same condition drawThigh uses for Semni's own knee circle, minus the
    // hip-rotate-hint/hoveringWhole terms that don't apply here (Rocky has
    // no hip stage and never appears in Simulation mode). Dragging the
    // knee (see input.c's draggingRockyKnee) repositions it freely inside
    // the rectangle -- like Semni, that's left unhighlighted here since
    // there's no separate "thigh" segment to redirect the highlight to.
    setColor(rs, rs->hoverRockyKnee && !rs->shiftHeld && !rs->draggingRockyKnee, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(kneeWorld, b.kneeRadius);

    // Shift+hover arms the bend (kneeAngle) scroll gesture below -- preview
    // it by highlighting the ankle + shin arcs, same idea as Semni's own
    // shinAffected hint for its kneeAngle bend.
    int rockyShinAffected = rs->hoverRockyKnee && rs->shiftHeld;

    setColor(rs, rockyShinAffected, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(ankleWorld, b.ankleRadius);

    PointF axisMidLocal = { (b.kneeCircle.x + b.ankleCircle.x) * 0.5f, (b.kneeCircle.y + b.ankleCircle.y) * 0.5f };

    Fillet shin1Fillet = filletFromAttachAngle(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
    PointF shin1KneeTangentLocal = circleEdge(b.kneeCircle, b.kneeRadius, b.shinArc1Angle);
    PointF shin1AnkleTangentLocal = internalTangentPoint(shin1Fillet.center, shin1Fillet.radius, b.ankleCircle, b.ankleRadius);
    PointF shin1NearLocal = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, axisMidLocal);

    Fillet shin2Fillet = filletFromAttachAngleConcave(b.kneeCircle, b.kneeRadius, b.ankleCircle, b.ankleRadius, b.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
    PointF shin2KneeTangentLocal = circleEdge(b.kneeCircle, b.kneeRadius, b.shinArc2Angle);
    PointF shin2AnkleTangentLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, b.ankleCircle);
    PointF shin2NearLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, axisMidLocal);

    setColor(rs, rockyShinAffected, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(jointToWorld(shin1KneeTangentLocal, b.kneeCircle, b.kneeAngle, center, angle),
            jointToWorld(shin1NearLocal, b.kneeCircle, b.kneeAngle, center, angle),
            jointToWorld(shin1AnkleTangentLocal, b.kneeCircle, b.kneeAngle, center, angle));

    setColor(rs, rockyShinAffected, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(jointToWorld(shin2KneeTangentLocal, b.kneeCircle, b.kneeAngle, center, angle),
            jointToWorld(shin2NearLocal, b.kneeCircle, b.kneeAngle, center, angle),
            jointToWorld(shin2AnkleTangentLocal, b.kneeCircle, b.kneeAngle, center, angle));
}

void drawRocky(Rocky b, RenderState* rs, int includeHandles, float opacity)
{
    drawRockyBodyRect(b, rs, opacity);
    drawRockyLeg(b, rs, opacity);

    // Body resize/move handle -- same visible-dot treatment drawSemniHandles
    // gives the hip circle (drawHandle, HIP_HANDLE_RADIUS), at the
    // rectangle's own center (its rotation pivot -- see getRockyCenter).
    // Gated on includeHandles/editor mode the same way Semni's own handles
    // are (see drawSemni's handlesVisible) so it doesn't show up in the
    // dimmed background copy or an exported frame.
    BOOL handlesVisible = includeHandles && (editorModeState.currentMode == EDITOR_MODE_SEMNI);
    if (handlesVisible)
    {
        PointF center = getRockyCenter(b);
        drawHandle(center, rs->draggingRockyBody || rs->hoverRockyBody, HIP_HANDLE_RADIUS, opacity);

        // Knee handle -- where the leg attaches to the rectangle (see
        // drawRockyLeg's own kneeWorld), same visible-dot treatment as
        // Semni's own knee handle (drawThigh's drawHandle(kneeWorld, ...)).
        PointF kneeWorld = rotatePoint(b.kneeCircle, center, b.angle);
        drawHandle(kneeWorld, rs->draggingRockyKnee || rs->hoverRockyKnee, KNEE_HANDLE_RADIUS, opacity);

        // One small handle at the midpoint of each of the 4 edges --
        // marks where hovering + dragging resizes just that side (see
        // input.c's hitTestRockyEdge), same idea as the body handle
        // above but sized down a notch (KNEE_HANDLE_RADIUS) so the two
        // read as different, less prominent handles.
        float angle = b.angle;

        PointF leftMid   = rotatePoint((PointF){ b.bodyX - b.bodyHalfWidth, b.bodyY }, center, angle);
        PointF rightMid  = rotatePoint((PointF){ b.bodyX + b.bodyHalfWidth, b.bodyY }, center, angle);
        PointF bottomMid = rotatePoint((PointF){ b.bodyX, b.bodyY - b.bodyHalfHeight }, center, angle);
        PointF topMid    = rotatePoint((PointF){ b.bodyX, b.bodyY + b.bodyHalfHeight }, center, angle);

        drawHandle(leftMid,   rs->draggingRockyEdge == ROCKY_EDGE_LEFT   || rs->hoverRockyEdge == ROCKY_EDGE_LEFT,   KNEE_HANDLE_RADIUS, opacity);
        drawHandle(rightMid,  rs->draggingRockyEdge == ROCKY_EDGE_RIGHT  || rs->hoverRockyEdge == ROCKY_EDGE_RIGHT,  KNEE_HANDLE_RADIUS, opacity);
        drawHandle(bottomMid, rs->draggingRockyEdge == ROCKY_EDGE_BOTTOM || rs->hoverRockyEdge == ROCKY_EDGE_BOTTOM, KNEE_HANDLE_RADIUS, opacity);
        drawHandle(topMid,    rs->draggingRockyEdge == ROCKY_EDGE_TOP    || rs->hoverRockyEdge == ROCKY_EDGE_TOP,    KNEE_HANDLE_RADIUS, opacity);
    }
}

// ---- Stilo ----
//
// Same torso as Semni (head/butt circles + two seam arcs, identical
// construction to drawSemniBody), but the leg goes straight from the hip
// (innerCircle) to the foot (ankleCircle) via one pair of tangent-fillet
// arcs instead of drawThigh+drawShin's two -- same math as drawThigh,
// just targeting ankleCircle/ankleRadius instead of kneeCircle/kneeRadius,
// and a single jointToWorld (hipAngle then bodyAngle) instead of the
// nested knee-then-hip-then-body chain Semni's shin needs.
void drawStilo(Stilo b, RenderState* rs, int includeHandles, float opacity)
{
    (void)includeHandles; // no draggable handles for Stilo yet -- see renderer.h's comment

    PointF center = getStiloCenter(b);
    float angle = b.angle;

    PointF headCenter = rotatePoint((PointF){ b.headX, b.y }, center, angle);
    PointF buttCenter = rotatePoint((PointF){ b.buttX, b.y }, center, angle);
    PointF inner = rotatePoint(b.innerCircle, center, angle);

    setColor(rs, 0, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(headCenter, b.headRadius);

    setColor(rs, 0, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(buttCenter, b.buttRadius);

    setColor(rs, 0, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(inner, b.innerRadius);

    PointF headLocal = { b.headX, b.y };
    PointF buttLocal = { b.buttX, b.y };
    PointF bodyMidLocal = { (headLocal.x + buttLocal.x) * 0.5f, (headLocal.y + buttLocal.y) * 0.5f };

    Fillet seamArc1Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
    PointF seamArc1HeadTangentLocal = circleEdge(headLocal, b.headRadius, b.seamArc1Angle);
    PointF seamArc1ButtTangentLocal = internalTangentPoint(seamArc1Fillet.center, seamArc1Fillet.radius, buttLocal, b.buttRadius);
    PointF seamArc1NearLocal = circleTowardPoint(seamArc1Fillet.center, seamArc1Fillet.radius, bodyMidLocal);

    Fillet seamArc2Fillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);
    PointF seamArc2HeadTangentLocal = circleEdge(headLocal, b.headRadius, b.seamArc2Angle);
    PointF seamArc2ButtTangentLocal = internalTangentPoint(seamArc2Fillet.center, seamArc2Fillet.radius, buttLocal, b.buttRadius);
    PointF seamArc2NearLocal = circleTowardPoint(seamArc2Fillet.center, seamArc2Fillet.radius, bodyMidLocal);

    setColor(rs, 0, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(rotatePoint(seamArc1HeadTangentLocal, center, angle),
            rotatePoint(seamArc1NearLocal, center, angle),
            rotatePoint(seamArc1ButtTangentLocal, center, angle));

    setColor(rs, 0, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(rotatePoint(seamArc2HeadTangentLocal, center, angle),
            rotatePoint(seamArc2NearLocal, center, angle),
            rotatePoint(seamArc2ButtTangentLocal, center, angle));

    // leg: hip straight to ankle, no knee stage
    PointF ankleWorld = jointToWorld(b.ankleCircle, b.innerCircle, b.hipAngle, center, angle);

    setColor(rs, 0, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(ankleWorld, b.ankleRadius);

    PointF legAxisMidLocal = { (b.innerCircle.x + b.ankleCircle.x) * 0.5f, (b.innerCircle.y + b.ankleCircle.y) * 0.5f };

    Fillet leg1Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.ankleCircle, b.ankleRadius, b.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    PointF leg1InnerTangentLocal = circleEdge(b.innerCircle, b.innerRadius, b.thighArc1Angle);
    PointF leg1AnkleTangentLocal = internalTangentPoint(leg1Fillet.center, leg1Fillet.radius, b.ankleCircle, b.ankleRadius);
    PointF leg1NearLocal = circleTowardPoint(leg1Fillet.center, leg1Fillet.radius, legAxisMidLocal);

    Fillet leg2Fillet = filletFromAttachAngleConcave(b.innerCircle, b.innerRadius, b.ankleCircle, b.ankleRadius, b.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
    PointF leg2InnerTangentLocal = circleEdge(b.innerCircle, b.innerRadius, b.thighArc2Angle);
    PointF leg2AnkleTangentLocal = circleTowardPoint(leg2Fillet.center, leg2Fillet.radius, b.ankleCircle);
    PointF leg2NearLocal = circleTowardPoint(leg2Fillet.center, leg2Fillet.radius, legAxisMidLocal);

    setColor(rs, 0, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(jointToWorld(leg1InnerTangentLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(leg1NearLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(leg1AnkleTangentLocal, b.innerCircle, b.hipAngle, center, angle));

    setColor(rs, 0, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(jointToWorld(leg2InnerTangentLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(leg2NearLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(leg2AnkleTangentLocal, b.innerCircle, b.hipAngle, center, angle));
}

static void renderRobot(AppState* app, int includeHandles, float opacity)
{
    RenderState rs;

    rs.activeHandle = app->activeHandle;
    rs.draggingSeamArc1 = app->draggingSeamArc1;
    rs.draggingSeamArc2 = app->draggingSeamArc2;
    rs.draggingInner = app->draggingInner;
    rs.draggingKnee = app->draggingKnee;
    rs.draggingThigh1 = app->draggingThigh1;
    rs.draggingThigh2 = app->draggingThigh2;
    rs.draggingAnkle = app->draggingAnkle;
    rs.draggingShin1 = app->draggingShin1;
    rs.draggingShin2 = app->draggingShin2;

    rs.hoverRockyBody = app->hoverRockyBody;
    rs.draggingRockyBody = app->draggingRockyBody;
    rs.hoverRockyEdge = app->hoverRockyEdge;
    rs.draggingRockyEdge = app->draggingRockyEdge;
    rs.hoverRockyKnee = app->hoverRockyKnee;
    rs.draggingRockyKnee = app->draggingRockyKnee;

    rs.hoverHip = app->hoverHip;
    rs.hoverKnee = app->hoverKnee;
    rs.hoverAnkle = app->hoverAnkle;
    rs.hoverHead = app->hoverHead;
    rs.hoverButt = app->hoverButt;

    rs.showSegments = app->showCircleSegments;
    rs.hoveredCircleSegment = app->hoveredCircleSegment;
    rs.hoveredBodyCircle = app->hoveredBodyCircle;
    rs.draggingWhole = app->draggingRobotSim;
    rs.hoveringWhole = app->hoveringRobotSim;

    // sampled live every frame (the render loop runs continuously -- see
    // main.c) rather than cached from WM_MOUSEMOVE's wParam, which would
    // only update while the mouse is actively moving -- pressing/releasing
    // Shift while the cursor sits still over the hip handle needs to
    // toggle the preview immediately, with no mouse movement required
    rs.shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    // Which of the three robots (see app.h's RobotKind) actually gets
    // drawn -- picked via the hRobotSelector dropdown in the control
    // panel (input.c). None of the three have a View Segments overlay yet
    // except Semni (see drawRocky/drawStilo's own comments). rs's Semni-
    // specific hover/drag fields above are simply unused unless Semni is
    // active -- harmless, since input.c only ever sets them while
    // ROBOT_KIND_SEMNI is the active kind (see its own WM_LBUTTONDOWN/
    // WM_MOUSEMOVE guards). Rocky's own hoverRockyBody/draggingRockyBody
    // above are set independently, only while Rocky is active.
    switch (app->robotScene.activeKind)
    {
        case ROBOT_KIND_ROCKY:
            drawRocky(app->robotScene.rocky, &rs, includeHandles, opacity);
            break;

        case ROBOT_KIND_STILO:
            drawStilo(app->robotScene.stilo, &rs, includeHandles, opacity);
            break;

        case ROBOT_KIND_SEMNI:
        default:
            drawSemni(app->robotScene.robot, &rs, includeHandles, opacity);
            break;
    }
}

void renderApp(AppState* app, HDC hdc)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    {
        float panX, panY;
        graphicsGetPan(&panX, &panY);
        glTranslatef(-panX, -panY, 0.0f);
    }

    // Draw ground reference line at the bottom. The viewport's
    // half-width is 1.5 units at the base zoom level, so we position
    // the line near the bottom (-1.3) to look like a ground plane
    // that helps the user position the robot for laying down poses.
    drawDashedHorizontalLine(-1.1f, 1.5f, 1.0f);

    renderRobot(app, 1, 1.0f);

    SwapBuffers(hdc);
}

// Renders the same scene as renderApp, but without the draggable handles,
// and without swapping buffers -- this only touches the back buffer, so
// whatever's currently on screen (with handles) is left completely alone.
// Meant to be called right before glReadPixels when exporting a frame, so
// the saved image doesn't include the editor's UI overlay.
void renderAppForSave(AppState* app)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    renderRobot(app, 0, 1.0f);
}

// Draws the live robot scene (with handles) into whatever's already in the
// color buffer -- no clear, no swap -- so main.c/canvas.c's
// renderCombinedFrame can composite it alongside the ArcSpline canvas in
// the same frame instead of the two subsystems taking turns owning the
// whole window. dimAmount washes out every draw call's own alpha (see
// setColor/drawHandle/drawDashedHorizontalLine) rather than covering the
// finished scene with a black overlay -- that would also darken the
// shared white canvas background underneath it, making an empty scene
// look grayed out everywhere instead of just this subsystem's own lines
// fading, which is what "Semni isn't the active editor mode" should
// actually look like.
void renderRobotScene(AppState* app, float dimAmount)
{
    float opacity = 1.0f - dimAmount;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    {
        float panX, panY;
        graphicsGetPan(&panX, &panY);
        glTranslatef(-panX, -panY, 0.0f);
    }

    // Ground reference line: a Design > Robot editing aid for laying down
    // poses (see renderApp's comment) -- hard-gated on actually being in
    // the Semni editor right now, same reasoning as handlesVisible in
    // drawSemni below: it's not useful in Simulation (which has the real
    // traced environment to pose against instead, and the robot may not
    // even be sitting on this fake line anymore once it's been dragged),
    // and it's not useful as part of Design > Environment's dimmed
    // background copy of the robot either, since nothing is being edited
    // there.
    if (editorModeState.currentMode == EDITOR_MODE_SEMNI)
        drawDashedHorizontalLine(-1.1f, 1.5f, opacity);

    renderRobot(app, 1, opacity);
}
