#include <stdio.h>
#include <windows.h>
#include <commctrl.h>
#include <math.h>

#include "app.h"
#include "input.h"
#include "geometry.h"
#include "config.h"

#include "robot.h"
#include "graphics.h"
#include "save.h"
#include "app_init.h"

// ---------------- ARC AUTO-ADJUST ----------------
// Re-validates an arc pair's EXISTING angles against a circle's new
// size/position, nudging an angle only as far as needed to keep its
// fillet solve from exceeding maxRadius -- geometry.h's
// clampToSafeAngleRange is a no-op when the current angle is already
// safe, so an arc that's still comfortably valid after the resize/move
// is left completely untouched, and one that isn't gets pulled back in
// by the minimum amount rather than reset to some fixed default. Same
// outer-bound margin (ARC_ANGLE_MARGIN_DEG and friends) the drag code
// already computes maxDelta with -- this just applies it to the angle
// that's already there instead of one freshly read off the mouse.
static void adjustHeadButtArcs(AppState* app)
{
    PointF headLocal = { app->robotScene.robot.headX, app->robotScene.robot.y };
    PointF buttLocal = { app->robotScene.robot.buttX, app->robotScene.robot.y };

    SafeAngleRange range = filletSafeAngleRange(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, MAX_ARC_R);

    app->robotScene.robot.seamArc1Angle = clampToSafeAngleRange(app->robotScene.robot.seamArc1Angle, range, ARC_ANGLE_MARGIN_DEG);
    app->robotScene.robot.seamArc2Angle = clampToSafeAngleRange(app->robotScene.robot.seamArc2Angle, range, ARC_ANGLE_MARGIN_DEG);
}

static void adjustThighArcs(AppState* app)
{
    SafeAngleRange range1 = filletSafeAngleRange(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, MAX_THIGH_ARC_R);
    SafeAngleRange range2 = filletSafeAngleRangeConcave(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, MAX_THIGH_ARC2_CONCAVE_R);

    app->robotScene.robot.thighArc1Angle = clampToSafeAngleRange(app->robotScene.robot.thighArc1Angle, range1, THIGH_ARC_ANGLE_MARGIN_DEG);
    app->robotScene.robot.thighArc2Angle = clampToSafeAngleRange(app->robotScene.robot.thighArc2Angle, range2, THIGH_ARC_ANGLE_MARGIN_DEG);
}

static void adjustShinArcs(AppState* app)
{
    SafeAngleRange range1 = filletSafeAngleRange(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius, MAX_SHIN_ARC_R);
    SafeAngleRange range2 = filletSafeAngleRangeConcave(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius, MAX_SHIN_ARC2_CONCAVE_R);

    app->robotScene.robot.shinArc1Angle = clampToSafeAngleRange(app->robotScene.robot.shinArc1Angle, range1, SHIN_ARC_ANGLE_MARGIN_DEG);
    app->robotScene.robot.shinArc2Angle = clampToSafeAngleRange(app->robotScene.robot.shinArc2Angle, range2, SHIN_ARC_ANGLE_MARGIN_DEG);
}

LRESULT handleInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, AppState* app)
{
    switch (msg)
    {
        case WM_LBUTTONDOWN:
        {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);

            screenToGL(hwnd, mx, my, &app->mouseGL.x, &app->mouseGL.y);

            app->draggingSeamArc1 = 0;
            app->draggingSeamArc2 = 0;
            app->draggingInner = 0;
            app->draggingKnee = 0;
            app->draggingThigh1 = 0;
            app->draggingThigh2 = 0;
            app->draggingAnkle = 0;
            app->draggingShin1 = 0;
            app->draggingShin2 = 0;

            PointF center = getCenter(app->robotScene.robot);

            PointF mouse = app->mouseGL;

            // seam attach handles: pinned to the exact midpoint between
            // head and butt on X, with Y solved from the arc's actual
            // fillet circle at that exact X (circleAtX) -- same
            // construction as drawSemniHandles, so the hit-test matches
            // exactly where the handle is actually drawn
            PointF headLocal = { app->robotScene.robot.headX, app->robotScene.robot.y };
            PointF buttLocal = { app->robotScene.robot.buttX, app->robotScene.robot.y };
            PointF bodyMidLocal = { (headLocal.x + buttLocal.x) * 0.5f, (headLocal.y + buttLocal.y) * 0.5f };

            Fillet seamArc1Fillet = filletFromAttachAngle(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, app->robotScene.robot.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
            Fillet seamArc2Fillet = filletFromAttachAngle(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, app->robotScene.robot.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);

            PointF seamArc1NearLocal = circleTowardPoint(seamArc1Fillet.center, seamArc1Fillet.radius, bodyMidLocal);
            PointF seamArc2NearLocal = circleTowardPoint(seamArc2Fillet.center, seamArc2Fillet.radius, bodyMidLocal);

            PointF seamArc1MidLocal = circleAtX(seamArc1Fillet.center, seamArc1Fillet.radius, bodyMidLocal.x, seamArc1NearLocal);
            PointF seamArc2MidLocal = circleAtX(seamArc2Fillet.center, seamArc2Fillet.radius, bodyMidLocal.x, seamArc2NearLocal);

            PointF seamArc1HandleWorld = rotatePoint(seamArc1MidLocal, center, app->robotScene.robot.angle);
            PointF seamArc2HandleWorld = rotatePoint(seamArc2MidLocal, center, app->robotScene.robot.angle);

            PointF innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, app->robotScene.robot.angle);

            // the leg chain (knee, thigh handles, ankle, shin handles) lives
            // in a frame that's additionally rotated by hipAngle around
            // innerCircle, independent of the whole-body angle
            PointF hipPivot = app->robotScene.robot.innerCircle;
            float hipAngle = app->robotScene.robot.hipAngle;

            PointF kneeWorld = jointToWorld(app->robotScene.robot.kneeCircle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // thigh arc handles: same tangent-fillet + circleAtAxisMid
            // construction used in renderer.c's drawThighHandles, so the
            // hit-test matches exactly where the handle is actually drawn
            PointF thighAxisMidLocal = { (app->robotScene.robot.innerCircle.x + app->robotScene.robot.kneeCircle.x) * 0.5f,
                                         (app->robotScene.robot.innerCircle.y + app->robotScene.robot.kneeCircle.y) * 0.5f };

            Fillet thigh1Fillet = filletFromAttachAngle(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius,
                                                         app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                         app->robotScene.robot.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
            PointF thigh1NearLocal = circleTowardPoint(thigh1Fillet.center, thigh1Fillet.radius, thighAxisMidLocal);
            PointF thigh1MidLocal = circleAtAxisMid(thigh1Fillet.center, thigh1Fillet.radius, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, thigh1NearLocal);

            // thighArc2Angle uses the concave construction (bulges inward
            // instead of outward) -- see app.h's comment. circleTowardPoint
            // + circleAtAxisMid work unchanged for it (purely geometric,
            // don't care whether the fillet is internally or externally
            // tangent), only the fillet solve itself differs.
            Fillet thigh2Fillet = filletFromAttachAngleConcave(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius,
                                                                app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                                app->robotScene.robot.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
            PointF thigh2NearLocal = circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, thighAxisMidLocal);
            PointF thigh2MidLocal = circleAtAxisMid(thigh2Fillet.center, thigh2Fillet.radius, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, thigh2NearLocal);

            PointF thigh1World = jointToWorld(thigh1MidLocal, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            PointF thigh2World = jointToWorld(thigh2MidLocal, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // same leg-local frame the drag math in WM_MOUSEMOVE uses --
            // computed once here so both thigh handles' click-capture can
            // read the mouse's perpendicular-to-axis position at the
            // moment the drag starts
            PointF legLocalMouseDown = inverseRotate(inverseRotate(mouse, center, app->robotScene.robot.angle), hipPivot, hipAngle);

            // the shin (ankle, shin handles) hangs off the knee, which
            // itself hangs off the hip -- two nested joint rotations
            // before it lands in world space
            PointF kneePivot = app->robotScene.robot.kneeCircle;
            float kneeAngle = app->robotScene.robot.kneeAngle;

            PointF ankleWorld = nestedJointToWorld(app->robotScene.robot.ankleCircle, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // shin arc handles: same tangent-fillet + circleAtAxisMid
            // construction used for the thigh handles above (and in
            // renderer.c's drawShinHandles), just between kneeCircle and
            // ankleCircle instead of innerCircle and kneeCircle, so the
            // hit-test matches exactly where the handle is actually drawn
            PointF shinAxisMidLocal = { (app->robotScene.robot.kneeCircle.x + app->robotScene.robot.ankleCircle.x) * 0.5f,
                                        (app->robotScene.robot.kneeCircle.y + app->robotScene.robot.ankleCircle.y) * 0.5f };

            Fillet shin1Fillet = filletFromAttachAngle(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                        app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius,
                                                        app->robotScene.robot.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
            PointF shin1NearLocal = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, shinAxisMidLocal);
            PointF shin1MidLocal = circleAtAxisMid(shin1Fillet.center, shin1Fillet.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, shin1NearLocal);

            // shinArc2Angle uses the concave construction (bulges inward
            // instead of outward -- see app.h's comment), same as
            // thighArc2Angle
            Fillet shin2Fillet = filletFromAttachAngleConcave(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                               app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius,
                                                               app->robotScene.robot.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
            PointF shin2NearLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, shinAxisMidLocal);
            PointF shin2MidLocal = circleAtAxisMid(shin2Fillet.center, shin2Fillet.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, shin2NearLocal);

            PointF shin1World = nestedJointToWorld(shin1MidLocal, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            PointF shin2World = nestedJointToWorld(shin2MidLocal, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // same leg-local frame the drag math in WM_MOUSEMOVE uses --
            // computed once here so both shin handles' click-capture can
            // read the mouse's perpendicular-to-axis position (relative to
            // the knee->ankle axis) at the moment the drag starts
            PointF shinLocalMouseDown = inverseRotate(legLocalMouseDown, kneePivot, kneeAngle);

            if (isNear(mouse, seamArc1HandleWorld, ARC_HANDLE_RADIUS))
            {
                app->draggingSeamArc1 = 1;
                app->activeHandle = 1;

                // remember where the drag started (mouse Y + current
                // angle) so WM_MOUSEMOVE can nudge the angle incrementally
                // from here instead of solving an absolute position
                app->arcDragStartMouseY = inverseRotate(mouse, center, app->robotScene.robot.angle).y;
                app->arcDragStartAngle = app->robotScene.robot.seamArc1Angle;
            }
            else if (isNear(mouse, seamArc2HandleWorld, ARC_HANDLE_RADIUS))
            {
                app->draggingSeamArc2 = 1;
                app->activeHandle = 2;

                app->arcDragStartMouseY = inverseRotate(mouse, center, app->robotScene.robot.angle).y;
                app->arcDragStartAngle = app->robotScene.robot.seamArc2Angle;
            }
            else if (isNear(mouse, innerWorld, HIP_HANDLE_RADIUS))
            {
                app->draggingInner = 1;
                app->activeHandle = 3;

                // remember the whole leg's offset from the hip right now,
                // so dragging the hip can carry it along as one rigid
                // piece instead of leaving it behind
                PointF hip = app->robotScene.robot.innerCircle;

                app->hipDragKneeOffset.x = app->robotScene.robot.kneeCircle.x - hip.x;
                app->hipDragKneeOffset.y = app->robotScene.robot.kneeCircle.y - hip.y;

                // thighArc1Angle/thighArc2Angle/shinArc1Angle/shinArc2Angle
                // need no offset -- they're already relative to their own
                // joint (hip or knee), unaffected by moving innerCircle
                // itself

                app->hipDragAnkleOffset.x = app->robotScene.robot.ankleCircle.x - hip.x;
                app->hipDragAnkleOffset.y = app->robotScene.robot.ankleCircle.y - hip.y;
            }
            else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS))
            {
                app->draggingKnee = 1;
                app->activeHandle = 4;

                // remember the shin's offset from the knee right now, so
                // it can be pinned to this exact shape/length for the
                // whole drag instead of drifting frame by frame
                PointF knee = app->robotScene.robot.kneeCircle;

                app->kneeDragAnkleOffset.x = app->robotScene.robot.ankleCircle.x - knee.x;
                app->kneeDragAnkleOffset.y = app->robotScene.robot.ankleCircle.y - knee.y;

                // shinArc1Angle/shinArc2Angle need no offset -- they're
                // already knee-frame-relative angles, unaffected by moving
                // kneeCircle itself
            }
            else if (isNear(mouse, thigh1World, THIGH_HANDLE_RADIUS))
            {
                app->draggingThigh1 = 1;
                app->activeHandle = 5;

                // remember where the drag started (mouse's perpendicular-
                // to-axis offset + current angle) so WM_MOUSEMOVE can nudge
                // the angle incrementally from here, same idea as
                // arcDragStartMouseY/arcDragStartAngle for the seam arcs
                app->thighArcDragStartPerp = perpOffsetOnAxis(legLocalMouseDown, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                app->thighArcDragStartAngle = app->robotScene.robot.thighArc1Angle;
            }
            else if (isNear(mouse, thigh2World, THIGH_HANDLE_RADIUS))
            {
                app->draggingThigh2 = 1;
                app->activeHandle = 6;

                app->thighArcDragStartPerp = perpOffsetOnAxis(legLocalMouseDown, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                app->thighArcDragStartAngle = app->robotScene.robot.thighArc2Angle;
            }
            else if (isNear(mouse, ankleWorld, ANKLE_HANDLE_RADIUS))
            {
                app->draggingAnkle = 1;
                app->activeHandle = 7;
            }
            else if (isNear(mouse, shin1World, SHIN_HANDLE_RADIUS))
            {
                app->draggingShin1 = 1;
                app->activeHandle = 8;

                // remember where the drag started (mouse's perpendicular-
                // to-axis offset, relative to the knee->ankle axis + the
                // current angle), same idea as thighArcDragStartPerp/Angle
                app->shinArcDragStartPerp = perpOffsetOnAxis(shinLocalMouseDown, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                app->shinArcDragStartAngle = app->robotScene.robot.shinArc1Angle;
            }
            else if (isNear(mouse, shin2World, SHIN_HANDLE_RADIUS))
            {
                app->draggingShin2 = 1;
                app->activeHandle = 9;

                app->shinArcDragStartPerp = perpOffsetOnAxis(shinLocalMouseDown, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                app->shinArcDragStartAngle = app->robotScene.robot.shinArc2Angle;
            }
        }
        break;

        case WM_LBUTTONUP:
        {
            app->draggingSeamArc1 = 0;
            app->draggingSeamArc2 = 0;
            app->draggingInner = 0;
            app->draggingKnee = 0;
            app->draggingThigh1 = 0;
            app->draggingThigh2 = 0;
            app->draggingAnkle = 0;
            app->draggingShin1 = 0;
            app->draggingShin2 = 0;
            app->activeHandle = 0;
        }
        break;

        case WM_MOUSEMOVE:
        {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);

            screenToGL(hwnd, mx, my, &app->mouseGL.x, &app->mouseGL.y);

            DWORD now = GetTickCount();

            if (now - app->lastLogTime >= 1000)
            {
                // dump the current robot pose as app_init.c-style assignments,
                // so it can be copied straight in as the new starting pose
                // once it's been shaped by hand with the handles
                printRobotAsInit(app->robotScene.robot);

                app->lastLogTime = now;
            }

            PointF mouse = app->mouseGL;

            PointF center = getCenter(app->robotScene.robot);
            float angle = app->robotScene.robot.angle;

            // hover state for the joint circle handles is tracked here,
            // continuously, regardless of whether anything is being
            // dragged -- that's what lets them highlight yellow just
            // from the mouse passing near them
            PointF innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, angle);
            PointF kneeWorld  = jointToWorld(app->robotScene.robot.kneeCircle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
            PointF ankleWorld = nestedJointToWorld(app->robotScene.robot.ankleCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
            PointF headWorld  = rotatePoint((PointF){app->robotScene.robot.headX, app->robotScene.robot.y}, center, angle);
            PointF buttWorld  = rotatePoint((PointF){app->robotScene.robot.buttX, app->robotScene.robot.y}, center, angle);

            app->hoverHip   = isNear(mouse, innerWorld, HIP_HANDLE_RADIUS);
            app->hoverKnee  = isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS);
            app->hoverAnkle = isNear(mouse, ankleWorld, ANKLE_HANDLE_RADIUS);
            app->hoverHead  = isNear(mouse, headWorld, HEAD_BUTT_HANDLE_RADIUS);
            app->hoverButt  = isNear(mouse, buttWorld, HEAD_BUTT_HANDLE_RADIUS);

            // bottom-left hover label: also needs the bulge/seam handle
            // positions (seam arc 1, seam arc 2, both thigh arcs, both
            // shin arcs), which the code below only computes while a drag
            // is active -- so they're worked out fresh here too, same
            // fillet + circleAtX/circleAtAxisMid construction
            // WM_LBUTTONDOWN's hit-test and renderer.c's drawSemniHandles/
            // drawThighHandles/drawShinHandles use, so this always lines
            // up with where each handle is actually drawn. Kept in its
            // own "Hover"-suffixed locals so it can't collide with the
            // (differently-scoped) drag-only versions of the same
            // computation further down.
            {
                PointF headLocalHover = { app->robotScene.robot.headX, app->robotScene.robot.y };
                PointF buttLocalHover = { app->robotScene.robot.buttX, app->robotScene.robot.y };
                PointF bodyMidLocalHover = { (headLocalHover.x + buttLocalHover.x) * 0.5f, (headLocalHover.y + buttLocalHover.y) * 0.5f };

                Fillet seamArc1FilletHover = filletFromAttachAngle(headLocalHover, app->robotScene.robot.headRadius, buttLocalHover, app->robotScene.robot.buttRadius, app->robotScene.robot.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
                Fillet seamArc2FilletHover = filletFromAttachAngle(headLocalHover, app->robotScene.robot.headRadius, buttLocalHover, app->robotScene.robot.buttRadius, app->robotScene.robot.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);

                PointF seamArc1NearLocalHover = circleTowardPoint(seamArc1FilletHover.center, seamArc1FilletHover.radius, bodyMidLocalHover);
                PointF seamArc2NearLocalHover = circleTowardPoint(seamArc2FilletHover.center, seamArc2FilletHover.radius, bodyMidLocalHover);

                PointF seamArc1MidLocalHover = circleAtX(seamArc1FilletHover.center, seamArc1FilletHover.radius, bodyMidLocalHover.x, seamArc1NearLocalHover);
                PointF seamArc2MidLocalHover = circleAtX(seamArc2FilletHover.center, seamArc2FilletHover.radius, bodyMidLocalHover.x, seamArc2NearLocalHover);

                PointF seamArc1HandleWorldHover = rotatePoint(seamArc1MidLocalHover, center, angle);
                PointF seamArc2HandleWorldHover = rotatePoint(seamArc2MidLocalHover, center, angle);

                PointF thighAxisMidLocalHover = { (app->robotScene.robot.innerCircle.x + app->robotScene.robot.kneeCircle.x) * 0.5f,
                                                  (app->robotScene.robot.innerCircle.y + app->robotScene.robot.kneeCircle.y) * 0.5f };

                Fillet thigh1FilletHover = filletFromAttachAngle(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
                PointF thigh1NearLocalHover = circleTowardPoint(thigh1FilletHover.center, thigh1FilletHover.radius, thighAxisMidLocalHover);
                PointF thigh1MidLocalHover = circleAtAxisMid(thigh1FilletHover.center, thigh1FilletHover.radius, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, thigh1NearLocalHover);

                Fillet thigh2FilletHover = filletFromAttachAngleConcave(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
                PointF thigh2NearLocalHover = circleTowardPoint(thigh2FilletHover.center, thigh2FilletHover.radius, thighAxisMidLocalHover);
                PointF thigh2MidLocalHover = circleAtAxisMid(thigh2FilletHover.center, thigh2FilletHover.radius, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, thigh2NearLocalHover);

                PointF thigh1WorldHover = jointToWorld(thigh1MidLocalHover, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
                PointF thigh2WorldHover = jointToWorld(thigh2MidLocalHover, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);

                PointF shinAxisMidLocalHover = { (app->robotScene.robot.kneeCircle.x + app->robotScene.robot.ankleCircle.x) * 0.5f,
                                                 (app->robotScene.robot.kneeCircle.y + app->robotScene.robot.ankleCircle.y) * 0.5f };

                Fillet shin1FilletHover = filletFromAttachAngle(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius, app->robotScene.robot.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
                PointF shin1NearLocalHover = circleTowardPoint(shin1FilletHover.center, shin1FilletHover.radius, shinAxisMidLocalHover);
                PointF shin1MidLocalHover = circleAtAxisMid(shin1FilletHover.center, shin1FilletHover.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, shin1NearLocalHover);

                Fillet shin2FilletHover = filletFromAttachAngleConcave(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius, app->robotScene.robot.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
                PointF shin2NearLocalHover = circleTowardPoint(shin2FilletHover.center, shin2FilletHover.radius, shinAxisMidLocalHover);
                PointF shin2MidLocalHover = circleAtAxisMid(shin2FilletHover.center, shin2FilletHover.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, shin2NearLocalHover);

                PointF shin1WorldHover = nestedJointToWorld(shin1MidLocalHover, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
                PointF shin2WorldHover = nestedJointToWorld(shin2MidLocalHover, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);

                // priority mirrors WM_LBUTTONDOWN's hit-test order
                // (seamArc1, seamArc2, hip, knee, thigh1, thigh2, ankle,
                // shin1, shin2), with head/butt appended at the end since
                // they aren't part of that click chain at all
                const wchar_t* hoverLabel = L"";

                if (isNear(mouse, seamArc1HandleWorldHover, ARC_HANDLE_RADIUS))
                    hoverLabel = L"Seam Arc 1";
                else if (isNear(mouse, seamArc2HandleWorldHover, ARC_HANDLE_RADIUS))
                    hoverLabel = L"Seam Arc 2";
                else if (app->hoverHip)
                    hoverLabel = L"Hip";
                else if (app->hoverKnee)
                    hoverLabel = L"Knee";
                else if (isNear(mouse, thigh1WorldHover, THIGH_HANDLE_RADIUS))
                    hoverLabel = L"Thigh Arc 1";
                else if (isNear(mouse, thigh2WorldHover, THIGH_HANDLE_RADIUS))
                    hoverLabel = L"Thigh Arc 2";
                else if (app->hoverAnkle)
                    hoverLabel = L"Ankle";
                else if (isNear(mouse, shin1WorldHover, SHIN_HANDLE_RADIUS))
                    hoverLabel = L"Shin Arc 1";
                else if (isNear(mouse, shin2WorldHover, SHIN_HANDLE_RADIUS))
                    hoverLabel = L"Shin Arc 2";
                // label text intentionally swapped vs. the flag name --
                // requested swap of "head"/"butt" as displayed NAMES only.
                // Doing it here (display-only) instead of swapping the
                // underlying headX/buttX/headRadius/buttRadius field
                // values (which was tried and reverted) avoids corrupting
                // seamArc1Angle/seamArc2Angle: those are stored as angles
                // measured specifically around the head circle
                // (filletFromAttachAngle/circleEdge take headLocal as
                // their asymmetric "c1" argument), so swapping which
                // physical circle headX/headRadius point to would silently
                // re-interpret those tuned angles around a different
                // circle entirely -- not just relabel them, which is what
                // caused the seam handle drag to look broken/inverted.
                else if (app->hoverHead)
                    hoverLabel = L"Butt";
                else if (app->hoverButt)
                    hoverLabel = L"Head";

                SetWindowText(app->ui.hHoverLabel, hoverLabel);
            }

            if (!app->draggingSeamArc1 && !app->draggingSeamArc2 &&
                !app->draggingInner &&
                !app->draggingKnee && !app->draggingThigh1 && !app->draggingThigh2 &&
                !app->draggingAnkle && !app->draggingShin1 && !app->draggingShin2)
                break;

            PointF localMouse = inverseRotate(mouse, center, angle);

            PointF headLocal = { app->robotScene.robot.headX, app->robotScene.robot.y };
            PointF buttLocal = { app->robotScene.robot.buttX, app->robotScene.robot.y };

            // the seam handle now sits at the arc's visible middle/bulge
            // point rather than the head-circle tangent point, so the
            // attach angle can no longer be read directly off the mouse's
            // polar angle around the head circle -- the handle isn't ON
            // that circle anymore. Instead it drags along ONE axis only
            // (vertical mouse movement), incrementally: angle = (angle
            // when the drag started) + (Y moved since then) * a
            // sensitivity constant -- see ARC_DRAG_SENSITIVITY_DEG_PER_UNIT
            // in config.h for why. Clamped on TWO sides: the far side (via
            // filletSafeAngleRange) stops it from flattening into a line,
            // and the near side keeps it away from centerDeg itself -- a
            // second, different degenerate point where the arc's bulge
            // collapses flat against the head-butt axis and flips to the
            // opposite side. Each handle is locked to its own side of
            // centerDeg (seam arc 1 stays negative-delta, seam arc 2 stays
            // positive-delta) so they can never cross into each other's
            // territory.
            //
            // The two arcs are kept symmetrical: dragging either handle
            // also updates the OTHER arc's angle to centerDeg - delta --
            // i.e. the same distance from centerDeg, mirrored to the
            // opposite side. Since both arcs share the exact same
            // centerDeg/safe range (it only depends on headLocal/buttLocal/
            // radii, not on which handle is being dragged), the mirrored
            // delta is automatically valid for the other arc too -- no
            // extra clamping needed.
            if (app->draggingSeamArc1)
            {
                SafeAngleRange range = filletSafeAngleRange(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, MAX_ARC_R);
                float maxDelta = range.halfWidthDeg - ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < ARC_SIDE_MARGIN_DEG) maxDelta = ARC_SIDE_MARGIN_DEG;

                float deltaY = localMouse.y - app->arcDragStartMouseY;
                float raw = app->arcDragStartAngle + deltaY * ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                if (delta > -ARC_SIDE_MARGIN_DEG) delta = -ARC_SIDE_MARGIN_DEG;
                if (delta < -maxDelta) delta = -maxDelta;

                app->robotScene.robot.seamArc1Angle = range.centerDeg + delta;
                app->robotScene.robot.seamArc2Angle = range.centerDeg - delta;
            }

            if (app->draggingSeamArc2)
            {
                SafeAngleRange range = filletSafeAngleRange(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, MAX_ARC_R);
                float maxDelta = range.halfWidthDeg - ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < ARC_SIDE_MARGIN_DEG) maxDelta = ARC_SIDE_MARGIN_DEG;

                float deltaY = localMouse.y - app->arcDragStartMouseY;
                float raw = app->arcDragStartAngle + deltaY * ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                // mirror image of seam arc 1's clamp -- locked to the
                // opposite (positive-delta) side of centerDeg
                if (delta < ARC_SIDE_MARGIN_DEG) delta = ARC_SIDE_MARGIN_DEG;
                if (delta > maxDelta) delta = maxDelta;

                app->robotScene.robot.seamArc2Angle = range.centerDeg + delta;
                app->robotScene.robot.seamArc1Angle = range.centerDeg - delta;
            }

            // dragging the hip carries the whole leg along as one rigid
            // piece -- each descendant point is re-applied fresh from its
            // offset (captured when the drag started) relative to the new
            // hip position, same "captured offset" pattern used below for
            // the knee drag, so the leg's shape/pose never drifts or gets
            // left behind
            if (app->draggingInner)
            {
                PointF newInner = localMouse;

                app->robotScene.robot.kneeCircle.x = newInner.x + app->hipDragKneeOffset.x;
                app->robotScene.robot.kneeCircle.y = newInner.y + app->hipDragKneeOffset.y;

                // thighArc1Angle/thighArc2Angle don't move with the hip --
                // they're already relative to it

                app->robotScene.robot.ankleCircle.x = newInner.x + app->hipDragAnkleOffset.x;
                app->robotScene.robot.ankleCircle.y = newInner.y + app->hipDragAnkleOffset.y;

                // shinArc1Angle/shinArc2Angle don't move with the hip
                // either -- already relative to kneeCircle, which itself
                // just got carried along via hipDragKneeOffset above

                app->robotScene.robot.innerCircle = newInner;

                // dragging the hip carries the whole leg as a rigid
                // translation, so the thigh/shin arcs' underlying
                // distances never actually change and this ends up a
                // no-op -- called anyway for the same reason as the
                // length-changing drags below (consistency: moving any
                // circle re-validates its arcs), and it's cheap since
                // clampToSafeAngleRange leaves an already-safe angle alone
                adjustThighArcs(app);
                adjustShinArcs(app);
            }

            // the leg chain sits in a frame additionally rotated by hipAngle
            // around innerCircle, so undo that rotation too before storing
            // the raw local coordinates for knee/thigh/ankle/shin
            PointF legLocalMouse = inverseRotate(localMouse, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle);

            // dragging the knee circle only changes the thigh's length --
            // it's constrained to slide along the hip->knee axis instead
            // of moving freely, so bending the leg is left to hipAngle
            if (app->draggingKnee)
            {
                PointF newKnee = constrainToAxis(
                    app->robotScene.robot.innerCircle,
                    app->robotScene.robot.kneeCircle,
                    legLocalMouse,
                    MIN_LIMB_LENGTH);

                app->robotScene.robot.kneeCircle = newKnee;

                // re-anchor the shin to the offsets captured when the
                // drag started (not nudged incrementally frame by frame)
                // so its length and shape can't drift over a long drag --
                // only the thigh is getting longer or shorter here
                app->robotScene.robot.ankleCircle.x = newKnee.x + app->kneeDragAnkleOffset.x;
                app->robotScene.robot.ankleCircle.y = newKnee.y + app->kneeDragAnkleOffset.y;

                // shinArc1Angle/shinArc2Angle don't need re-anchoring
                // either -- already relative to kneeCircle, which just
                // moved to newKnee above

                // the hip<->knee distance just changed, so the thigh arcs'
                // fillet solve did too -- re-validate their existing
                // angles against it. The shin arcs are left alone
                // (knee<->ankle distance is preserved by the re-anchor
                // above), matching the thigh-only blue highlight this
                // drag already gets
                adjustThighArcs(app);
            }

            // thigh arcs: same tangent-restricted, angle-driven drag as
            // the head/butt seams above (seamArc1Angle/seamArc2Angle), but
            // reading the mouse's perpendicular-to-axis movement
            // (perpOffsetOnAxis) instead of raw Y, since the hip->knee
            // axis isn't fixed horizontal like the head-butt axis -- it
            // rotates with hipAngle and the user can pose the leg any
            // direction. thighArc1Angle (convex) stays locked to the
            // negative-delta side of ITS centerDeg, same "stay off the
            // degenerate center" safety seam arc 1/2 uses. thighArc2Angle
            // (concave) drags against a totally different, disjoint safe
            // range (see its own block below) so it doesn't need that
            // same one-sided lock. Dragging one never mirrors the other --
            // the thigh's two sides aren't meant to bulge symmetrically.
            // Nothing needs to "recenter" on a hip/knee move the way the
            // old free-point handles did: the angle is already fully
            // relative to innerCircle/kneeCircle, so the fillet solve
            // just adapts automatically every frame.
            if (app->draggingThigh1)
            {
                SafeAngleRange range = filletSafeAngleRange(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius,
                                                             app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                             MAX_THIGH_ARC_R);
                float maxDelta = range.halfWidthDeg - THIGH_ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < THIGH_ARC_SIDE_MARGIN_DEG) maxDelta = THIGH_ARC_SIDE_MARGIN_DEG;

                float perpNow = perpOffsetOnAxis(legLocalMouse, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                float raw = app->thighArcDragStartAngle + (perpNow - app->thighArcDragStartPerp) * THIGH_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                if (delta > -THIGH_ARC_SIDE_MARGIN_DEG) delta = -THIGH_ARC_SIDE_MARGIN_DEG;
                if (delta < -maxDelta) delta = -maxDelta;

                app->robotScene.robot.thighArc1Angle = range.centerDeg + delta;
            }

            // thighArc2Angle drags the same incremental, perpendicular-
            // offset way as thighArc1Angle, but against its own concave
            // safe range (filletSafeAngleRangeConcave) -- which is
            // centered on the opposite side of innerCircle, facing
            // kneeCircle. No side-lock needed here: thighArc1Angle's
            // range and this one no longer share a degenerate center to
            // stay apart from, so this just clamps symmetrically to
            // whichever side of ITS OWN center the drag reaches.
            if (app->draggingThigh2)
            {
                SafeAngleRange range = filletSafeAngleRangeConcave(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius,
                                                                    app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                                    MAX_THIGH_ARC2_CONCAVE_R);
                float maxDelta = range.halfWidthDeg - THIGH_ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < 0.0f) maxDelta = 0.0f;

                float perpNow = perpOffsetOnAxis(legLocalMouse, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                float raw = app->thighArcDragStartAngle + (perpNow - app->thighArcDragStartPerp) * THIGH_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                if (delta > maxDelta) delta = maxDelta;
                if (delta < -maxDelta) delta = -maxDelta;

                app->robotScene.robot.thighArc2Angle = range.centerDeg + delta;
            }

            // the shin sits in a frame additionally rotated by kneeAngle
            // around kneeCircle, so undo that rotation too before storing
            // the raw local coordinates for ankle/shin
            PointF shinLocalMouse = inverseRotate(legLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle);

            // same idea for the ankle circle: constrained to the
            // knee->ankle axis so dragging it only changes the shin's
            // length, not its direction
            if (app->draggingAnkle)
            {
                app->robotScene.robot.ankleCircle = constrainToAxis(
                    app->robotScene.robot.kneeCircle,
                    app->robotScene.robot.ankleCircle,
                    shinLocalMouse,
                    MIN_LIMB_LENGTH);

                // knee<->ankle distance just changed, so the shin arcs'
                // fillet solve did too -- re-validate their existing
                // angles against it
                adjustShinArcs(app);
            }

            // shin arcs: same tangent-restricted, angle-driven drag as the
            // thigh arcs above, just reading the mouse's perpendicular-to-
            // axis movement relative to the knee->ankle axis instead of
            // hip->knee (shinLocalMouse already has kneeAngle undone, same
            // way legLocalMouse has hipAngle undone for the thigh).
            // shinArc1Angle (convex) stays locked to the negative-delta
            // side of ITS centerDeg, same "stay off the degenerate center"
            // safety seam arc 1/2 and thighArc1Angle use. shinArc2Angle
            // (concave) drags against a totally different, disjoint safe
            // range (see its own block below) so it doesn't need that
            // same one-sided lock. Dragging one never mirrors the other.
            // Nothing needs recentering on a knee/ankle move -- the angle
            // is already fully relative to kneeCircle/ankleCircle, so the
            // fillet solve just adapts automatically every frame.
            if (app->draggingShin1)
            {
                SafeAngleRange range = filletSafeAngleRange(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                             app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius,
                                                             MAX_SHIN_ARC_R);
                float maxDelta = range.halfWidthDeg - SHIN_ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < SHIN_ARC_SIDE_MARGIN_DEG) maxDelta = SHIN_ARC_SIDE_MARGIN_DEG;

                float perpNow = perpOffsetOnAxis(shinLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                float raw = app->shinArcDragStartAngle + (perpNow - app->shinArcDragStartPerp) * SHIN_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                if (delta > -SHIN_ARC_SIDE_MARGIN_DEG) delta = -SHIN_ARC_SIDE_MARGIN_DEG;
                if (delta < -maxDelta) delta = -maxDelta;

                app->robotScene.robot.shinArc1Angle = range.centerDeg + delta;
            }

            // shinArc2Angle drags the same incremental, perpendicular-
            // offset way as shinArc1Angle, but against its own concave
            // safe range (filletSafeAngleRangeConcave) -- centered on the
            // opposite side of kneeCircle, facing ankleCircle. No
            // side-lock needed: shinArc1Angle's range and this one don't
            // share a degenerate center, so this just clamps symmetrically
            // to whichever side of ITS OWN center the drag reaches.
            if (app->draggingShin2)
            {
                SafeAngleRange range = filletSafeAngleRangeConcave(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                                    app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius,
                                                                    MAX_SHIN_ARC2_CONCAVE_R);
                float maxDelta = range.halfWidthDeg - SHIN_ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < 0.0f) maxDelta = 0.0f;

                float perpNow = perpOffsetOnAxis(shinLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                float raw = app->shinArcDragStartAngle + (perpNow - app->shinArcDragStartPerp) * SHIN_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                if (delta > maxDelta) delta = maxDelta;
                if (delta < -maxDelta) delta = -maxDelta;

                app->robotScene.robot.shinArc2Angle = range.centerDeg + delta;
            }
        }
        break;

        case WM_MOUSEWHEEL:
        {
            // wheel delta comes in the high word of wParam, positive = away from user
            short wheelDelta = (short)HIWORD(wParam);

            // wheel messages give screen coords, not client coords
            POINT pt;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            ScreenToClient(hwnd, &pt);

            float gx, gy;
            screenToGL(hwnd, pt.x, pt.y, &gx, &gy);

            PointF mouse = { gx, gy };

            PointF center = getCenter(app->robotScene.robot);
            PointF innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, app->robotScene.robot.angle);
            PointF kneeWorld = jointToWorld(app->robotScene.robot.kneeCircle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, app->robotScene.robot.angle);
            PointF ankleWorld = nestedJointToWorld(app->robotScene.robot.ankleCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, app->robotScene.robot.angle);
            PointF headWorld = rotatePoint((PointF){app->robotScene.robot.headX, app->robotScene.robot.y}, center, app->robotScene.robot.angle);
            PointF buttWorld = rotatePoint((PointF){app->robotScene.robot.buttX, app->robotScene.robot.y}, center, app->robotScene.robot.angle);

            float step = 2.0f;
            float radiusStep = 0.01f;

            // WM_MOUSEWHEEL packs the modifier keys held during the scroll
            // into the low word of wParam (MK_SHIFT/MK_CONTROL/etc.), same
            // as WM_MOUSEMOVE -- so no GetAsyncKeyState polling needed
            int shiftHeld = (LOWORD(wParam) & MK_SHIFT) != 0;

            if (isNear(mouse, innerWorld, HIP_HANDLE_RADIUS) && shiftHeld)
            {
                // rotate just the hip joint (and everything hanging off of
                // it -- knee, ankle, thigh/shin handles), not the whole
                // robot body. Gated behind Shift so a plain scroll while
                // hovering the (small, easy-to-clip) hip handle falls
                // through to the ordinary view-zoom behavior below instead
                // of rotating the hip by accident.
                if (wheelDelta > 0)
                    app->robotScene.robot.hipAngle += step;
                else
                    app->robotScene.robot.hipAngle -= step;
            }
            else if (isNear(mouse, innerWorld, HIP_HANDLE_RADIUS))
            {
                // plain scroll (no Shift) on the hip handle resizes the hip
                // circle itself instead of rotating -- same idea as the
                // head/butt handles below, just reachable without a
                // modifier since resizing the hip doesn't cascade into the
                // rest of the leg the way rotating it does
                if (wheelDelta > 0)
                    app->robotScene.robot.innerRadius += radiusStep;
                else
                    app->robotScene.robot.innerRadius -= radiusStep;

                if (app->robotScene.robot.innerRadius < MIN_R)
                    app->robotScene.robot.innerRadius = MIN_R;
                if (app->robotScene.robot.innerRadius > MAX_R)
                    app->robotScene.robot.innerRadius = MAX_R;

                // hip radius feeds the thigh arcs' fillet solve --
                // re-validate their existing angles against the new size
                adjustThighArcs(app);
            }
            else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS) && shiftHeld)
            {
                // rotate just the knee joint (and the shin/ankle hanging
                // off of it), leaving the hip and the rest of the body
                // untouched. Gated behind Shift for the same reason as the
                // hip above -- a plain scroll while hovering the knee
                // handle falls through to the ordinary view-zoom instead.
                if (wheelDelta > 0)
                    app->robotScene.robot.kneeAngle += step;
                else
                    app->robotScene.robot.kneeAngle -= step;
            }
            else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS))
            {
                // plain scroll (no Shift) on the knee handle resizes the
                // knee circle itself instead of rotating -- same idea as
                // the hip handle above
                if (wheelDelta > 0)
                    app->robotScene.robot.kneeRadius += radiusStep;
                else
                    app->robotScene.robot.kneeRadius -= radiusStep;

                if (app->robotScene.robot.kneeRadius < MIN_R)
                    app->robotScene.robot.kneeRadius = MIN_R;
                if (app->robotScene.robot.kneeRadius > MAX_R)
                    app->robotScene.robot.kneeRadius = MAX_R;

                // knee radius feeds both the thigh arcs' fillet solve (hip
                // <-> knee) and the shin arcs' (knee <-> ankle), so both
                // pairs get re-validated
                adjustThighArcs(app);
                adjustShinArcs(app);
            }
            else if (isNear(mouse, ankleWorld, ANKLE_HANDLE_RADIUS))
            {
                // plain scroll on the ankle/foot handle resizes it -- no
                // Shift-gated rotate branch needed here, unlike the hip and
                // knee above: the ankle is the last joint in the chain, so
                // there's nothing further down for a rotation to carry
                // along, and it can just always resize on scroll like the
                // head/butt handles below
                if (wheelDelta > 0)
                    app->robotScene.robot.ankleRadius += radiusStep;
                else
                    app->robotScene.robot.ankleRadius -= radiusStep;

                if (app->robotScene.robot.ankleRadius < MIN_R)
                    app->robotScene.robot.ankleRadius = MIN_R;
                if (app->robotScene.robot.ankleRadius > MAX_R)
                    app->robotScene.robot.ankleRadius = MAX_R;

                // ankle radius feeds the shin arcs' fillet solve --
                // re-validate their existing angles against the new size
                adjustShinArcs(app);
            }
            else if (isNear(mouse, headWorld, HEAD_BUTT_HANDLE_RADIUS))
            {
                // scroll on the head circle's own handle to resize it,
                // same range as the head radius slider
                if (wheelDelta > 0)
                    app->robotScene.robot.headRadius += radiusStep;
                else
                    app->robotScene.robot.headRadius -= radiusStep;

                if (app->robotScene.robot.headRadius < MIN_R)
                    app->robotScene.robot.headRadius = MIN_R;
                if (app->robotScene.robot.headRadius > MAX_R)
                    app->robotScene.robot.headRadius = MAX_R;

                // head radius feeds the seam arc 1/2 fillet solve --
                // re-validate their existing angles
                adjustHeadButtArcs(app);
            }
            else if (isNear(mouse, buttWorld, HEAD_BUTT_HANDLE_RADIUS))
            {
                // same idea for the butt circle's handle
                if (wheelDelta > 0)
                    app->robotScene.robot.buttRadius += radiusStep;
                else
                    app->robotScene.robot.buttRadius -= radiusStep;

                if (app->robotScene.robot.buttRadius < MIN_R)
                    app->robotScene.robot.buttRadius = MIN_R;
                if (app->robotScene.robot.buttRadius > MAX_R)
                    app->robotScene.robot.buttRadius = MAX_R;

                // butt radius feeds the same seam arc 1/2 fillet solve
                adjustHeadButtArcs(app);
            }
            else
            {
                // not over any handle -- treat the wheel as a view zoom
                // instead. Always centered on the robot's own rotation
                // pivot (not the cursor) -- see graphicsZoom's comment for
                // why: a cursor-centered version could drift the view off
                // the robot entirely, since it's drawn as thin outlines
                // rather than filled shapes.
                float factor = (wheelDelta > 0) ? ZOOM_STEP : (1.0f / ZOOM_STEP);
                graphicsZoom(factor);
            }
        }
        break;

        case WM_KEYDOWN:
        {
            if (wParam == VK_LEFT)
                app->robotScene.robot.angle += 2.0f;

            if (wParam == VK_RIGHT)
                app->robotScene.robot.angle -= 2.0f;

            if (wParam == VK_UP)
            {
                // Check if Ctrl is held for finer movement
                int ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                float step = ctrlHeld ? 0.01f : 0.05f;

                // Move entire robot including body and legs
                app->robotScene.robot.y += step;
                app->robotScene.robot.innerCircle.y += step;
                app->robotScene.robot.kneeCircle.y += step;
                app->robotScene.robot.ankleCircle.y += step;
            }

            if (wParam == VK_DOWN)
            {
                // Check if Ctrl is held for finer movement
                int ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                float step = ctrlHeld ? 0.01f : 0.05f;

                // Move entire robot including body and legs
                app->robotScene.robot.y -= step;
                app->robotScene.robot.innerCircle.y -= step;
                app->robotScene.robot.kneeCircle.y -= step;
                app->robotScene.robot.ankleCircle.y -= step;
            }
        }
        break;

        case WM_SIZE:
        {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);

            graphicsOnResize(w, h);

            RECT rect;
            GetClientRect(hwnd, &rect);

            int btnHeight = 30;
            int btnSpacing = 10;

            // Top row: Standing Position and Home Position
            int yTop = 10;
            int xStanding = 10;
            int xHome = xStanding + 120 + btnSpacing;

            // Bottom row: Save and Mirror Leg
            int yBottom = yTop + btnHeight + btnSpacing;
            int xSave = 10;
            int xMirror = xSave + 80 + btnSpacing;

            SetWindowPos(app->ui.hStandingPositionButton, NULL,
                 xStanding, yTop, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE);

            SetWindowPos(app->ui.hHomePositionButton, NULL,
                 xHome, yTop, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE);

            SetWindowPos(app->ui.hSaveButton, NULL,
                 xSave, yBottom, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE);

            SetWindowPos(app->ui.hMirrorButton, NULL,
                 xMirror, yBottom, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE);

            // bottom-left hover status label
            int hoverLabelHeight = 20;
            int hoverLabelY = rect.bottom - hoverLabelHeight - 10;

            SetWindowPos(app->ui.hHoverLabel, NULL,
                 10, hoverLabelY, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE);
        }
        break;

        case WM_CREATE:
        {
             app->ui.hStandingPositionButton = CreateWindow(
                L"BUTTON",
                L"Standing",
                WS_VISIBLE | WS_CHILD,
                10, 10, 120, 30,
                hwnd,
                (HMENU)ID_STANDING_POSITION_BUTTON,
                NULL,
                NULL
            );

             app->ui.hHomePositionButton = CreateWindow(
                L"BUTTON",
                L"Home",
                WS_VISIBLE | WS_CHILD,
                140, 10, 110, 30,
                hwnd,
                (HMENU)ID_HOME_POSITION_BUTTON,
                NULL,
                NULL
            );

             app->ui.hSaveButton = CreateWindow(
                L"BUTTON",
                L"Save",
                WS_VISIBLE | WS_CHILD,
                10, 50, 80, 30,
                hwnd,
                (HMENU)ID_SAVE_BUTTON,
                NULL,
                NULL
            );

             app->ui.hMirrorButton = CreateWindow(
                L"BUTTON",
                L"Mirror Leg",
                WS_VISIBLE | WS_CHILD,
                100, 50, 100, 30,
                hwnd,
                (HMENU)ID_MIRROR_LEG_BUTTON,
                NULL,
                NULL
            );

             // bottom-left hover status label
             app->ui.hHoverLabel = CreateWindow(
                L"STATIC",
                L"",
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                10, 560, 260, 20,
                hwnd,
                NULL,
                NULL,
                NULL
            );
        }
        break;

        // no WM_DESTROY handler existed before, so clicking the window's
        // close button destroyed the window but never told the message
        // loop to stop -- WinMain's PeekMessage loop kept spinning
        // (rendering to a now-dead HDC) and the process, and the console
        // AllocConsole opened alongside it, never exited. PostQuitMessage
        // queues WM_QUIT, which main.c's loop explicitly checks for and
        // returns 0 on, ending the process (and with it, the console).
        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case ID_SAVE_BUTTON:
                    // Save both the robot image and its mathematical equations
                    saveCanvasAsBMP("semni.bmp", app->hwndMain, app);
                    saveRobotAsEquations("semni.txt", app);
                    SetFocus(app->hwndMain);  // return focus to main window for keyboard input
                    break;

                case ID_MIRROR_LEG_BUTTON:
                    mirrorHipLeg(&app->robotScene.robot);
                    SetFocus(app->hwndMain);  // return focus for keyboard input
                    break;

                case ID_STANDING_POSITION_BUTTON:
				    initStandingPosition(app);
				    SetFocus(app->hwndMain);
				    break;

				case ID_HOME_POSITION_BUTTON:
				    initHomePosition(app);
				    SetFocus(app->hwndMain);
				    break;
            }
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
