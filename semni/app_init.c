#include "app.h"

void initAppState(AppState* app)
{
    app->robotScene.robot.headX = -0.600000f;
    app->robotScene.robot.buttX = 0.600000f;
    app->robotScene.robot.y = 0.000000f;

    app->robotScene.robot.headRadius = 0.101000f;
    app->robotScene.robot.buttRadius = 0.288500f;

    app->robotScene.robot.topCtrl.x = 0.000000f;
    app->robotScene.robot.topCtrl.y = 0.298469f;

    app->robotScene.robot.bottomCtrl.x = 0.000000f;
    app->robotScene.robot.bottomCtrl.y = -0.300992f;

    app->robotScene.robot.innerCircle.x = 0.000000f;
    app->robotScene.robot.innerCircle.y = 0.000000f;

    app->robotScene.robot.innerRadius = 0.120000f;

    app->robotScene.robot.angle = 56.000000f;
    app->robotScene.robot.hipAngle = 2.000000f;
    app->robotScene.robot.kneeAngle = -78.000000f;

    // knee joint sits below the hip (innerCircle); the two thigh handles
    // bulge outward on either side so the connecting arcs read as a thigh
    app->robotScene.robot.kneeCircle.x = 0.000000f;
    app->robotScene.robot.kneeCircle.y = -0.521971f;

    app->robotScene.robot.kneeRadius = 0.080000f;

    app->robotScene.robot.thighCtrl1.x = 0.137303f;
    app->robotScene.robot.thighCtrl1.y = -0.260985f;

    app->robotScene.robot.thighCtrl2.x = -0.079008f;
    app->robotScene.robot.thighCtrl2.y = -0.260985f;

    // ankle sits below the knee, and the shin tapers in a bit more than
    // the thigh did, continuing the leg
    app->robotScene.robot.ankleCircle.x = 0.000000f;
    app->robotScene.robot.ankleCircle.y = -0.820574f;

    app->robotScene.robot.ankleRadius = 0.050000f;

    app->robotScene.robot.shinCtrl1.x = 0.079633f;
    app->robotScene.robot.shinCtrl1.y = -0.671273f;

    app->robotScene.robot.shinCtrl2.x = -0.055620f;
    app->robotScene.robot.shinCtrl2.y = -0.671273f;
}
