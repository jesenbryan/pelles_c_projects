#include <stdio.h>

#include "robot.h"

Point getCenter(Semni b)
{
    Point c;
    c.x = (b.headX + b.buttX) * 0.5f;
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

// The leg's joints (innerCircle, kneeCircle, ankleCircle) are stored as
// raw local coordinates, in the frame BEFORE hipAngle/kneeAngle rotate
// them into place (see app.h's comments on those fields) -- so mirroring
// the leg is a plain x-reflection of each one, about the body's own
// center line (the headX/buttX midpoint, same reference getCenter uses).
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
    float centerX = (b->headX + b->buttX) * 0.5f;

    b->innerCircle.x = 2.0f * centerX - b->innerCircle.x;
    b->kneeCircle.x  = 2.0f * centerX - b->kneeCircle.x;
    b->ankleCircle.x = 2.0f * centerX - b->ankleCircle.x;

    b->hipAngle  = -b->hipAngle;
    b->kneeAngle = -b->kneeAngle;

    b->thighArc1Angle = mirrorArcAngle(b->thighArc1Angle);
    b->thighArc2Angle = mirrorArcAngle(b->thighArc2Angle);
    b->shinArc1Angle  = mirrorArcAngle(b->shinArc1Angle);
    b->shinArc2Angle  = mirrorArcAngle(b->shinArc2Angle);
}

void printRobotAsInit(Semni b)
{
    printf("app->robotScene.robot.headX = %.6ff;\n", b.headX);
    printf("app->robotScene.robot.buttX = %.6ff;\n", b.buttX);
    printf("app->robotScene.robot.y = %.6ff;\n\n", b.y);

    printf("app->robotScene.robot.headRadius = %.6ff;\n", b.headRadius);
    printf("app->robotScene.robot.buttRadius = %.6ff;\n\n", b.buttRadius);

    printf("app->robotScene.robot.topArcAngle = %.6ff;\n", b.topArcAngle);
    printf("app->robotScene.robot.bottomArcAngle = %.6ff;\n\n", b.bottomArcAngle);

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

    printf("app->robotScene.robot.ankleCircle.x = %.6ff;\n", b.ankleCircle.x);
    printf("app->robotScene.robot.ankleCircle.y = %.6ff;\n\n", b.ankleCircle.y);

    printf("app->robotScene.robot.ankleRadius = %.6ff;\n\n", b.ankleRadius);

    printf("app->robotScene.robot.shinArc1Angle = %.6ff;\n", b.shinArc1Angle);
    printf("app->robotScene.robot.shinArc2Angle = %.6ff;\n", b.shinArc2Angle);

    printf("----\n");
}
