#include <stdio.h>

#include "robot.h"

Point getCenter(Semni b)
{
    Point c;
    c.x = (b.headX + b.buttX) * 0.5f;
    c.y = b.y;
    return c;
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

    printf("app->robotScene.robot.shinCtrl1.x = %.6ff;\n", b.shinCtrl1.x);
    printf("app->robotScene.robot.shinCtrl1.y = %.6ff;\n\n", b.shinCtrl1.y);

    printf("app->robotScene.robot.shinCtrl2.x = %.6ff;\n", b.shinCtrl2.x);
    printf("app->robotScene.robot.shinCtrl2.y = %.6ff;\n", b.shinCtrl2.y);

    printf("----\n");
}
