#include "renderer.h"
#include <GL/gl.h>
#include <math.h>
#include "geometry.h"
#include "robot.h"
#include "config.h"
#include <stdio.h>

static void setColor(int active, float r, float g, float b)
{
    if (active)
        glColor3f(r, g, b);
    else
        glColor3f(0.0f, 0.0f, 0.0f);
}

void drawCircle(Point c, float r)
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

// Draws a pure quadratic bezier curve with p0/p2 as its two endpoints and
// p1 as its actual control point (not solved backwards to land ON the
// curve -- p1 just pulls the curve toward itself, standard bezier
// behavior, so the curve generally does NOT pass through p1 itself,
// unlike the old circular arc). p0/p2 now come from geometry.h's
// circleHalfPoint -- a fixed "quarter turn around from the axis" point on
// each circle, with no tangency solve involved at all -- and p1 comes
// from axisBulgePoint, a freely-draggable perpendicular offset from the
// axis midpoint. Callers just hand this whichever three points they've
// built; the function itself doesn't care where they came from.
void drawFilletCurve(Point p0, Point p1, Point p2)
{
    const int segments = 40;

    glBegin(GL_LINE_STRIP);

    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;
        float u = 1.0f - t;

        // standard quadratic bezier basis: (1-t)^2*p0 + 2*(1-t)*t*p1 + t^2*p2
        float bx = u * u * p0.x + 2.0f * u * t * p1.x + t * t * p2.x;
        float by = u * u * p0.y + 2.0f * u * t * p1.y + t * t * p2.y;

        glVertex2f(bx, by);
    }

    glEnd();
}

void drawHandle(Point p, int selected, float radius)
{
    const int segments = 64;

    // drawn semi-transparent (HANDLE_ALPHA) so handles sit lightly on top
    // of the robot instead of as solid opaque shapes
    if (selected)
        glColor4f(1.0f, 0.85f, 0.35f, HANDLE_ALPHA);
    else
        glColor4f(1.0f, 0.0f, 0.0f, HANDLE_ALPHA);

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
void drawDashedHorizontalLine(float y, float viewportHalfWidth)
{
    glColor3f(0.6f, 0.6f, 0.6f); // medium gray, subtle
    
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

void drawSemniBody(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point headCenter = rotatePoint((Point){b.headX, b.y}, center, angle);
    Point buttCenter = rotatePoint((Point){b.buttX, b.y}, center, angle);
    Point inner = rotatePoint(b.innerCircle, center, angle);

	// ---- HEAD circle ----
    // sliders are gone now -- the resize handle's hover state takes over
    // this highlight instead
    setColor(rs->hoverHead, 0.2f, 0.4f, 1.0f);
    drawCircle(headCenter, b.headRadius);

	// ---- BUTT circle ----
    setColor(rs->hoverButt, 0.2f, 0.4f, 1.0f);
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
    setColor(rs->draggingInner || (rs->hoverHip && !rs->shiftHeld), 0.2f, 0.4f, 1.0f);
    drawCircle(inner, b.innerRadius);

    // seam arc 1/2 (formerly "top"/"bottom"): each a bezier curve between
    // the head and butt circles, with NO tangency solve at all. Each
    // circle's attach point is fixed at exactly a quarter turn (90
    // degrees) around from the head-butt axis (circleHalfPoint) -- seam
    // arc 1 uses side +1, seam arc 2 side -1, so together they split each
    // circle into exact halves. Done in local (unrotated) space, then
    // rotated into world space at the end, same pattern used elsewhere
    // for local-frame points.
    Point headLocal = { b.headX, b.y };
    Point buttLocal = { b.buttX, b.y };

    Point seamArc1HeadLocal = circleHalfPoint(headLocal, b.headRadius, headLocal, buttLocal, +1);
    Point seamArc1ButtLocal = circleHalfPoint(buttLocal, b.buttRadius, headLocal, buttLocal, +1);
    Point seamArc1BulgeLocal = axisBulgePoint(headLocal, buttLocal, b.seamArc1Bulge);

    Point seamArc2HeadLocal = circleHalfPoint(headLocal, b.headRadius, headLocal, buttLocal, -1);
    Point seamArc2ButtLocal = circleHalfPoint(buttLocal, b.buttRadius, headLocal, buttLocal, -1);
    Point seamArc2BulgeLocal = axisBulgePoint(headLocal, buttLocal, b.seamArc2Bulge);

    // seamArc1HeadLocal/seamArc1BulgeLocal/seamArc1ButtLocal feed straight
    // into drawFilletCurve as p0/p1/p2 -- the two fixed attach points and
    // the draggable bulge control point
    //
    // dragging either seam handle updates BOTH seamArc1Bulge and
    // seamArc2Bulge (see WM_MOUSEMOVE's mirrored assignment), so both
    // arcs highlight together too -- otherwise only the one under the
    // cursor would turn blue even though the other is visibly moving too
    int seamActive = rs->draggingSeamArc1 || rs->draggingSeamArc2;

    Point seamArc1P0 = rotatePoint(seamArc1HeadLocal, center, angle);
    Point seamArc1P1 = rotatePoint(seamArc1BulgeLocal, center, angle);
    Point seamArc1P2 = rotatePoint(seamArc1ButtLocal, center, angle);

    Point seamArc2P0 = rotatePoint(seamArc2HeadLocal, center, angle);
    Point seamArc2P1 = rotatePoint(seamArc2BulgeLocal, center, angle);
    Point seamArc2P2 = rotatePoint(seamArc2ButtLocal, center, angle);

    setColor(seamActive, 0.2f, 0.4f, 1.0f);
    drawFilletCurve(seamArc1P0, seamArc1P1, seamArc1P2);

    setColor(seamActive, 0.2f, 0.4f, 1.0f);
    drawFilletCurve(seamArc2P0, seamArc2P1, seamArc2P2);
}

// Draws the knee joint and the two arcs connecting it back to the inner
// joint (innerCircle), so together they read as a thigh: hip circle ->
// two tapered arcs -> knee circle.
//
// The knee/thigh points first get rotated by hipAngle around innerCircle
// (the hip joint's own rotation) and only then by the whole-body angle
// around center, so scrolling on the hip handle swings the leg without
// touching the rest of the body.
static void drawThigh(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point kneeWorld = jointToWorld(b.kneeCircle, b.innerCircle, b.hipAngle, center, angle);

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
    setColor(hipRotateHint || (rs->hoverKnee && !rs->shiftHeld && !rs->draggingKnee), 0.2f, 0.4f, 1.0f);
    drawCircle(kneeWorld, b.kneeRadius);

    // the two thigh arcs: same fixed-attach-point construction as the
    // head/butt seams, just between innerCircle and kneeCircle -- worked
    // out in the leg's own local (pre-hipAngle) frame, then rotated into
    // world space at the end, same pattern drawSemniBody uses for the
    // head/butt seams (which work in pre-body-angle local space).
    // thighArc1 (outer silhouette) uses side -1, thighArc2 (inner pinch)
    // uses side +1 -- see app.h's comment for why the sides land this way.
    Point thigh1InnerLocal = circleHalfPoint(b.innerCircle, b.innerRadius, b.innerCircle, b.kneeCircle, -1);
    Point thigh1KneeLocal = circleHalfPoint(b.kneeCircle, b.kneeRadius, b.innerCircle, b.kneeCircle, -1);
    Point thigh1BulgeLocal = axisBulgePoint(b.innerCircle, b.kneeCircle, b.thighArc1Bulge);

    Point thigh2InnerLocal = circleHalfPoint(b.innerCircle, b.innerRadius, b.innerCircle, b.kneeCircle, +1);
    Point thigh2KneeLocal = circleHalfPoint(b.kneeCircle, b.kneeRadius, b.innerCircle, b.kneeCircle, +1);
    Point thigh2BulgeLocal = axisBulgePoint(b.innerCircle, b.kneeCircle, b.thighArc2Bulge);

    // dragging the knee circle also stretches/shrinks both thigh arcs
    // (they attach to kneeCircle, which just moved along the hip->knee
    // axis -- see WM_MOUSEMOVE's draggingKnee/constrainToAxis handling),
    // so that gets the same blue highlight as actually dragging an arc's
    // own handle, same "carries other parts along" idea as hipRotateHint
    setColor(rs->draggingThigh1 || rs->draggingKnee || hipRotateHint, 0.2f, 0.4f, 1.0f);
    drawFilletCurve(jointToWorld(thigh1InnerLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh1BulgeLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh1KneeLocal, b.innerCircle, b.hipAngle, center, angle));

    setColor(rs->draggingThigh2 || rs->draggingKnee || hipRotateHint, 0.2f, 0.4f, 1.0f);
    drawFilletCurve(jointToWorld(thigh2InnerLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh2BulgeLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh2KneeLocal, b.innerCircle, b.hipAngle, center, angle));
}

static void drawThighHandles(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point kneeWorld = jointToWorld(b.kneeCircle, b.innerCircle, b.hipAngle, center, angle);

    // thigh arc handles: sit exactly at the curve's bezier control point
    // (axisBulgePoint) -- that point IS the draggable parameter now (no
    // tangency solve to derive it from), so the handle can just be drawn
    // where the curve is actually being pulled toward, with no separate
    // "find the point on the fillet circle nearest the axis" step needed
    Point thigh1MidLocal = axisBulgePoint(b.innerCircle, b.kneeCircle, b.thighArc1Bulge);
    Point thigh2MidLocal = axisBulgePoint(b.innerCircle, b.kneeCircle, b.thighArc2Bulge);

    Point thigh1World = jointToWorld(thigh1MidLocal, b.innerCircle, b.hipAngle, center, angle);
    Point thigh2World = jointToWorld(thigh2MidLocal, b.innerCircle, b.hipAngle, center, angle);

    drawHandle(kneeWorld, rs->draggingKnee || rs->hoverKnee, KNEE_HANDLE_RADIUS);
    drawHandle(thigh1World, rs->draggingThigh1, THIGH_HANDLE_RADIUS);
    drawHandle(thigh2World, rs->draggingThigh2, THIGH_HANDLE_RADIUS);
}

// Continues the leg past the knee: draws the ankle joint and the two arcs
// connecting it back to kneeCircle, same fixed-attach-point construction
// as drawThigh -- the shin arcs' bulge control points are
// shinArc1Bulge/shinArc2Bulge, just one joint further down the chain. The
// ankle and shin points first rotate around kneeCircle by kneeAngle (the
// knee's own rotation), then follow the hip/body transforms like the rest
// of the leg -- so scrolling on the knee handle swings the shin without
// touching the thigh, hip, or body.
static void drawShin(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point ankleWorld = nestedJointToWorld(b.ankleCircle, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

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
    setColor(shinAffected || (rs->hoverAnkle && !rs->draggingAnkle), 0.2f, 0.4f, 1.0f);
    drawCircle(ankleWorld, b.ankleRadius);

    // worked out in the shin's own local (pre-kneeAngle) frame, then
    // carried through the nested knee->hip->body transforms at the end,
    // same pattern drawThigh uses for its pre-hipAngle local frame.
    // shinArc1 (outer silhouette) uses side -1, shinArc2 (inner pinch)
    // uses side +1 -- same convention as thighArc1/thighArc2.
    Point shin1KneeLocal = circleHalfPoint(b.kneeCircle, b.kneeRadius, b.kneeCircle, b.ankleCircle, -1);
    Point shin1AnkleLocal = circleHalfPoint(b.ankleCircle, b.ankleRadius, b.kneeCircle, b.ankleCircle, -1);
    Point shin1BulgeLocal = axisBulgePoint(b.kneeCircle, b.ankleCircle, b.shinArc1Bulge);

    Point shin2KneeLocal = circleHalfPoint(b.kneeCircle, b.kneeRadius, b.kneeCircle, b.ankleCircle, +1);
    Point shin2AnkleLocal = circleHalfPoint(b.ankleCircle, b.ankleRadius, b.kneeCircle, b.ankleCircle, +1);
    Point shin2BulgeLocal = axisBulgePoint(b.kneeCircle, b.ankleCircle, b.shinArc2Bulge);

    // dragging the ankle circle also stretches/shrinks both shin arcs
    // (they attach to ankleCircle, which just moved along the knee->ankle
    // axis -- see WM_MOUSEMOVE's draggingAnkle/constrainToAxis handling),
    // same "carries the other visibly-reacting parts along" idea as
    // draggingKnee getting added to the thigh arcs in drawThigh
    setColor(rs->draggingShin1 || rs->draggingAnkle || shinAffected, 0.2f, 0.4f, 1.0f);
    drawFilletCurve(nestedJointToWorld(shin1KneeLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin1BulgeLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin1AnkleLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle));

    setColor(rs->draggingShin2 || rs->draggingAnkle || shinAffected, 0.2f, 0.4f, 1.0f);
    drawFilletCurve(nestedJointToWorld(shin2KneeLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin2BulgeLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle),
            nestedJointToWorld(shin2AnkleLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle));
}

static void drawShinHandles(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point ankleWorld = nestedJointToWorld(b.ankleCircle, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

    // shin arc handles: same "sit exactly at the bezier control point"
    // idea as the thigh handles above
    Point shin1MidLocal = axisBulgePoint(b.kneeCircle, b.ankleCircle, b.shinArc1Bulge);
    Point shin2MidLocal = axisBulgePoint(b.kneeCircle, b.ankleCircle, b.shinArc2Bulge);

    Point shin1World = nestedJointToWorld(shin1MidLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    Point shin2World = nestedJointToWorld(shin2MidLocal, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

    drawHandle(ankleWorld, rs->draggingAnkle || rs->hoverAnkle, ANKLE_HANDLE_RADIUS);
    drawHandle(shin1World, rs->draggingShin1, SHIN_HANDLE_RADIUS);
    drawHandle(shin2World, rs->draggingShin2, SHIN_HANDLE_RADIUS);
}

void drawSemniHandles(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point inner        = rotatePoint(b.innerCircle, center, angle);
    Point headHandle   = rotatePoint((Point){b.headX, b.y}, center, angle);
    Point buttHandle    = rotatePoint((Point){b.buttX, b.y}, center, angle);

    // seam attach handles: sit exactly at the curve's bezier control point
    // (axisBulgePoint) -- same "handle IS the draggable parameter, so draw
    // it right where the curve is being pulled toward" idea as the thigh/
    // shin handles
    Point headLocal = { b.headX, b.y };
    Point buttLocal = { b.buttX, b.y };

    Point seamArc1MidLocal = axisBulgePoint(headLocal, buttLocal, b.seamArc1Bulge);
    Point seamArc2MidLocal = axisBulgePoint(headLocal, buttLocal, b.seamArc2Bulge);

    Point seamArc1Handle = rotatePoint(seamArc1MidLocal, center, angle);
    Point seamArc2Handle = rotatePoint(seamArc2MidLocal, center, angle);

    drawHandle(seamArc1Handle, rs->draggingSeamArc1, ARC_HANDLE_RADIUS);
    drawHandle(seamArc2Handle, rs->draggingSeamArc2, ARC_HANDLE_RADIUS);

    // joint circle handles: highlight on hover too, not just while dragging
    drawHandle(inner,
               rs->draggingInner || rs->hoverHip,
               HIP_HANDLE_RADIUS);

    drawHandle(headHandle,
               rs->hoverHead,
               HEAD_BUTT_HANDLE_RADIUS);

    drawHandle(buttHandle,
               rs->hoverButt,
               HEAD_BUTT_HANDLE_RADIUS);
}

void drawSemni(Semni b, RenderState* rs, int includeHandles)
{
    drawSemniBody(b, rs);
    drawThigh(b, rs);
    drawShin(b, rs);

    // the draggable handles are editor UI, not part of the robot itself --
    // skip them when rendering a frame that's about to be exported
    if (includeHandles)
    {
        drawSemniHandles(b, rs);
        drawThighHandles(b, rs);
        drawShinHandles(b, rs);
    }
}

static void renderRobot(AppState* app, int includeHandles)
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

    // sampled live every frame (the render loop runs continuously -- see
    // main.c) rather than cached from WM_MOUSEMOVE's wParam, which would
    // only update while the mouse is actively moving -- pressing/releasing
    // Shift while the cursor sits still over the hip handle needs to
    // toggle the preview immediately, with no mouse movement required
    rs.shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    drawSemni(app->robotScene.robot, &rs, includeHandles);
}

void renderApp(AppState* app, HDC hdc)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Draw ground reference line at the bottom. The viewport's
    // half-width is 1.5 units at the base zoom level, so we position
    // the line near the bottom (-1.3) to look like a ground plane
    // that helps the user position the robot for laying down poses.
    drawDashedHorizontalLine(-1.1f, 1.5f);

    renderRobot(app, 1);

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

    renderRobot(app, 0);
}
