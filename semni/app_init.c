#include "app.h"

void initHomePosition(AppState* app)
{
	app->robotScene.robot.buttX = 0.600000f;
	app->robotScene.robot.headX = -0.600000f;
	app->robotScene.robot.y = -0.770000f;
	app->robotScene.robot.buttRadius = 0.288500f;
	app->robotScene.robot.headRadius = 0.081000f;
	app->robotScene.robot.seamArc1Bulge = 0.303113f;
	app->robotScene.robot.seamArc2Bulge = -0.303113f;
	app->robotScene.robot.innerCircle.x = 0.000000f;
	app->robotScene.robot.innerCircle.y = -0.770000f;
	app->robotScene.robot.innerRadius = 0.120000f;
	app->robotScene.robot.angle = 178.000000f;
	app->robotScene.robot.hipAngle = -836.000000f;
	app->robotScene.robot.kneeAngle = 36.000000f;
	app->robotScene.robot.kneeCircle.x = 0.000000f;
	app->robotScene.robot.kneeCircle.y = -1.291971f;
	app->robotScene.robot.kneeRadius = 0.080000f;
	app->robotScene.robot.thighArc1Bulge = -0.150410f;
	app->robotScene.robot.thighArc2Bulge = 0.063868f;
	app->robotScene.robot.ankleCircle.x = 0.000000f;
	app->robotScene.robot.ankleCircle.y = -1.590574f;
	app->robotScene.robot.ankleRadius = 0.050000f;
	app->robotScene.robot.shinArc1Bulge = -0.085042f;
	app->robotScene.robot.shinArc2Bulge = 0.042168f;
}

void initStandingPosition(AppState* app)
{
	app->robotScene.robot.buttX = 0.600000f;
	app->robotScene.robot.headX = -0.600000f;
	app->robotScene.robot.y = -0.420000f;
	app->robotScene.robot.buttRadius = 0.288500f;
	app->robotScene.robot.headRadius = 0.081000f;
	app->robotScene.robot.seamArc1Bulge = 0.303113f;
	app->robotScene.robot.seamArc2Bulge = -0.303113f;
	app->robotScene.robot.innerCircle.x = 0.000000f;
	app->robotScene.robot.innerCircle.y = -0.420000f;
	app->robotScene.robot.innerRadius = 0.120000f;
	app->robotScene.robot.angle = 88.000000f;
	app->robotScene.robot.hipAngle = -760.000000f;
	app->robotScene.robot.kneeAngle = -26.000000f;
	app->robotScene.robot.kneeCircle.x = 0.000000f;
	app->robotScene.robot.kneeCircle.y = -0.941971f;
	app->robotScene.robot.kneeRadius = 0.080000f;
	app->robotScene.robot.thighArc1Bulge = -0.150410f;
	app->robotScene.robot.thighArc2Bulge = 0.063868f;
	app->robotScene.robot.ankleCircle.x = 0.000000f;
	app->robotScene.robot.ankleCircle.y = -1.240574f;
	app->robotScene.robot.ankleRadius = 0.050000f;
	app->robotScene.robot.shinArc1Bulge = -0.085042f;
	app->robotScene.robot.shinArc2Bulge = 0.042168f;
}

void initAppState(AppState* app)
{
	initStandingPosition(app);
}
