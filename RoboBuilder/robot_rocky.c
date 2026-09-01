#include <stdio.h>
#include <math.h>

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

#include "robot.h"
#include "renderer.h"

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

