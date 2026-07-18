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
static void setColor(int active, float r, float g, float b, float opacity)
{
    if (active)
        glColor4f(r, g, b, opacity);
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

// Draws a circular arc that passes through p0, p1, and p2 (p1 is the
// "bulge" handle, same role it played as the bezier control point).
void drawArc(PointF p0, PointF p1, PointF p2)
{
    CircleF circle = circumcircle(p0, p1, p2);

    if (!circle.valid)
    {
        // p0, p1, p2 are (nearly) collinear -- fall back to straight lines
        glBegin(GL_LINE_STRIP);
        glVertex2f(p0.x, p0.y);
        glVertex2f(p1.x, p1.y);
        glVertex2f(p2.x, p2.y);
        glEnd();
        return;
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

    const int segments = 40;

    glBegin(GL_LINE_STRIP);

    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;
        float a = a0 + sweep * t;

        glVertex2f(
            circle.center.x + cosf(a) * circle.radius,
            circle.center.y + sinf(a) * circle.radius
        );
    }

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
    setColor(rs->hoverHead, 0.2f, 0.4f, 1.0f, opacity);
    drawCircle(headCenter, b.headRadius);

	// ---- BUTT circle ----
    setColor(rs->hoverButt, 0.2f, 0.4f, 1.0f, opacity);
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
    // A plain hover (no shift) still means "scroll resizes this circle",
    // so it keeps the highlight.
    setColor(rs->draggingInner || (rs->hoverHip && !rs->shiftHeld), 0.2f, 0.4f, 1.0f, opacity);
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

    setColor(seamActive, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(seamArc1P0, seamArc1P1, seamArc1P2);

    setColor(seamActive, 0.2f, 0.4f, 1.0f, opacity);
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
    setColor(hipRotateHint || (rs->hoverKnee && !rs->shiftHeld && !rs->draggingKnee), 0.2f, 0.4f, 1.0f, opacity);
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
    setColor(rs->draggingThigh1 || rs->draggingKnee || hipRotateHint, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(jointToWorld(thigh1InnerTangentLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh1NearLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh1KneeTangentLocal, b.innerCircle, b.hipAngle, center, angle));

    setColor(rs->draggingThigh2 || rs->draggingKnee || hipRotateHint, 0.2f, 0.4f, 1.0f, opacity);
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
    setColor(shinAffected || (rs->hoverAnkle && !rs->draggingAnkle), 0.2f, 0.4f, 1.0f, opacity);
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
    setColor(rs->draggingShin1 || rs->draggingAnkle || shinAffected, 0.2f, 0.4f, 1.0f, opacity);
    drawArc(nestedJointToWorld(shin1KneeTangentLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin1NearLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin1AnkleTangentLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle));

    setColor(rs->draggingShin2 || rs->draggingAnkle || shinAffected, 0.2f, 0.4f, 1.0f, opacity);
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

void drawSemni(Semni b, RenderState* rs, int includeHandles, float opacity)
{
    drawSemniBody(b, rs, opacity);
    drawThigh(b, rs, opacity);
    drawShin(b, rs, opacity);

    // Hard-gated on actually being in the Semni editor right now, on top
    // of the showSegments toggle itself -- this overlay is Robot-mode
    // editor UI (like the draggable handles below), not part of the
    // robot's own design, so it has no business appearing in the OTHER
    // mode's rendering of this scene (e.g. the dimmed background copy
    // drawn behind the ArcSpline canvas -- see renderCombinedFrame).
    if (rs->showSegments && editorModeState.currentMode == EDITOR_MODE_SEMNI)
        drawSemniCircleSegments(b, rs->hoveredCircleSegment, opacity);

    // the draggable handles are editor UI, not part of the robot itself --
    // skip them when rendering a frame that's about to be exported
    if (includeHandles)
    {
        drawSemniHandles(b, rs, opacity);
        drawThighHandles(b, rs, opacity);
        drawShinHandles(b, rs, opacity);
    }
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

    rs.hoverHip = app->hoverHip;
    rs.hoverKnee = app->hoverKnee;
    rs.hoverAnkle = app->hoverAnkle;
    rs.hoverHead = app->hoverHead;
    rs.hoverButt = app->hoverButt;

    rs.showSegments = app->showCircleSegments;
    rs.hoveredCircleSegment = app->hoveredCircleSegment;

    // sampled live every frame (the render loop runs continuously -- see
    // main.c) rather than cached from WM_MOUSEMOVE's wParam, which would
    // only update while the mouse is actively moving -- pressing/releasing
    // Shift while the cursor sits still over the hip handle needs to
    // toggle the preview immediately, with no mouse movement required
    rs.shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    drawSemni(app->robotScene.robot, &rs, includeHandles, opacity);
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

    drawDashedHorizontalLine(-1.1f, 1.5f, opacity);

    renderRobot(app, 1, opacity);
}
