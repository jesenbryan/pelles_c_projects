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

// Mirroring is supposed to reflect the leg about a TRUE vertical line on
// screen, through the body's own centerline (buttX/headX midpoint, same
// reference getCenter uses) -- but Semni's whole body is very often
// itself rotated (b.angle; e.g. the saved Standing/Home poses use 88/178
// degrees, not 0), and mirroring is only supposed to flip the LEG, not
// spin the body too, so b.angle stays exactly as it is. A plain per-field
// x-reflection of innerCircle/kneeCircle/footCircle (the old approach)
// only produces a true mirror image WITHIN the frame before that shared
// b.angle is applied -- composing it with an unchanged, nonzero b.angle
// on top reflects around a version of the centerline TILTED by b.angle,
// not the actual vertical line the user sees on screen, which is exactly
// why the leg used to come out looking wrong and needed a manual hip
// rotation afterward to compensate.
//
// hipAngle and kneeAngle still simply negate (reflecting a point and then
// rotating it by angle T lands in the same place as rotating it by -T and
// then reflecting -- reflection∘rotate(T) == rotate(-T)∘reflection), but
// every stored local point (innerCircle, kneeCircle, footCircle, and the
// four arc attach angles) has to be solved backward through the FULL
// chain of stages down to the screen -- world position, reflect about
// axisX, then inverseRotate back through bodyAngle/hipAngle/kneeAngle one
// stage at a time (in reverse order) -- instead of just negating its x,
// so the forward render reproduces a true screen-space mirror image
// regardless of what b.angle happens to be. Each arc angle is recovered
// via atan2f on its own mirrored local point, same idea as
// mirrorArcAngle's old 180-angle trick but general enough to survive an
// arbitrary bodyAngle (that trick assumed no outer rotation at all).
void mirrorHipLeg(Semni* b)
{
    PointF center = getCenter(*b);
    float  bodyAngle = b->angle;
    float  axisX = center.x;

    float oldHipAngle  = b->hipAngle;
    float oldKneeAngle = b->kneeAngle;
    float newHipAngle  = -oldHipAngle;
    float newKneeAngle = -oldKneeAngle;

    PointF oldInner = b->innerCircle;
    PointF oldKnee  = b->kneeCircle;
    PointF oldFoot  = b->footCircle;

    // hip: one stage, bodyAngle around center
    PointF hipWorld  = rotatePoint(oldInner, center, bodyAngle);
    PointF hipTarget = hipWorld; hipTarget.x = 2.0f * axisX - hipWorld.x;
    PointF newInner  = inverseRotate(hipTarget, center, bodyAngle);

    // knee: hipAngle around innerCircle, then bodyAngle around center
    PointF kneeWorld  = jointToWorld(oldKnee, oldInner, oldHipAngle, center, bodyAngle);
    PointF kneeTarget = kneeWorld; kneeTarget.x = 2.0f * axisX - kneeWorld.x;
    PointF kneeAfterBody = inverseRotate(kneeTarget, center, bodyAngle);
    PointF newKnee        = inverseRotate(kneeAfterBody, newInner, newHipAngle);

    // foot: kneeAngle around kneeCircle, hipAngle around innerCircle, bodyAngle around center
    PointF footWorld  = nestedJointToWorld(oldFoot, oldKnee, oldKneeAngle, oldInner, oldHipAngle, center, bodyAngle);
    PointF footTarget = footWorld; footTarget.x = 2.0f * axisX - footWorld.x;
    PointF footAfterBody = inverseRotate(footTarget, center, bodyAngle);
    PointF footAfterHip  = inverseRotate(footAfterBody, newInner, newHipAngle);
    PointF newFoot        = inverseRotate(footAfterHip, newKnee, newKneeAngle);

    // thigh arc 1/2: same two stages as the knee, from innerCircle's own circle
    PointF thigh1Local  = circleEdge(oldInner, b->innerRadius, b->thighArc1Angle);
    PointF thigh1World  = jointToWorld(thigh1Local, oldInner, oldHipAngle, center, bodyAngle);
    PointF thigh1Target = thigh1World; thigh1Target.x = 2.0f * axisX - thigh1World.x;
    PointF thigh1AfterBody = inverseRotate(thigh1Target, center, bodyAngle);
    PointF newThigh1Local  = inverseRotate(thigh1AfterBody, newInner, newHipAngle);
    float newThighArc1Angle = atan2f(newThigh1Local.y - newInner.y, newThigh1Local.x - newInner.x) * 180.0f / 3.1415926f;

    PointF thigh2Local  = circleEdge(oldInner, b->innerRadius, b->thighArc2Angle);
    PointF thigh2World  = jointToWorld(thigh2Local, oldInner, oldHipAngle, center, bodyAngle);
    PointF thigh2Target = thigh2World; thigh2Target.x = 2.0f * axisX - thigh2World.x;
    PointF thigh2AfterBody = inverseRotate(thigh2Target, center, bodyAngle);
    PointF newThigh2Local  = inverseRotate(thigh2AfterBody, newInner, newHipAngle);
    float newThighArc2Angle = atan2f(newThigh2Local.y - newInner.y, newThigh2Local.x - newInner.x) * 180.0f / 3.1415926f;

    // shin arc 1/2: same three stages as the foot, from kneeCircle's own circle
    PointF shin1Local  = circleEdge(oldKnee, b->kneeRadius, b->shinArc1Angle);
    PointF shin1World  = nestedJointToWorld(shin1Local, oldKnee, oldKneeAngle, oldInner, oldHipAngle, center, bodyAngle);
    PointF shin1Target = shin1World; shin1Target.x = 2.0f * axisX - shin1World.x;
    PointF shin1AfterBody = inverseRotate(shin1Target, center, bodyAngle);
    PointF shin1AfterHip  = inverseRotate(shin1AfterBody, newInner, newHipAngle);
    PointF newShin1Local  = inverseRotate(shin1AfterHip, newKnee, newKneeAngle);
    float newShinArc1Angle = atan2f(newShin1Local.y - newKnee.y, newShin1Local.x - newKnee.x) * 180.0f / 3.1415926f;

    PointF shin2Local  = circleEdge(oldKnee, b->kneeRadius, b->shinArc2Angle);
    PointF shin2World  = nestedJointToWorld(shin2Local, oldKnee, oldKneeAngle, oldInner, oldHipAngle, center, bodyAngle);
    PointF shin2Target = shin2World; shin2Target.x = 2.0f * axisX - shin2World.x;
    PointF shin2AfterBody = inverseRotate(shin2Target, center, bodyAngle);
    PointF shin2AfterHip  = inverseRotate(shin2AfterBody, newInner, newHipAngle);
    PointF newShin2Local  = inverseRotate(shin2AfterHip, newKnee, newKneeAngle);
    float newShinArc2Angle = atan2f(newShin2Local.y - newKnee.y, newShin2Local.x - newKnee.x) * 180.0f / 3.1415926f;

    b->innerCircle = newInner;
    b->kneeCircle  = newKnee;
    b->footCircle  = newFoot;

    b->hipAngle  = newHipAngle;
    b->kneeAngle = newKneeAngle;

    b->thighArc1Angle = newThighArc1Angle;
    b->thighArc2Angle = newThighArc2Angle;
    b->shinArc1Angle  = newShinArc1Angle;
    b->shinArc2Angle  = newShinArc2Angle;
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

// Same chain-aware, bodyAngle-correct construction as mirrorHipLeg above --
// see its comment for the full derivation. Stilo now has the identical
// hip->knee->foot chain (innerCircle -> kneeCircle -> footCircle, plus
// thighArc1/2 between hip/knee and shinArc1/2 between knee/foot), so this
// is that same math verbatim, just reading/writing Stilo's own fields.
void mirrorStiloLeg(Stilo* s)
{
    PointF center = getStiloCenter(*s);
    float  bodyAngle = s->angle;
    float  axisX = center.x;

    float oldHipAngle  = s->hipAngle;
    float oldKneeAngle = s->kneeAngle;
    float newHipAngle  = -oldHipAngle;
    float newKneeAngle = -oldKneeAngle;

    PointF oldInner = s->innerCircle;
    PointF oldKnee  = s->kneeCircle;
    PointF oldFoot  = s->footCircle;

    // hip: one stage, bodyAngle around center
    PointF hipWorld  = rotatePoint(oldInner, center, bodyAngle);
    PointF hipTarget = hipWorld; hipTarget.x = 2.0f * axisX - hipWorld.x;
    PointF newInner  = inverseRotate(hipTarget, center, bodyAngle);

    // knee: hipAngle around innerCircle, then bodyAngle around center
    PointF kneeWorld  = jointToWorld(oldKnee, oldInner, oldHipAngle, center, bodyAngle);
    PointF kneeTarget = kneeWorld; kneeTarget.x = 2.0f * axisX - kneeWorld.x;
    PointF kneeAfterBody = inverseRotate(kneeTarget, center, bodyAngle);
    PointF newKnee        = inverseRotate(kneeAfterBody, newInner, newHipAngle);

    // foot: kneeAngle around kneeCircle, hipAngle around innerCircle, bodyAngle around center
    PointF footWorld  = nestedJointToWorld(oldFoot, oldKnee, oldKneeAngle, oldInner, oldHipAngle, center, bodyAngle);
    PointF footTarget = footWorld; footTarget.x = 2.0f * axisX - footWorld.x;
    PointF footAfterBody = inverseRotate(footTarget, center, bodyAngle);
    PointF footAfterHip  = inverseRotate(footAfterBody, newInner, newHipAngle);
    PointF newFoot        = inverseRotate(footAfterHip, newKnee, newKneeAngle);

    // thigh arc 1/2: same two stages as the knee, from innerCircle's own circle
    PointF thigh1Local  = circleEdge(oldInner, s->innerRadius, s->thighArc1Angle);
    PointF thigh1World  = jointToWorld(thigh1Local, oldInner, oldHipAngle, center, bodyAngle);
    PointF thigh1Target = thigh1World; thigh1Target.x = 2.0f * axisX - thigh1World.x;
    PointF thigh1AfterBody = inverseRotate(thigh1Target, center, bodyAngle);
    PointF newThigh1Local  = inverseRotate(thigh1AfterBody, newInner, newHipAngle);
    float newThighArc1Angle = atan2f(newThigh1Local.y - newInner.y, newThigh1Local.x - newInner.x) * 180.0f / 3.1415926f;

    PointF thigh2Local  = circleEdge(oldInner, s->innerRadius, s->thighArc2Angle);
    PointF thigh2World  = jointToWorld(thigh2Local, oldInner, oldHipAngle, center, bodyAngle);
    PointF thigh2Target = thigh2World; thigh2Target.x = 2.0f * axisX - thigh2World.x;
    PointF thigh2AfterBody = inverseRotate(thigh2Target, center, bodyAngle);
    PointF newThigh2Local  = inverseRotate(thigh2AfterBody, newInner, newHipAngle);
    float newThighArc2Angle = atan2f(newThigh2Local.y - newInner.y, newThigh2Local.x - newInner.x) * 180.0f / 3.1415926f;

    // shin arc 1/2: same three stages as the foot, from kneeCircle's own circle
    PointF shin1Local  = circleEdge(oldKnee, s->kneeRadius, s->shinArc1Angle);
    PointF shin1World  = nestedJointToWorld(shin1Local, oldKnee, oldKneeAngle, oldInner, oldHipAngle, center, bodyAngle);
    PointF shin1Target = shin1World; shin1Target.x = 2.0f * axisX - shin1World.x;
    PointF shin1AfterBody = inverseRotate(shin1Target, center, bodyAngle);
    PointF shin1AfterHip  = inverseRotate(shin1AfterBody, newInner, newHipAngle);
    PointF newShin1Local  = inverseRotate(shin1AfterHip, newKnee, newKneeAngle);
    float newShinArc1Angle = atan2f(newShin1Local.y - newKnee.y, newShin1Local.x - newKnee.x) * 180.0f / 3.1415926f;

    PointF shin2Local  = circleEdge(oldKnee, s->kneeRadius, s->shinArc2Angle);
    PointF shin2World  = nestedJointToWorld(shin2Local, oldKnee, oldKneeAngle, oldInner, oldHipAngle, center, bodyAngle);
    PointF shin2Target = shin2World; shin2Target.x = 2.0f * axisX - shin2World.x;
    PointF shin2AfterBody = inverseRotate(shin2Target, center, bodyAngle);
    PointF shin2AfterHip  = inverseRotate(shin2AfterBody, newInner, newHipAngle);
    PointF newShin2Local  = inverseRotate(shin2AfterHip, newKnee, newKneeAngle);
    float newShinArc2Angle = atan2f(newShin2Local.y - newKnee.y, newShin2Local.x - newKnee.x) * 180.0f / 3.1415926f;

    s->innerCircle = newInner;
    s->kneeCircle  = newKnee;
    s->footCircle  = newFoot;

    s->hipAngle  = newHipAngle;
    s->kneeAngle = newKneeAngle;

    s->thighArc1Angle = newThighArc1Angle;
    s->thighArc2Angle = newThighArc2Angle;
    s->shinArc1Angle  = newShinArc1Angle;
    s->shinArc2Angle  = newShinArc2Angle;

    // ---- leg 2 -- same chain-aware construction, same shared center/
    // bodyAngle/axisX above (one torso, two legs), just reading/writing
    // Stilo's own Leg2 fields (see app.h's Stilo comment) instead of its
    // first leg's.
    float oldHipAngleLeg2  = s->hipAngleLeg2;
    float oldKneeAngleLeg2 = s->kneeAngleLeg2;
    float newHipAngleLeg2  = -oldHipAngleLeg2;
    float newKneeAngleLeg2 = -oldKneeAngleLeg2;

    PointF oldInnerLeg2 = s->innerCircleLeg2;
    PointF oldKneeLeg2  = s->kneeCircleLeg2;
    PointF oldFootLeg2  = s->footCircleLeg2;

    // hip: one stage, bodyAngle around center
    PointF hipWorldLeg2  = rotatePoint(oldInnerLeg2, center, bodyAngle);
    PointF hipTargetLeg2 = hipWorldLeg2; hipTargetLeg2.x = 2.0f * axisX - hipWorldLeg2.x;
    PointF newInnerLeg2  = inverseRotate(hipTargetLeg2, center, bodyAngle);

    // knee: hipAngle around innerCircle, then bodyAngle around center
    PointF kneeWorldLeg2  = jointToWorld(oldKneeLeg2, oldInnerLeg2, oldHipAngleLeg2, center, bodyAngle);
    PointF kneeTargetLeg2 = kneeWorldLeg2; kneeTargetLeg2.x = 2.0f * axisX - kneeWorldLeg2.x;
    PointF kneeAfterBodyLeg2 = inverseRotate(kneeTargetLeg2, center, bodyAngle);
    PointF newKneeLeg2        = inverseRotate(kneeAfterBodyLeg2, newInnerLeg2, newHipAngleLeg2);

    // foot: kneeAngle around kneeCircle, hipAngle around innerCircle, bodyAngle around center
    PointF footWorldLeg2  = nestedJointToWorld(oldFootLeg2, oldKneeLeg2, oldKneeAngleLeg2, oldInnerLeg2, oldHipAngleLeg2, center, bodyAngle);
    PointF footTargetLeg2 = footWorldLeg2; footTargetLeg2.x = 2.0f * axisX - footWorldLeg2.x;
    PointF footAfterBodyLeg2 = inverseRotate(footTargetLeg2, center, bodyAngle);
    PointF footAfterHipLeg2  = inverseRotate(footAfterBodyLeg2, newInnerLeg2, newHipAngleLeg2);
    PointF newFootLeg2        = inverseRotate(footAfterHipLeg2, newKneeLeg2, newKneeAngleLeg2);

    // thigh arc 1/2: same two stages as the knee, from innerCircleLeg2's own circle
    PointF thigh1LocalLeg2  = circleEdge(oldInnerLeg2, s->innerRadiusLeg2, s->thighArc1AngleLeg2);
    PointF thigh1WorldLeg2  = jointToWorld(thigh1LocalLeg2, oldInnerLeg2, oldHipAngleLeg2, center, bodyAngle);
    PointF thigh1TargetLeg2 = thigh1WorldLeg2; thigh1TargetLeg2.x = 2.0f * axisX - thigh1WorldLeg2.x;
    PointF thigh1AfterBodyLeg2 = inverseRotate(thigh1TargetLeg2, center, bodyAngle);
    PointF newThigh1LocalLeg2  = inverseRotate(thigh1AfterBodyLeg2, newInnerLeg2, newHipAngleLeg2);
    float newThighArc1AngleLeg2 = atan2f(newThigh1LocalLeg2.y - newInnerLeg2.y, newThigh1LocalLeg2.x - newInnerLeg2.x) * 180.0f / 3.1415926f;

    PointF thigh2LocalLeg2  = circleEdge(oldInnerLeg2, s->innerRadiusLeg2, s->thighArc2AngleLeg2);
    PointF thigh2WorldLeg2  = jointToWorld(thigh2LocalLeg2, oldInnerLeg2, oldHipAngleLeg2, center, bodyAngle);
    PointF thigh2TargetLeg2 = thigh2WorldLeg2; thigh2TargetLeg2.x = 2.0f * axisX - thigh2WorldLeg2.x;
    PointF thigh2AfterBodyLeg2 = inverseRotate(thigh2TargetLeg2, center, bodyAngle);
    PointF newThigh2LocalLeg2  = inverseRotate(thigh2AfterBodyLeg2, newInnerLeg2, newHipAngleLeg2);
    float newThighArc2AngleLeg2 = atan2f(newThigh2LocalLeg2.y - newInnerLeg2.y, newThigh2LocalLeg2.x - newInnerLeg2.x) * 180.0f / 3.1415926f;

    // shin arc 1/2: same three stages as the foot, from kneeCircleLeg2's own circle
    PointF shin1LocalLeg2  = circleEdge(oldKneeLeg2, s->kneeRadiusLeg2, s->shinArc1AngleLeg2);
    PointF shin1WorldLeg2  = nestedJointToWorld(shin1LocalLeg2, oldKneeLeg2, oldKneeAngleLeg2, oldInnerLeg2, oldHipAngleLeg2, center, bodyAngle);
    PointF shin1TargetLeg2 = shin1WorldLeg2; shin1TargetLeg2.x = 2.0f * axisX - shin1WorldLeg2.x;
    PointF shin1AfterBodyLeg2 = inverseRotate(shin1TargetLeg2, center, bodyAngle);
    PointF shin1AfterHipLeg2  = inverseRotate(shin1AfterBodyLeg2, newInnerLeg2, newHipAngleLeg2);
    PointF newShin1LocalLeg2  = inverseRotate(shin1AfterHipLeg2, newKneeLeg2, newKneeAngleLeg2);
    float newShinArc1AngleLeg2 = atan2f(newShin1LocalLeg2.y - newKneeLeg2.y, newShin1LocalLeg2.x - newKneeLeg2.x) * 180.0f / 3.1415926f;

    PointF shin2LocalLeg2  = circleEdge(oldKneeLeg2, s->kneeRadiusLeg2, s->shinArc2AngleLeg2);
    PointF shin2WorldLeg2  = nestedJointToWorld(shin2LocalLeg2, oldKneeLeg2, oldKneeAngleLeg2, oldInnerLeg2, oldHipAngleLeg2, center, bodyAngle);
    PointF shin2TargetLeg2 = shin2WorldLeg2; shin2TargetLeg2.x = 2.0f * axisX - shin2WorldLeg2.x;
    PointF shin2AfterBodyLeg2 = inverseRotate(shin2TargetLeg2, center, bodyAngle);
    PointF shin2AfterHipLeg2  = inverseRotate(shin2AfterBodyLeg2, newInnerLeg2, newHipAngleLeg2);
    PointF newShin2LocalLeg2  = inverseRotate(shin2AfterHipLeg2, newKneeLeg2, newKneeAngleLeg2);
    float newShinArc2AngleLeg2 = atan2f(newShin2LocalLeg2.y - newKneeLeg2.y, newShin2LocalLeg2.x - newKneeLeg2.x) * 180.0f / 3.1415926f;

    s->innerCircleLeg2 = newInnerLeg2;
    s->kneeCircleLeg2  = newKneeLeg2;
    s->footCircleLeg2  = newFootLeg2;

    s->hipAngleLeg2  = newHipAngleLeg2;
    s->kneeAngleLeg2 = newKneeAngleLeg2;

    s->thighArc1AngleLeg2 = newThighArc1AngleLeg2;
    s->thighArc2AngleLeg2 = newThighArc2AngleLeg2;
    s->shinArc1AngleLeg2  = newShinArc1AngleLeg2;
    s->shinArc2AngleLeg2  = newShinArc2AngleLeg2;
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
    printf("app->robotScene.stilo.hipAngle = %.6ff;\n", s.hipAngle);
    printf("app->robotScene.stilo.kneeAngle = %.6ff;\n\n", s.kneeAngle);

    printf("app->robotScene.stilo.kneeCircle.x = %.6ff;\n", s.kneeCircle.x);
    printf("app->robotScene.stilo.kneeCircle.y = %.6ff;\n\n", s.kneeCircle.y);

    printf("app->robotScene.stilo.kneeRadius = %.6ff;\n\n", s.kneeRadius);

    printf("app->robotScene.stilo.thighArc1Angle = %.6ff;\n", s.thighArc1Angle);
    printf("app->robotScene.stilo.thighArc2Angle = %.6ff;\n\n", s.thighArc2Angle);

    printf("app->robotScene.stilo.footCircle.x = %.6ff;\n", s.footCircle.x);
    printf("app->robotScene.stilo.footCircle.y = %.6ff;\n\n", s.footCircle.y);

    printf("app->robotScene.stilo.footRadius = %.6ff;\n\n", s.footRadius);

    printf("app->robotScene.stilo.shinArc1Angle = %.6ff;\n", s.shinArc1Angle);
    printf("app->robotScene.stilo.shinArc2Angle = %.6ff;\n\n", s.shinArc2Angle);

    // ---- leg 2 ----
    printf("app->robotScene.stilo.innerCircleLeg2.x = %.6ff;\n", s.innerCircleLeg2.x);
    printf("app->robotScene.stilo.innerCircleLeg2.y = %.6ff;\n\n", s.innerCircleLeg2.y);

    printf("app->robotScene.stilo.innerRadiusLeg2 = %.6ff;\n\n", s.innerRadiusLeg2);

    printf("app->robotScene.stilo.hipAngleLeg2 = %.6ff;\n", s.hipAngleLeg2);
    printf("app->robotScene.stilo.kneeAngleLeg2 = %.6ff;\n\n", s.kneeAngleLeg2);

    printf("app->robotScene.stilo.kneeCircleLeg2.x = %.6ff;\n", s.kneeCircleLeg2.x);
    printf("app->robotScene.stilo.kneeCircleLeg2.y = %.6ff;\n\n", s.kneeCircleLeg2.y);

    printf("app->robotScene.stilo.kneeRadiusLeg2 = %.6ff;\n\n", s.kneeRadiusLeg2);

    printf("app->robotScene.stilo.thighArc1AngleLeg2 = %.6ff;\n", s.thighArc1AngleLeg2);
    printf("app->robotScene.stilo.thighArc2AngleLeg2 = %.6ff;\n\n", s.thighArc2AngleLeg2);

    printf("app->robotScene.stilo.footCircleLeg2.x = %.6ff;\n", s.footCircleLeg2.x);
    printf("app->robotScene.stilo.footCircleLeg2.y = %.6ff;\n\n", s.footCircleLeg2.y);

    printf("app->robotScene.stilo.footRadiusLeg2 = %.6ff;\n\n", s.footRadiusLeg2);

    printf("app->robotScene.stilo.shinArc1AngleLeg2 = %.6ff;\n", s.shinArc1AngleLeg2);
    printf("app->robotScene.stilo.shinArc2AngleLeg2 = %.6ff;\n", s.shinArc2AngleLeg2);

    printf("----\n");
}
