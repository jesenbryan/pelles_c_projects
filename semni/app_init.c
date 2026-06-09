#include "app.h"

void initAppState(AppState* app)
{
    app->robotScene.robot.leftX = -0.6f;
    app->robotScene.robot.rightX = 0.6f;
    app->robotScene.robot.y = 0.0f;

    app->robotScene.robot.leftRadius = 0.2f;
    app->robotScene.robot.rightRadius = 0.2f;

    app->robotScene.robot.topCtrl.x = 0.0f;
    app->robotScene.robot.topCtrl.y = 0.4f;

    app->robotScene.robot.bottomCtrl.x = 0.0f;
    app->robotScene.robot.bottomCtrl.y = -0.4f;

    app->robotScene.robot.innerCircle.x = 0.0f;
    app->robotScene.robot.innerCircle.y = 0.0f;

    app->robotScene.robot.innerRadius = 0.12f;

    app->paintCount = 0;
    app->painting = 0;
}
