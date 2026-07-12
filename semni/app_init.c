#include "app.h"

void initAppState(AppState* app)
{
    app->robotScene.robot.headX = -0.600000f;
    app->robotScene.robot.buttX = 0.600000f;
    app->robotScene.robot.y = 0.000000f;

    app->robotScene.robot.headRadius = 0.101000f;
    app->robotScene.robot.buttRadius = 0.288500f;

    // arbitrary starting attach angles (mirrored top/bottom) -- verified
    // by hand to land well within the valid, non-degenerate range for
    // this head/butt layout (gives a fillet radius around 0.93). Drag the
    // seam handles from here.
    app->robotScene.robot.topArcAngle = 150.000000f;
    app->robotScene.robot.bottomArcAngle = -150.000000f;

    app->robotScene.robot.innerCircle.x = 0.000000f;
    app->robotScene.robot.innerCircle.y = 0.000000f;

    app->robotScene.robot.innerRadius = 0.120000f;

    app->robotScene.robot.angle = 56.000000f;
    app->robotScene.robot.hipAngle = 2.000000f;
    app->robotScene.robot.kneeAngle = -78.000000f;

    // knee joint sits below the hip (innerCircle); the two thigh arcs
    // bulge outward on either side so the connecting arcs read as a thigh
    app->robotScene.robot.kneeCircle.x = 0.000000f;
    app->robotScene.robot.kneeCircle.y = -0.521971f;

    app->robotScene.robot.kneeRadius = 0.080000f;

    // arbitrary starting attach angles (independent, not mirrored -- see
    // thighArc1Angle/thighArc2Angle's comment in app.h) -- verified by
    // hand to land well within the valid, non-degenerate range for this
    // hip/knee layout. thighArc1Angle uses the usual convex (outward-
    // bulging) construction, fillet radius around 0.57. thighArc2Angle
    // uses the concave (inward-pinching) construction instead -- its
    // safe range is centered on the OPPOSITE side of the hip circle
    // (facing the knee), so 140 degrees no longer means anything for it;
    // -130 lands comfortably inside the concave range (fillet radius
    // around 0.26). Drag the thigh handles from here.
    app->robotScene.robot.thighArc1Angle = 30.000000f;
    app->robotScene.robot.thighArc2Angle = -130.000000f;

    // ankle sits below the knee, and the shin tapers in a bit more than
    // the thigh did, continuing the leg
    app->robotScene.robot.ankleCircle.x = 0.000000f;
    app->robotScene.robot.ankleCircle.y = -0.820574f;

    app->robotScene.robot.ankleRadius = 0.050000f;

    // arbitrary starting attach angles (independent, not mirrored -- same
    // convention as thighArc1Angle/thighArc2Angle) -- verified by hand to
    // land well within the valid, non-degenerate range for this knee/ankle
    // layout. Drag the shin handles from here.
    app->robotScene.robot.shinArc1Angle = 30.000000f;
    app->robotScene.robot.shinArc2Angle = 140.000000f;
}
