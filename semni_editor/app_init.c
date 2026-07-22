#include "app.h"

void initHomePosition(AppState* app)
{
	app->robotScene.robot.buttX = 0.600000f;
	app->robotScene.robot.headX = -0.600000f;
	app->robotScene.robot.y = -0.770000f;
	app->robotScene.robot.buttRadius = 0.288500f;
	app->robotScene.robot.headRadius = 0.081000f;
	app->robotScene.robot.seamArc1Angle = 122.854446f;
	app->robotScene.robot.seamArc2Angle = 237.145584f;
	app->robotScene.robot.innerCircle.x = 0.000000f;
	app->robotScene.robot.innerCircle.y = -0.770000f;
	app->robotScene.robot.innerRadius = 0.120000f;
	app->robotScene.robot.angle = 178.000000f;
	app->robotScene.robot.hipAngle = -836.000000f;
	app->robotScene.robot.kneeAngle = 36.000000f;
	app->robotScene.robot.kneeCircle.x = 0.000000f;
	app->robotScene.robot.kneeCircle.y = -1.291971f;
	app->robotScene.robot.kneeRadius = 0.080000f;
	app->robotScene.robot.thighArc1Angle = 161.913239f;
	app->robotScene.robot.thighArc2Angle = -19.956024f;
	app->robotScene.robot.footCircle.x = 0.000000f;
	app->robotScene.robot.footCircle.y = -1.590574f;
	app->robotScene.robot.footRadius = 0.050000f;
	app->robotScene.robot.shinArc1Angle = 169.821564f;
	app->robotScene.robot.shinArc2Angle = -22.810974f;
}

void initStandingPosition(AppState* app)
{
	app->robotScene.robot.buttX = 0.600000f;
	app->robotScene.robot.headX = -0.600000f;
	app->robotScene.robot.y = -0.420000f;
	app->robotScene.robot.buttRadius = 0.288500f;
	app->robotScene.robot.headRadius = 0.081000f;
	app->robotScene.robot.seamArc1Angle = 122.854446f;
	app->robotScene.robot.seamArc2Angle = 237.145584f;
	app->robotScene.robot.innerCircle.x = 0.000000f;
	app->robotScene.robot.innerCircle.y = -0.420000f;
	app->robotScene.robot.innerRadius = 0.120000f;
	app->robotScene.robot.angle = 88.000000f;
	app->robotScene.robot.hipAngle = -760.000000f;
	app->robotScene.robot.kneeAngle = -26.000000f;
	app->robotScene.robot.kneeCircle.x = 0.000000f;
	app->robotScene.robot.kneeCircle.y = -0.941971f;
	app->robotScene.robot.kneeRadius = 0.080000f;
	app->robotScene.robot.thighArc1Angle = 161.913239f;
	app->robotScene.robot.thighArc2Angle = -19.956024f;
	app->robotScene.robot.footCircle.x = 0.000000f;
	app->robotScene.robot.footCircle.y = -1.240574f;
	app->robotScene.robot.footRadius = 0.050000f;
	app->robotScene.robot.shinArc1Angle = 169.821564f;
	app->robotScene.robot.shinArc2Angle = -22.810974f;
}

// ---- Rocky ----
//
// angle is kept at 0 for both of Rocky's poses -- unlike Semni/Stilo's
// torso (which needs a ~90 degree whole-body rotation to turn its
// horizontal head/butt pair into a vertical stack, see initStiloStanding-
// Position's comment), a rectangle already reads as an upright body at
// angle 0, so there's no rotation to reason about: every coordinate below
// is exactly where it ends up on screen.
void initRockyStandingPosition(AppState* app)
{
	app->robotScene.rocky.bodyX = 0.000000f;
	app->robotScene.rocky.bodyY = 0.150000f;
	app->robotScene.rocky.bodyHalfWidth = 0.300000f;
	app->robotScene.rocky.bodyHalfHeight = 0.400000f;
	app->robotScene.rocky.angle = 0.000000f;
	app->robotScene.rocky.kneeCircle.x = 0.000000f;
	app->robotScene.rocky.kneeCircle.y = -0.300000f;
	app->robotScene.rocky.kneeRadius = 0.080000f;
	app->robotScene.rocky.kneeAngle = 0.000000f;
	app->robotScene.rocky.footCircle.x = 0.000000f;
	app->robotScene.rocky.footCircle.y = -0.600000f;
	app->robotScene.rocky.footRadius = 0.050000f;
	// shin arc angles borrowed directly from Semni's own knee-to-foot
	// defaults (initStandingPosition above) -- same construction, same
	// knee/foot radii, so the same angles already look right.
	app->robotScene.rocky.shinArc1Angle = 169.821564f;
	app->robotScene.rocky.shinArc2Angle = -22.810974f;
}

void initRockyHomePosition(AppState* app)
{
	app->robotScene.rocky.bodyX = 0.000000f;
	app->robotScene.rocky.bodyY = 0.150000f;
	app->robotScene.rocky.bodyHalfWidth = 0.300000f;
	app->robotScene.rocky.bodyHalfHeight = 0.400000f;
	app->robotScene.rocky.angle = 0.000000f;
	app->robotScene.rocky.kneeCircle.x = 0.000000f;
	app->robotScene.rocky.kneeCircle.y = -0.300000f;
	app->robotScene.rocky.kneeRadius = 0.080000f;
	// bent knee for a crouched "home" pose -- angle stays 0 (whole body
	// untouched), only the leg swings around kneeCircle
	app->robotScene.rocky.kneeAngle = -35.000000f;
	app->robotScene.rocky.footCircle.x = 0.000000f;
	app->robotScene.rocky.footCircle.y = -0.600000f;
	app->robotScene.rocky.footRadius = 0.050000f;
	app->robotScene.rocky.shinArc1Angle = 169.821564f;
	app->robotScene.rocky.shinArc2Angle = -22.810974f;
}

// ---- Stilo ----
//
// Same torso construction as Semni (head/butt/seam arcs), so angle = 90
// again turns the local horizontal head/butt pair into a vertical stack
// (see rotatePoint's math: a point offset (dx, 0) from the pivot lands at
// world offset (0, dx) after a 90 degree rotation -- exactly what makes
// Semni's own Standing pose read as upright). innerCircle is placed
// EXACTLY at the torso's own pivot (getStiloCenter), same coincidence
// Semni's Standing pose has (its innerCircle sits at its own getCenter
// too) -- that pins the hip in place regardless of angle. hipAngle is set
// to exactly -angle so the two rotations cancel (rotating twice around
// the SAME point is just one rotation by their sum), which means
// footCircle's stored coordinates below are already its exact final
// on-screen offset from the hip -- no compounded rotation to reason
// about, unlike Semni's own hand-tuned hipAngle.
void initStiloStandingPosition(AppState* app)
{
	app->robotScene.stilo.headX = -0.100000f;
	app->robotScene.stilo.buttX = 0.100000f;
	app->robotScene.stilo.y = -0.300000f;
	app->robotScene.stilo.headRadius = 0.090000f;
	app->robotScene.stilo.buttRadius = 0.280000f;
	app->robotScene.stilo.seamArc1Angle = 122.854446f;
	app->robotScene.stilo.seamArc2Angle = 237.145584f;
	app->robotScene.stilo.innerCircle.x = 0.000000f;
	app->robotScene.stilo.innerCircle.y = -0.300000f;
	app->robotScene.stilo.innerRadius = 0.120000f;
	app->robotScene.stilo.angle = 90.000000f;
	app->robotScene.stilo.hipAngle = -90.000000f;
	app->robotScene.stilo.footCircle.x = 0.000000f;
	app->robotScene.stilo.footCircle.y = -0.850000f;
	app->robotScene.stilo.footRadius = 0.060000f;
	app->robotScene.stilo.thighArc1Angle = 170.000000f;
	app->robotScene.stilo.thighArc2Angle = -15.000000f;
}

void initStiloHomePosition(AppState* app)
{
	app->robotScene.stilo.headX = -0.100000f;
	app->robotScene.stilo.buttX = 0.100000f;
	app->robotScene.stilo.y = -0.550000f;
	app->robotScene.stilo.headRadius = 0.090000f;
	app->robotScene.stilo.buttRadius = 0.280000f;
	app->robotScene.stilo.seamArc1Angle = 122.854446f;
	app->robotScene.stilo.seamArc2Angle = 237.145584f;
	app->robotScene.stilo.innerCircle.x = 0.000000f;
	app->robotScene.stilo.innerCircle.y = -0.550000f;
	app->robotScene.stilo.innerRadius = 0.120000f;
	app->robotScene.stilo.angle = 90.000000f;
	app->robotScene.stilo.hipAngle = -90.000000f;
	// leg tucked up and to the side (shorter, angled offset from the hip)
	// for a crouched "home" pose, instead of straight down
	app->robotScene.stilo.footCircle.x = 0.200000f;
	app->robotScene.stilo.footCircle.y = -0.800000f;
	app->robotScene.stilo.footRadius = 0.060000f;
	app->robotScene.stilo.thighArc1Angle = 170.000000f;
	app->robotScene.stilo.thighArc2Angle = -15.000000f;
}

void initAppState(AppState* app)
{
	initStandingPosition(app);
	initRockyStandingPosition(app);
	initStiloStandingPosition(app);
	app->robotScene.activeKind = ROBOT_KIND_SEMNI;
}
