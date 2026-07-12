#include "app.h"

void initAppState(AppState* app)
{
app->robotScene.robot.headX = -0.600000f;
app->robotScene.robot.buttX = 0.600000f;
app->robotScene.robot.y = 0.000000f;

app->robotScene.robot.headRadius = 0.081000f;
app->robotScene.robot.buttRadius = 0.288500f;

app->robotScene.robot.topArcAngle = 122.854446f;
app->robotScene.robot.bottomArcAngle = 237.145584f;

app->robotScene.robot.innerCircle.x = 0.000000f;
app->robotScene.robot.innerCircle.y = 0.000000f;

app->robotScene.robot.innerRadius = 0.120000f;

app->robotScene.robot.angle = 88.000000f;
app->robotScene.robot.hipAngle = 682.000000f;
app->robotScene.robot.kneeAngle = -38.000000f;

app->robotScene.robot.kneeCircle.x = 0.000000f;
app->robotScene.robot.kneeCircle.y = -0.521971f;

app->robotScene.robot.kneeRadius = 0.080000f;

app->robotScene.robot.thighArc1Angle = 18.086754f;
app->robotScene.robot.thighArc2Angle = -160.043976f;

app->robotScene.robot.ankleCircle.x = 0.000000f;
app->robotScene.robot.ankleCircle.y = -0.820574f;

app->robotScene.robot.ankleRadius = 0.050000f;

app->robotScene.robot.shinArc1Angle = 10.178429f;
app->robotScene.robot.shinArc2Angle = -157.189026f;
}
