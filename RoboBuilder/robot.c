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

// Same rigid-translate idea as translateRobot above: bodyX/bodyY is
// Rocky's own local torso center (rotated around by angle at render/
// collision time, same convention as Semni's headX/buttX/y), and
// kneeCircle/footCircle are local points hanging off it -- shifting all
// three by (dx, dy) moves the whole robot without touching angle,
// kneeAngle, or any radius.
void translateRocky(Rocky* r, float dx, float dy)
{
    r->bodyX += dx;
    r->bodyY += dy;

    r->kneeCircle.x += dx;
    r->kneeCircle.y += dy;

    r->footCircle.x += dx;
    r->footCircle.y += dy;
}

// ---- Stilo ----

PointF getStiloCenter(Stilo s)
{
    PointF c;
    c.x = (s.buttX + s.headX) * 0.5f;
    c.y = s.y;
    return c;
}

// Mirrors Stilo's leg 1 (hip1 + feet1, plus its two thigh arcs) across the
// torso's own center line -- same "reflect the world position, then solve
// backward through hip1Angle/bodyAngle one stage at a time" construction as
// mirrorHipLeg above (see its comment for the full derivation), just a
// shorter chain: hip1 -> feet1 directly, no knee/shin/foot sub-stage.
// hip1Angle simply negates (reflecting a point and then rotating it by T
// lands in the same place as rotating by -T and then reflecting); feet1Circle
// and the two thigh1 arc attach angles have to be solved backward through
// bodyAngle then hip1Angle. Leg 2 and the shared torso are untouched.
void mirrorStiloLeg(Stilo* s)
{
    PointF center = getStiloCenter(*s);
    float  bodyAngle = s->angle;
    float  axisX = center.x;

    float oldHip1Angle = s->hip1Angle;
    float newHip1Angle = -oldHip1Angle;

    PointF oldHip1  = s->hip1Circle;
    PointF oldFeet1 = s->feet1Circle;

    // hip1: one stage, bodyAngle around center
    PointF hip1World  = rotatePoint(oldHip1, center, bodyAngle);
    PointF hip1Target = hip1World; hip1Target.x = 2.0f * axisX - hip1World.x;
    PointF newHip1     = inverseRotate(hip1Target, center, bodyAngle);

    // feet1: hip1Angle around hip1Circle, then bodyAngle around center
    PointF feet1World  = jointToWorld(oldFeet1, oldHip1, oldHip1Angle, center, bodyAngle);
    PointF feet1Target = feet1World; feet1Target.x = 2.0f * axisX - feet1World.x;
    PointF feet1AfterBody = inverseRotate(feet1Target, center, bodyAngle);
    PointF newFeet1        = inverseRotate(feet1AfterBody, newHip1, newHip1Angle);

    // thigh1 arc 1/2: same two stages as feet1, from hip1Circle's own circle
    PointF thigh1Arc1Local  = circleEdge(oldHip1, s->hip1Radius, s->thigh1Arc1Angle);
    PointF thigh1Arc1World  = jointToWorld(thigh1Arc1Local, oldHip1, oldHip1Angle, center, bodyAngle);
    PointF thigh1Arc1Target = thigh1Arc1World; thigh1Arc1Target.x = 2.0f * axisX - thigh1Arc1World.x;
    PointF thigh1Arc1AfterBody = inverseRotate(thigh1Arc1Target, center, bodyAngle);
    PointF newThigh1Arc1Local  = inverseRotate(thigh1Arc1AfterBody, newHip1, newHip1Angle);
    float newThigh1Arc1Angle = atan2f(newThigh1Arc1Local.y - newHip1.y, newThigh1Arc1Local.x - newHip1.x) * 180.0f / 3.1415926f;

    PointF thigh1Arc2Local  = circleEdge(oldHip1, s->hip1Radius, s->thigh1Arc2Angle);
    PointF thigh1Arc2World  = jointToWorld(thigh1Arc2Local, oldHip1, oldHip1Angle, center, bodyAngle);
    PointF thigh1Arc2Target = thigh1Arc2World; thigh1Arc2Target.x = 2.0f * axisX - thigh1Arc2World.x;
    PointF thigh1Arc2AfterBody = inverseRotate(thigh1Arc2Target, center, bodyAngle);
    PointF newThigh1Arc2Local  = inverseRotate(thigh1Arc2AfterBody, newHip1, newHip1Angle);
    float newThigh1Arc2Angle = atan2f(newThigh1Arc2Local.y - newHip1.y, newThigh1Arc2Local.x - newHip1.x) * 180.0f / 3.1415926f;

    s->hip1Circle  = newHip1;
    s->feet1Circle = newFeet1;

    s->hip1Angle = newHip1Angle;

    s->thigh1Arc1Angle = newThigh1Arc1Angle;
    s->thigh1Arc2Angle = newThigh1Arc2Angle;
}

// Same construction as mirrorStiloLeg above, for Stilo's SECOND leg (its
// own hip2/feet2/thigh2 fields, see app.h's Stilo comment) -- leaves leg 1
// and the shared torso untouched.
void mirrorStiloLeg2(Stilo* s)
{
    PointF center = getStiloCenter(*s);
    float  bodyAngle = s->angle;
    float  axisX = center.x;

    float oldHip2Angle = s->hip2Angle;
    float newHip2Angle = -oldHip2Angle;

    PointF oldHip2  = s->hip2Circle;
    PointF oldFeet2 = s->feet2Circle;

    // hip2: one stage, bodyAngle around center
    PointF hip2World  = rotatePoint(oldHip2, center, bodyAngle);
    PointF hip2Target = hip2World; hip2Target.x = 2.0f * axisX - hip2World.x;
    PointF newHip2     = inverseRotate(hip2Target, center, bodyAngle);

    // feet2: hip2Angle around hip2Circle, then bodyAngle around center
    PointF feet2World  = jointToWorld(oldFeet2, oldHip2, oldHip2Angle, center, bodyAngle);
    PointF feet2Target = feet2World; feet2Target.x = 2.0f * axisX - feet2World.x;
    PointF feet2AfterBody = inverseRotate(feet2Target, center, bodyAngle);
    PointF newFeet2        = inverseRotate(feet2AfterBody, newHip2, newHip2Angle);

    // thigh2 arc 1/2: same two stages as feet2, from hip2Circle's own circle
    PointF thigh2Arc1Local  = circleEdge(oldHip2, s->hip2Radius, s->thigh2Arc1Angle);
    PointF thigh2Arc1World  = jointToWorld(thigh2Arc1Local, oldHip2, oldHip2Angle, center, bodyAngle);
    PointF thigh2Arc1Target = thigh2Arc1World; thigh2Arc1Target.x = 2.0f * axisX - thigh2Arc1World.x;
    PointF thigh2Arc1AfterBody = inverseRotate(thigh2Arc1Target, center, bodyAngle);
    PointF newThigh2Arc1Local  = inverseRotate(thigh2Arc1AfterBody, newHip2, newHip2Angle);
    float newThigh2Arc1Angle = atan2f(newThigh2Arc1Local.y - newHip2.y, newThigh2Arc1Local.x - newHip2.x) * 180.0f / 3.1415926f;

    PointF thigh2Arc2Local  = circleEdge(oldHip2, s->hip2Radius, s->thigh2Arc2Angle);
    PointF thigh2Arc2World  = jointToWorld(thigh2Arc2Local, oldHip2, oldHip2Angle, center, bodyAngle);
    PointF thigh2Arc2Target = thigh2Arc2World; thigh2Arc2Target.x = 2.0f * axisX - thigh2Arc2World.x;
    PointF thigh2Arc2AfterBody = inverseRotate(thigh2Arc2Target, center, bodyAngle);
    PointF newThigh2Arc2Local  = inverseRotate(thigh2Arc2AfterBody, newHip2, newHip2Angle);
    float newThigh2Arc2Angle = atan2f(newThigh2Arc2Local.y - newHip2.y, newThigh2Arc2Local.x - newHip2.x) * 180.0f / 3.1415926f;

    s->hip2Circle  = newHip2;
    s->feet2Circle = newFeet2;

    s->hip2Angle = newHip2Angle;

    s->thigh2Arc1Angle = newThigh2Arc1Angle;
    s->thigh2Arc2Angle = newThigh2Arc2Angle;
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

    printf("app->robotScene.stilo.angle = %.6ff;\n\n", s.angle);

    // ---- leg 1 ----
    printf("app->robotScene.stilo.hip1Circle.x = %.6ff;\n", s.hip1Circle.x);
    printf("app->robotScene.stilo.hip1Circle.y = %.6ff;\n\n", s.hip1Circle.y);

    printf("app->robotScene.stilo.hip1Radius = %.6ff;\n\n", s.hip1Radius);

    printf("app->robotScene.stilo.hip1Angle = %.6ff;\n\n", s.hip1Angle);

    printf("app->robotScene.stilo.feet1Circle.x = %.6ff;\n", s.feet1Circle.x);
    printf("app->robotScene.stilo.feet1Circle.y = %.6ff;\n\n", s.feet1Circle.y);

    printf("app->robotScene.stilo.feet1Radius = %.6ff;\n\n", s.feet1Radius);

    printf("app->robotScene.stilo.thigh1Arc1Angle = %.6ff;\n", s.thigh1Arc1Angle);
    printf("app->robotScene.stilo.thigh1Arc2Angle = %.6ff;\n\n", s.thigh1Arc2Angle);

    // ---- leg 2 ----
    printf("app->robotScene.stilo.hip2Circle.x = %.6ff;\n", s.hip2Circle.x);
    printf("app->robotScene.stilo.hip2Circle.y = %.6ff;\n\n", s.hip2Circle.y);

    printf("app->robotScene.stilo.hip2Radius = %.6ff;\n\n", s.hip2Radius);

    printf("app->robotScene.stilo.hip2Angle = %.6ff;\n\n", s.hip2Angle);

    printf("app->robotScene.stilo.feet2Circle.x = %.6ff;\n", s.feet2Circle.x);
    printf("app->robotScene.stilo.feet2Circle.y = %.6ff;\n\n", s.feet2Circle.y);

    printf("app->robotScene.stilo.feet2Radius = %.6ff;\n\n", s.feet2Radius);

    printf("app->robotScene.stilo.thigh2Arc1Angle = %.6ff;\n", s.thigh2Arc1Angle);
    printf("app->robotScene.stilo.thigh2Arc2Angle = %.6ff;\n", s.thigh2Arc2Angle);

    printf("----\n");
}

// Same rigid-translate idea as translateRobot above. Stilo's torso is
// built exactly like Semni's (headX/buttX/y -- see app.h's Stilo comment),
// so the torso half is identical; hip1Circle/feet1Circle/hip2Circle/
// feet2Circle are the local points for both independent legs, and all
// four shift the same way innerCircle/kneeCircle/footCircle do for Semni.
void translateStilo(Stilo* s, float dx, float dy)
{
    s->headX += dx;
    s->buttX += dx;
    s->y += dy;

    s->hip1Circle.x += dx;
    s->hip1Circle.y += dy;
    s->feet1Circle.x += dx;
    s->feet1Circle.y += dy;

    s->hip2Circle.x += dx;
    s->hip2Circle.y += dy;
    s->feet2Circle.x += dx;
    s->feet2Circle.y += dy;
}
