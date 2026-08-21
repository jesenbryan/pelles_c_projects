#include <stdio.h>

#include "robot.h"

Point getCenter(Semni b)
{
    Point c;
    c.x = (b.buttX + b.headX) * 0.5f;
    c.y = b.y;
    return c;
}

// The leg's joints (innerCircle, kneeCircle, ankleCircle) are stored as
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
// The four arc bulge distances (thighArc1Bulge/thighArc2Bulge/
// shinArc1Bulge/shinArc2Bulge) aren't rotations being composed either, but
// they DO need to flip: each is a signed distance along a perpendicular
// direction (geometry.h's axisBulgePoint) that's itself derived fresh
// every frame from the hip->knee/knee->ankle axis, and an x-only
// reflection of that axis's two endpoints reverses which physical
// direction "positive perpendicular" points in (working through the
// same "reflection reverses orientation" fact the rotation-angle negation
// above relies on) -- so keeping the mirrored curve's shape a true mirror
// image (not a re-bent version of the original) just needs a plain
// negation, same as hipAngle/kneeAngle.
void mirrorHipLeg(Semni* b)
{
    float centerX = (b->buttX + b->headX) * 0.5f;

    b->innerCircle.x = 2.0f * centerX - b->innerCircle.x;
    b->kneeCircle.x  = 2.0f * centerX - b->kneeCircle.x;
    b->ankleCircle.x = 2.0f * centerX - b->ankleCircle.x;

    b->hipAngle  = -b->hipAngle;
    b->kneeAngle = -b->kneeAngle;

    b->thighArc1Bulge = -b->thighArc1Bulge;
    b->thighArc2Bulge = -b->thighArc2Bulge;
    b->shinArc1Bulge  = -b->shinArc1Bulge;
    b->shinArc2Bulge  = -b->shinArc2Bulge;
}

void printRobotAsInit(Semni b)
{
    printf("app->robotScene.robot.buttX = %.6ff;\n", b.buttX);
    printf("app->robotScene.robot.headX = %.6ff;\n", b.headX);
    printf("app->robotScene.robot.y = %.6ff;\n\n", b.y);

    printf("app->robotScene.robot.buttRadius = %.6ff;\n", b.buttRadius);
    printf("app->robotScene.robot.headRadius = %.6ff;\n\n", b.headRadius);

    printf("app->robotScene.robot.seamArc1Bulge = %.6ff;\n", b.seamArc1Bulge);
    printf("app->robotScene.robot.seamArc2Bulge = %.6ff;\n\n", b.seamArc2Bulge);

    printf("app->robotScene.robot.innerCircle.x = %.6ff;\n", b.innerCircle.x);
    printf("app->robotScene.robot.innerCircle.y = %.6ff;\n\n", b.innerCircle.y);

    printf("app->robotScene.robot.innerRadius = %.6ff;\n\n", b.innerRadius);

    printf("app->robotScene.robot.angle = %.6ff;\n", b.angle);
    printf("app->robotScene.robot.hipAngle = %.6ff;\n", b.hipAngle);
    printf("app->robotScene.robot.kneeAngle = %.6ff;\n\n", b.kneeAngle);

    printf("app->robotScene.robot.kneeCircle.x = %.6ff;\n", b.kneeCircle.x);
    printf("app->robotScene.robot.kneeCircle.y = %.6ff;\n\n", b.kneeCircle.y);

    printf("app->robotScene.robot.kneeRadius = %.6ff;\n\n", b.kneeRadius);

    printf("app->robotScene.robot.thighArc1Bulge = %.6ff;\n", b.thighArc1Bulge);
    printf("app->robotScene.robot.thighArc2Bulge = %.6ff;\n\n", b.thighArc2Bulge);

    printf("app->robotScene.robot.ankleCircle.x = %.6ff;\n", b.ankleCircle.x);
    printf("app->robotScene.robot.ankleCircle.y = %.6ff;\n\n", b.ankleCircle.y);

    printf("app->robotScene.robot.ankleRadius = %.6ff;\n\n", b.ankleRadius);

    printf("app->robotScene.robot.shinArc1Bulge = %.6ff;\n", b.shinArc1Bulge);
    printf("app->robotScene.robot.shinArc2Bulge = %.6ff;\n", b.shinArc2Bulge);

    printf("----\n");
}
