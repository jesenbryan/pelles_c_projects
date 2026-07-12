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

// Draws a circular arc that passes through p0, p1, and p2 (p1 is the
// "bulge" handle, same role it played as the bezier control point).
void drawArc(Point p0, Point p1, Point p2)
{
    Circle circle = circumcircle(p0, p1, p2);

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
    setColor(rs->draggingInner, 0.2f, 0.4f, 1.0f);
    drawCircle(inner, b.innerRadius);

    // top/bottom seams: each is a circular arc internally tangent to both
    // the head and butt circles, parameterized by the angle where it
    // attaches to the head circle (topArcAngle/bottomArcAngle) -- the
    // fillet's radius, center, and its other tangent point (on butt) all
    // fall out of a closed-form solve. Done in local (unrotated) space,
    // then rotated into world space at the end, same pattern used
    // elsewhere for local-frame points.
    Point headLocal = { b.headX, b.y };
    Point buttLocal = { b.buttX, b.y };
    Point bodyMidLocal = { (headLocal.x + buttLocal.x) * 0.5f, (headLocal.y + buttLocal.y) * 0.5f };

    Fillet topFillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.topArcAngle, MIN_ARC_R, MAX_ARC_R);
    Point topHeadTangentLocal = circleEdge(headLocal, b.headRadius, b.topArcAngle); // exact, by definition of the attach angle
    Point topButtTangentLocal = internalTangentPoint(topFillet.center, topFillet.radius, buttLocal, b.buttRadius);
    // hint point for drawArc: the point on the fillet circle nearest the
    // body's midline, so it sweeps the near/visible arc instead of the
    // far side of a possibly-huge circle
    Point topNearLocal = circleTowardPoint(topFillet.center, topFillet.radius, bodyMidLocal);

    Fillet bottomFillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.bottomArcAngle, MIN_ARC_R, MAX_ARC_R);
    Point bottomHeadTangentLocal = circleEdge(headLocal, b.headRadius, b.bottomArcAngle);
    Point bottomButtTangentLocal = internalTangentPoint(bottomFillet.center, bottomFillet.radius, buttLocal, b.buttRadius);
    Point bottomNearLocal = circleTowardPoint(bottomFillet.center, bottomFillet.radius, bodyMidLocal);

    // topHeadTangentLocal/topNearLocal/topButtTangentLocal all sit exactly
    // on the same known circle by construction, so feeding them through
    // the existing 3-point drawArc reuses its circumcircle + sweep-
    // direction logic for free -- the near-pole point just tells it which
    // way (short or long way around) to sweep
    setColor(rs->draggingTopArc, 0.2f, 0.4f, 1.0f);
    drawArc(rotatePoint(topHeadTangentLocal, center, angle),
            rotatePoint(topNearLocal, center, angle),
            rotatePoint(topButtTangentLocal, center, angle));

    setColor(rs->draggingBottomArc, 0.2f, 0.4f, 1.0f);
    drawArc(rotatePoint(bottomHeadTangentLocal, center, angle),
            rotatePoint(bottomNearLocal, center, angle),
            rotatePoint(bottomButtTangentLocal, center, angle));
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
    // hip, so hovering the hip handle highlights it too -- not just while
    // it's actually being dragged
    setColor(rs->draggingKnee || rs->hoverHip, 0.2f, 0.4f, 1.0f);
    drawCircle(kneeWorld, b.kneeRadius);

    // the two thigh arcs: same tangent-fillet construction as the
    // head/butt seams, just between innerCircle and kneeCircle -- worked
    // out in the leg's own local (pre-hipAngle) frame, then rotated into
    // world space at the end, same pattern drawSemniBody uses for the
    // head/butt seams (which work in pre-body-angle local space)
    Point axisMidLocal = { (b.innerCircle.x + b.kneeCircle.x) * 0.5f, (b.innerCircle.y + b.kneeCircle.y) * 0.5f };

    Fillet thigh1Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    Point thigh1InnerTangentLocal = circleEdge(b.innerCircle, b.innerRadius, b.thighArc1Angle);
    Point thigh1KneeTangentLocal = internalTangentPoint(thigh1Fillet.center, thigh1Fillet.radius, b.kneeCircle, b.kneeRadius);
    Point thigh1NearLocal = circleTowardPoint(thigh1Fillet.center, thigh1Fillet.radius, axisMidLocal);

    Fillet thigh2Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    Point thigh2InnerTangentLocal = circleEdge(b.innerCircle, b.innerRadius, b.thighArc2Angle);
    Point thigh2KneeTangentLocal = internalTangentPoint(thigh2Fillet.center, thigh2Fillet.radius, b.kneeCircle, b.kneeRadius);
    Point thigh2NearLocal = circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, axisMidLocal);

    setColor(rs->draggingThigh1 || rs->hoverHip, 0.2f, 0.4f, 1.0f);
    drawArc(jointToWorld(thigh1InnerTangentLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh1NearLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh1KneeTangentLocal, b.innerCircle, b.hipAngle, center, angle));

    setColor(rs->draggingThigh2 || rs->hoverHip, 0.2f, 0.4f, 1.0f);
    drawArc(jointToWorld(thigh2InnerTangentLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh2NearLocal, b.innerCircle, b.hipAngle, center, angle),
            jointToWorld(thigh2KneeTangentLocal, b.innerCircle, b.hipAngle, center, angle));
}

static void drawThighHandles(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point kneeWorld = jointToWorld(b.kneeCircle, b.innerCircle, b.hipAngle, center, angle);

    // thigh arc handles: pinned to the exact middle of the hip->knee axis
    // (circleAtAxisMid), same idea as the head/butt seam handles' use of
    // circleAtX -- except the axis here can point any direction, so the
    // generalized version is needed instead of pinning to a fixed X
    Point axisMidLocal = { (b.innerCircle.x + b.kneeCircle.x) * 0.5f, (b.innerCircle.y + b.kneeCircle.y) * 0.5f };

    Fillet thigh1Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    Point thigh1NearLocal = circleTowardPoint(thigh1Fillet.center, thigh1Fillet.radius, axisMidLocal);
    Point thigh1MidLocal = circleAtAxisMid(thigh1Fillet.center, thigh1Fillet.radius, b.innerCircle, b.kneeCircle, thigh1NearLocal);

    Fillet thigh2Fillet = filletFromAttachAngle(b.innerCircle, b.innerRadius, b.kneeCircle, b.kneeRadius, b.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
    Point thigh2NearLocal = circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, axisMidLocal);
    Point thigh2MidLocal = circleAtAxisMid(thigh2Fillet.center, thigh2Fillet.radius, b.innerCircle, b.kneeCircle, thigh2NearLocal);

    Point thigh1World = jointToWorld(thigh1MidLocal, b.innerCircle, b.hipAngle, center, angle);
    Point thigh2World = jointToWorld(thigh2MidLocal, b.innerCircle, b.hipAngle, center, angle);

    drawHandle(kneeWorld, rs->draggingKnee || rs->hoverKnee, KNEE_HANDLE_RADIUS);
    drawHandle(thigh1World, rs->draggingThigh1, THIGH_HANDLE_RADIUS);
    drawHandle(thigh2World, rs->draggingThigh2, THIGH_HANDLE_RADIUS);
}

// Continues the leg past the knee: draws the ankle joint and the two arcs
// connecting it back to kneeCircle, same pattern as drawThigh. The ankle
// and shin points first rotate around kneeCircle by kneeAngle (the knee's
// own rotation), then follow the hip/body transforms like the rest of the
// leg -- so scrolling on the knee handle swings the shin without touching
// the thigh, hip, or body.
static void drawShin(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point kneeWorld  = jointToWorld(b.kneeCircle, b.innerCircle, b.hipAngle, center, angle);
    Point ankleWorld = nestedJointToWorld(b.ankleCircle, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

    Point ctrl1World = nestedJointToWorld(b.shinCtrl1, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    Point ctrl2World = nestedJointToWorld(b.shinCtrl2, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

    // the shin (ankle circle + both connecting arcs) swings whenever
    // either the hip OR the knee rotates, so hovering either handle
    // highlights it -- but the thigh only cares about the hip (see above)
    int shinAffected = rs->hoverHip || rs->hoverKnee;

    setColor(rs->draggingAnkle || shinAffected, 0.2f, 0.4f, 1.0f);
    drawCircle(ankleWorld, b.ankleRadius);

    float limbAngle = atan2f(ankleWorld.y - kneeWorld.y,
                              ankleWorld.x - kneeWorld.x) * 180.0f / 3.1415926f;

    Point side1P0 = circleEdge(kneeWorld,  b.kneeRadius,  limbAngle + 90);
    Point side1P2 = circleEdge(ankleWorld, b.ankleRadius, limbAngle + 90);

    Point side2P0 = circleEdge(kneeWorld,  b.kneeRadius,  limbAngle - 90);
    Point side2P2 = circleEdge(ankleWorld, b.ankleRadius, limbAngle - 90);

    setColor(rs->draggingShin1 || shinAffected, 0.2f, 0.4f, 1.0f);
    drawArc(side1P0, ctrl1World, side1P2);

    setColor(rs->draggingShin2 || shinAffected, 0.2f, 0.4f, 1.0f);
    drawArc(side2P0, ctrl2World, side2P2);
}

static void drawShinHandles(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point ankleWorld  = nestedJointToWorld(b.ankleCircle, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    Point ctrl1World  = nestedJointToWorld(b.shinCtrl1, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);
    Point ctrl2World  = nestedJointToWorld(b.shinCtrl2, b.kneeCircle, b.kneeAngle, b.innerCircle, b.hipAngle, center, angle);

    drawHandle(ankleWorld, rs->draggingAnkle || rs->hoverAnkle, ANKLE_HANDLE_RADIUS);
    drawHandle(ctrl1World, rs->draggingShin1, SHIN_HANDLE_RADIUS);
    drawHandle(ctrl2World, rs->draggingShin2, SHIN_HANDLE_RADIUS);
}

void drawSemniHandles(Semni b, RenderState* rs)
{
    Point center = getCenter(b);
    float angle = b.angle;

    Point inner        = rotatePoint(b.innerCircle, center, angle);
    Point headHandle   = rotatePoint((Point){b.headX, b.y}, center, angle);
    Point buttHandle    = rotatePoint((Point){b.buttX, b.y}, center, angle);

    // seam attach handles: pinned to the exact midpoint between head and
    // butt on X, with Y solved from the arc's actual fillet circle at
    // that exact X (circleAtX) so the handle sits genuinely ON the
    // visible curve AND in the true middle -- filletBulgePoint alone only
    // gives the closest point to the midline, which is usually close but
    // not exactly at bodyMid.x
    Point headLocal = { b.headX, b.y };
    Point buttLocal = { b.buttX, b.y };
    Point bodyMidLocal = { (headLocal.x + buttLocal.x) * 0.5f, (headLocal.y + buttLocal.y) * 0.5f };

    Fillet topSeamFillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.topArcAngle, MIN_ARC_R, MAX_ARC_R);
    Fillet bottomSeamFillet = filletFromAttachAngle(headLocal, b.headRadius, buttLocal, b.buttRadius, b.bottomArcAngle, MIN_ARC_R, MAX_ARC_R);

    Point topNearLocal = circleTowardPoint(topSeamFillet.center, topSeamFillet.radius, bodyMidLocal);
    Point bottomNearLocal = circleTowardPoint(bottomSeamFillet.center, bottomSeamFillet.radius, bodyMidLocal);

    Point topMidLocal = circleAtX(topSeamFillet.center, topSeamFillet.radius, bodyMidLocal.x, topNearLocal);
    Point bottomMidLocal = circleAtX(bottomSeamFillet.center, bottomSeamFillet.radius, bodyMidLocal.x, bottomNearLocal);

    Point topHandle = rotatePoint(topMidLocal, center, angle);
    Point bottomHandle = rotatePoint(bottomMidLocal, center, angle);

    drawHandle(topHandle, rs->draggingTopArc, ARC_HANDLE_RADIUS);
    drawHandle(bottomHandle, rs->draggingBottomArc, ARC_HANDLE_RADIUS);

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
    rs.draggingTopArc = app->draggingTopArc;
    rs.draggingBottomArc = app->draggingBottomArc;
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

    drawSemni(app->robotScene.robot, &rs, includeHandles);
}

void renderApp(AppState* app, HDC hdc)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

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
