#include <stdio.h>
#include <math.h>

#include "robot.h"
#include "renderer.h"

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
