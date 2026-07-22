#include <stdio.h>
#include <math.h>

#include "robot.h"
#include "renderer.h"

PointF getCenter(Semni b)
{
    PointF c;
    c.x = (b.buttX + b.headX) * 0.5f;
    c.y = b.y;
    return c;
}

// Reflects an attach angle (circleEdge's cos/sin convention) the way a
// point ON A CIRCLE reflects under an x-mirror: circleEdge(c1, r1, a) =
// c1 + r1*(cos(a), sin(a)), so mirroring c1's own x (as mirrorHipLeg does
// for innerCircle/kneeCircle) and wanting the point on the circle to
// mirror too means r1*cos(a) has to negate while r1*sin(a) stays put --
// i.e. a' = 180 - a (cos(180-a) = -cos(a), sin(180-a) = sin(a)).
static float mirrorArcAngle(float angleDeg)
{
    float mirrored = 180.0f - angleDeg;

    while (mirrored > 180.0f) mirrored -= 360.0f;
    while (mirrored < -180.0f) mirrored += 360.0f;

    return mirrored;
}

// The leg's joints (innerCircle, kneeCircle, footCircle) are stored as
// raw local coordinates, in the frame BEFORE hipAngle/kneeAngle rotate
// them into place (see app.h's comments on those fields) -- so mirroring
// the leg is a plain x-reflection of each one, about the body's own
// center line (the buttX/headX midpoint, same reference getCenter uses).
//
// A reflection alone would only flip the joints' REST positions though --
// to also flip the VISIBLE (rotated) pose, hipAngle and kneeAngle, which
// each compose a rotation on top of those joints, have to negate too:
// reflecting a point and then rotating it by angle T lands in the same
// place as rotating it by -T and then reflecting (a standard fact about
// 2D isometries -- reflection∘rotate(T) == rotate(-T)∘reflection), so
// negating every rotation angle downstream of a reflected joint is what
// keeps the whole chain self-consistent as a true mirror image instead of
// just moving the joints while leaving them bent the original way.
//
// The four arc attach angles (thighArc1Angle/thighArc2Angle/
// shinArc1Angle/shinArc2Angle) aren't rotations being composed, though --
// they're points parameterized around their own circle (circleEdge), so
// they mirror via mirrorArcAngle (180 - angle) instead of negation; see
// its comment.
void mirrorHipLeg(Semni* b)
{
    float centerX = (b->buttX + b->headX) * 0.5f;

    b->innerCircle.x = 2.0f * centerX - b->innerCircle.x;
    b->kneeCircle.x  = 2.0f * centerX - b->kneeCircle.x;
    b->footCircle.x = 2.0f * centerX - b->footCircle.x;

    b->hipAngle  = -b->hipAngle;
    b->kneeAngle = -b->kneeAngle;

    b->thighArc1Angle = mirrorArcAngle(b->thighArc1Angle);
    b->thighArc2Angle = mirrorArcAngle(b->thighArc2Angle);
    b->shinArc1Angle  = mirrorArcAngle(b->shinArc1Angle);
    b->shinArc2Angle  = mirrorArcAngle(b->shinArc2Angle);
}

// All of headX/buttX/y, innerCircle, kneeCircle, and footCircle live in
// the same world-space frame (see getCenter/rotatePoint's use of them in
// renderer.c) -- shifting every one of them by the identical (dx, dy)
// shifts getCenter() by the same amount too, so every downstream
// rotatePoint/jointToWorld/nestedJointToWorld call (which all ultimately
// rotate around getCenter() or a joint nested under it) lands exactly
// (dx, dy) away from where it would have otherwise. Nothing else -- no
// angle, no radius -- needs to change for this to be a pure rigid move.
void translateRobot(Semni* b, float dx, float dy)
{
    b->headX += dx;
    b->buttX += dx;
    b->y += dy;

    b->innerCircle.x += dx;
    b->innerCircle.y += dy;

    b->kneeCircle.x += dx;
    b->kneeCircle.y += dy;

    b->footCircle.x += dx;
    b->footCircle.y += dy;
}

BOOL isPointInsideRobotBody(Semni b, float wx, float wy)
{
    CircleSegment segs[NUM_ROBOT_BODY_CIRCLES];
    computeSemniBodyCircles(b, segs);

    for (int i = 0; i < NUM_ROBOT_BODY_CIRCLES; i++)
    {
        float dx = wx - segs[i].center.x;
        float dy = wy - segs[i].center.y;
        if (sqrtf(dx * dx + dy * dy) <= segs[i].radius)
            return TRUE;
    }
    return FALSE;
}

void printRobotAsInit(Semni b)
{
    printf("app->robotScene.robot.buttX = %.6ff;\n", b.buttX);
    printf("app->robotScene.robot.headX = %.6ff;\n", b.headX);
    printf("app->robotScene.robot.y = %.6ff;\n\n", b.y);

    printf("app->robotScene.robot.buttRadius = %.6ff;\n", b.buttRadius);
    printf("app->robotScene.robot.headRadius = %.6ff;\n\n", b.headRadius);

    printf("app->robotScene.robot.seamArc1Angle = %.6ff;\n", b.seamArc1Angle);
    printf("app->robotScene.robot.seamArc2Angle = %.6ff;\n\n", b.seamArc2Angle);

    printf("app->robotScene.robot.innerCircle.x = %.6ff;\n", b.innerCircle.x);
    printf("app->robotScene.robot.innerCircle.y = %.6ff;\n\n", b.innerCircle.y);

    printf("app->robotScene.robot.innerRadius = %.6ff;\n\n", b.innerRadius);

    printf("app->robotScene.robot.angle = %.6ff;\n", b.angle);
    printf("app->robotScene.robot.hipAngle = %.6ff;\n", b.hipAngle);
    printf("app->robotScene.robot.kneeAngle = %.6ff;\n\n", b.kneeAngle);

    printf("app->robotScene.robot.kneeCircle.x = %.6ff;\n", b.kneeCircle.x);
    printf("app->robotScene.robot.kneeCircle.y = %.6ff;\n\n", b.kneeCircle.y);

    printf("app->robotScene.robot.kneeRadius = %.6ff;\n\n", b.kneeRadius);

    printf("app->robotScene.robot.thighArc1Angle = %.6ff;\n", b.thighArc1Angle);
    printf("app->robotScene.robot.thighArc2Angle = %.6ff;\n\n", b.thighArc2Angle);

    printf("app->robotScene.robot.footCircle.x = %.6ff;\n", b.footCircle.x);
    printf("app->robotScene.robot.footCircle.y = %.6ff;\n\n", b.footCircle.y);

    printf("app->robotScene.robot.footRadius = %.6ff;\n\n", b.footRadius);

    printf("app->robotScene.robot.shinArc1Angle = %.6ff;\n", b.shinArc1Angle);
    printf("app->robotScene.robot.shinArc2Angle = %.6ff;\n", b.shinArc2Angle);

    printf("----\n");
}

// ---- Rocky ----

PointF getRockyCenter(Rocky r)
{
    PointF c;
    c.x = r.bodyX;
    c.y = r.bodyY;
    return c;
}

void mirrorRockyLeg(Rocky* r)
{
    r->kneeAngle = -r->kneeAngle;

    r->shinArc1Angle = mirrorArcAngle(r->shinArc1Angle);
    r->shinArc2Angle = mirrorArcAngle(r->shinArc2Angle);
}

void printRockyAsInit(Rocky r)
{
    printf("app->robotScene.rocky.bodyX = %.6ff;\n", r.bodyX);
    printf("app->robotScene.rocky.bodyY = %.6ff;\n\n", r.bodyY);

    printf("app->robotScene.rocky.bodyHalfWidth = %.6ff;\n", r.bodyHalfWidth);
    printf("app->robotScene.rocky.bodyHalfHeight = %.6ff;\n\n", r.bodyHalfHeight);

    printf("app->robotScene.rocky.angle = %.6ff;\n", r.angle);
    printf("app->robotScene.rocky.kneeAngle = %.6ff;\n\n", r.kneeAngle);

    printf("app->robotScene.rocky.kneeCircle.x = %.6ff;\n", r.kneeCircle.x);
    printf("app->robotScene.rocky.kneeCircle.y = %.6ff;\n\n", r.kneeCircle.y);

    printf("app->robotScene.rocky.kneeRadius = %.6ff;\n\n", r.kneeRadius);

    printf("app->robotScene.rocky.footCircle.x = %.6ff;\n", r.footCircle.x);
    printf("app->robotScene.rocky.footCircle.y = %.6ff;\n\n", r.footCircle.y);

    printf("app->robotScene.rocky.footRadius = %.6ff;\n\n", r.footRadius);

    printf("app->robotScene.rocky.shinArc1Angle = %.6ff;\n", r.shinArc1Angle);
    printf("app->robotScene.rocky.shinArc2Angle = %.6ff;\n", r.shinArc2Angle);

    printf("----\n");
}

// ---- Stilo ----

PointF getStiloCenter(Stilo s)
{
    PointF c;
    c.x = (s.buttX + s.headX) * 0.5f;
    c.y = s.y;
    return c;
}

void mirrorStiloLeg(Stilo* s)
{
    float centerX = (s->buttX + s->headX) * 0.5f;

    s->innerCircle.x = 2.0f * centerX - s->innerCircle.x;
    s->footCircle.x = 2.0f * centerX - s->footCircle.x;

    s->hipAngle = -s->hipAngle;

    s->thighArc1Angle = mirrorArcAngle(s->thighArc1Angle);
    s->thighArc2Angle = mirrorArcAngle(s->thighArc2Angle);
}

void printStiloAsInit(Stilo s)
{
    printf("app->robotScene.stilo.buttX = %.6ff;\n", s.buttX);
    printf("app->robotScene.stilo.headX = %.6ff;\n", s.headX);
    printf("app->robotScene.stilo.y = %.6ff;\n\n", s.y);

    printf("app->robotScene.stilo.buttRadius = %.6ff;\n", s.buttRadius);
    printf("app->robotScene.stilo.headRadius = %.6ff;\n\n", s.headRadius);

    printf("app->robotScene.stilo.seamArc1Angle = %.6ff;\n", s.seamArc1Angle);
    printf("app->robotScene.stilo.seamArc2Angle = %.6ff;\n\n", s.seamArc2Angle);

    printf("app->robotScene.stilo.innerCircle.x = %.6ff;\n", s.innerCircle.x);
    printf("app->robotScene.stilo.innerCircle.y = %.6ff;\n\n", s.innerCircle.y);

    printf("app->robotScene.stilo.innerRadius = %.6ff;\n\n", s.innerRadius);

    printf("app->robotScene.stilo.angle = %.6ff;\n", s.angle);
    printf("app->robotScene.stilo.hipAngle = %.6ff;\n\n", s.hipAngle);

    printf("app->robotScene.stilo.footCircle.x = %.6ff;\n", s.footCircle.x);
    printf("app->robotScene.stilo.footCircle.y = %.6ff;\n\n", s.footCircle.y);

    printf("app->robotScene.stilo.footRadius = %.6ff;\n\n", s.footRadius);

    printf("app->robotScene.stilo.thighArc1Angle = %.6ff;\n", s.thighArc1Angle);
    printf("app->robotScene.stilo.thighArc2Angle = %.6ff;\n", s.thighArc2Angle);

    printf("----\n");
}
