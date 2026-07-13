#include "app.h"

void initAppState(AppState* app)
{
app->robotScene.robot.headX = -0.600000f;
app->robotScene.robot.buttX = 0.600000f;
app->robotScene.robot.y = 0.000000f;

app->robotScene.robot.headRadius = 0.081000f;
app->robotScene.robot.buttRadius = 0.288500f;

app->robotScene.robot.seamArc1Angle = 122.854446f;
app->robotScene.robot.seamArc2Angle = 237.145584f;

app->robotScene.robot.innerCircle.x = 0.000000f;
app->robotScene.robot.innerCircle.y = 0.000000f;

app->robotScene.robot.innerRadius = 0.120000f;

app->robotScene.robot.angle = 88.000000f;
app->robotScene.robot.hipAngle = -762.000000f;
app->robotScene.robot.kneeAngle = -26.000000f;

app->robotScene.robot.kneeCircle.x = 0.000000f;
app->robotScene.robot.kneeCircle.y = -0.521971f;

app->robotScene.robot.kneeRadius = 0.080000f;

app->robotScene.robot.thighArc1Angle = 161.913239f;
app->robotScene.robot.thighArc2Angle = -19.956024f;

app->robotScene.robot.ankleCircle.x = 0.000000f;
app->robotScene.robot.ankleCircle.y = -0.820574f;

app->robotScene.robot.ankleRadius = 0.050000f;

app->robotScene.robot.shinArc1Angle = 169.821564f;
app->robotScene.robot.shinArc2Angle = -22.810974f;
}
