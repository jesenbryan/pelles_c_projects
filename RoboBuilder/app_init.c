#include "app.h"
#include "save.h" // for loadRobotPoseFromFile/loadRockyPoseFromFile/loadStiloPoseFromFile -- see initAppState below

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
// Same torso as initStandingPosition (Semni's own Standing pose) above,
// but each leg is now just a hip circle + feet circle joined by a pair of
// thigh arcs -- no knee/shin/foot sub-stage any more (see app.h's Stilo
// comment). feet1Radius/feet1Circle carry over the exact position/size the
// old kneeCircle/kneeRadius had (feet1 IS the old knee joint, just
// relabeled as the leg's new end), and thigh1Arc1Angle/thigh1Arc2Angle
// carry over the old thighArc1Angle/thighArc2Angle unchanged (same two
// circles, same fillet).
void initStiloStandingPosition(AppState* app)
{
	app->robotScene.stilo.buttX = 0.600000f;
	app->robotScene.stilo.headX = -0.600000f;
	app->robotScene.stilo.y = -0.420000f;
	app->robotScene.stilo.buttRadius = 0.288500f;
	app->robotScene.stilo.headRadius = 0.081000f;
	app->robotScene.stilo.seamArc1Angle = 122.854446f;
	app->robotScene.stilo.seamArc2Angle = 237.145584f;
	app->robotScene.stilo.hip1Circle.x = 0.000000f;
	app->robotScene.stilo.hip1Circle.y = -0.420000f;
	app->robotScene.stilo.hip1Radius = 0.120000f;
	app->robotScene.stilo.angle = 88.000000f;
	app->robotScene.stilo.hip1Angle = -760.000000f;
	app->robotScene.stilo.feet1Circle.x = 0.000000f;
	app->robotScene.stilo.feet1Circle.y = -0.941971f;
	app->robotScene.stilo.feet1Radius = 0.080000f;
	app->robotScene.stilo.thigh1Arc1Angle = 161.913239f;
	app->robotScene.stilo.thigh1Arc2Angle = -19.956024f;

	// ---- leg 2 ----
	//
	// A straight copy of leg 1's pose above -- same radii, same hip angle,
	// same thigh arcs -- just rigidly shifted by a fixed (+0.25, 0) in the
	// torso's own LOCAL frame (hip1Circle/feet1Circle both shifted by the
	// identical delta, same idea as robot.c's translateRobot), so leg 2
	// keeps leg 1's exact shape but no longer sits exactly on top of it --
	// both visible and independently draggable via its own hip/feet
	// handles. Fully interactive from here: drag any of leg 2's handles to
	// pose it differently.
	app->robotScene.stilo.hip2Circle.x = 0.250000f;
	app->robotScene.stilo.hip2Circle.y = -0.420000f;
	app->robotScene.stilo.hip2Radius = 0.120000f;
	app->robotScene.stilo.hip2Angle = -760.000000f;
	app->robotScene.stilo.feet2Circle.x = 0.250000f;
	app->robotScene.stilo.feet2Circle.y = -0.941971f;
	app->robotScene.stilo.feet2Radius = 0.080000f;
	app->robotScene.stilo.thigh2Arc1Angle = 161.913239f;
	app->robotScene.stilo.thigh2Arc2Angle = -19.956024f;
}

// Same torso as initHomePosition (Semni's own Home pose) above, with the
// same hip+feet-only leg shape as initStiloStandingPosition above -- see
// its comment for why feet1/feet2 and thigh1/thigh2 carry over the old
// knee/thighArc numbers unchanged.
void initStiloHomePosition(AppState* app)
{
	app->robotScene.stilo.buttX = 0.600000f;
	app->robotScene.stilo.headX = -0.600000f;
	app->robotScene.stilo.y = -0.770000f;
	app->robotScene.stilo.buttRadius = 0.288500f;
	app->robotScene.stilo.headRadius = 0.081000f;
	app->robotScene.stilo.seamArc1Angle = 122.854446f;
	app->robotScene.stilo.seamArc2Angle = 237.145584f;
	app->robotScene.stilo.hip1Circle.x = 0.000000f;
	app->robotScene.stilo.hip1Circle.y = -0.770000f;
	app->robotScene.stilo.hip1Radius = 0.120000f;
	app->robotScene.stilo.angle = 178.000000f;
	app->robotScene.stilo.hip1Angle = -836.000000f;
	app->robotScene.stilo.feet1Circle.x = 0.000000f;
	app->robotScene.stilo.feet1Circle.y = -1.291971f;
	app->robotScene.stilo.feet1Radius = 0.080000f;
	app->robotScene.stilo.thigh1Arc1Angle = 161.913239f;
	app->robotScene.stilo.thigh1Arc2Angle = -19.956024f;

	// ---- leg 2 -- same "rigid +0.25 local-X shift of leg 1" construction
	// as initStiloStandingPosition's own leg 2 above, just under the Home
	// pose's own numbers.
	app->robotScene.stilo.hip2Circle.x = 0.250000f;
	app->robotScene.stilo.hip2Circle.y = -0.770000f;
	app->robotScene.stilo.hip2Radius = 0.120000f;
	app->robotScene.stilo.hip2Angle = -836.000000f;
	app->robotScene.stilo.feet2Circle.x = 0.250000f;
	app->robotScene.stilo.feet2Circle.y = -1.291971f;
	app->robotScene.stilo.feet2Radius = 0.080000f;
	app->robotScene.stilo.thigh2Arc1Angle = 161.913239f;
	app->robotScene.stilo.thigh2Arc2Angle = -19.956024f;
}

void initAppState(AppState* app)
{
	// Seed every robot kind into its Home pose on startup -- "custom file
	// first, hardcoded default as fallback" pattern, same as input.c's
	// robot-kind-switch handler (WM_COMMAND's kind combo box) and the
	// Home button (ID_HOME_POSITION_BUTTON) both already use, so a user
	// who has saved their own custom Home pose (Poses\*_home.txt, see
	// ID_SET_HOME_BUTTON) sees THAT pose right from launch, not just
	// initHomePosition/initRockyHomePosition/initStiloHomePosition's own
	// hardcoded numbers. Falls back to the hardcoded pose for whichever
	// kind has no saved file yet (e.g. a fresh install with no Poses
	// folder), exactly like every other caller of these loaders.
	if (!loadRobotPoseFromFile("Poses\\semni_home.txt", &app->robotScene.robot))
		initHomePosition(app);

	if (!loadRockyPoseFromFile("Poses\\rocky_home.txt", &app->robotScene.rocky))
		initRockyHomePosition(app);

	initStiloHomePosition(app);
	loadStiloPoseFromFile("Poses\\stilo_home.txt", &app->robotScene.stilo);

	app->robotScene.activeKind = ROBOT_KIND_SEMNI;

	// Rob.txt/Arm.txt export weights (see app.h's Rocky.bodyWeight/
	// legWeight and input.c's hWeightRatioSlider) -- deliberately
	// set here ONCE rather than inside initRockyStandingPosition/
	// initRockyHomePosition above, since those two also get re-run every
	// time the Standing/Home buttons are clicked (see input.c's
	// ID_STANDING_POSITION_BUTTON/ID_HOME_POSITION_BUTTON) -- putting the
	// default there would silently wipe out whatever weight the user had
	// already typed in every time they re-pose the robot.
	// 0.5/0.5 instead of an arbitrary equal value like 1.0/1.0 -- only
	// the RATIO between these two ever matters (see save.c/renderer.c's
	// computeRockyMassCenterWorld), so this is numerically identical to
	// 1.0/1.0, but reads immediately as "50/50 split" the way the boxes
	// are meant to be understood, matching the initial 50/50 mass-center
	// position (exactly halfway between the rectangle's and leg's own
	// centroids) this default already produces.
	app->robotScene.rocky.bodyWeight = 0.5f;
	app->robotScene.rocky.legWeight = 0.5f;

	// Separate real total-mass export value (app.h's Rocky.actualWeight,
	// input.c's hActualWeightEdit) -- same one-time-default reasoning as
	// bodyWeight/legWeight just above.
	app->robotScene.rocky.actualWeight = 1.0f;

	// Same bodyWeight/legWeight/actualWeight defaults for Semni and Stilo
	// now that they have their own SemniExport/StiloExport weight-share
	// export (see save.c's saveSemniAsRobLeg/saveStiloAsRobLeg) and their
	// own draggable mass-center dot (renderer.c's
	// computeSemniMassCenterWorld/computeStiloMassCenterWorld) -- same
	// one-time-only placement (not inside any Standing/Home pose init) and
	// same 0.5/0.5/1.0 reasoning as Rocky's own version just above. Stilo's
	// legWeight is the one COMBINED value for both legs together (see
	// app.h's own comment on Stilo's legWeight).
	app->robotScene.robot.bodyWeight = 0.5f;
	app->robotScene.robot.legWeight = 0.5f;
	app->robotScene.robot.actualWeight = 1.0f;

	app->robotScene.stilo.bodyWeight = 0.5f;
	app->robotScene.stilo.legWeight = 0.5f;
	app->robotScene.stilo.actualWeight = 1.0f;
}
