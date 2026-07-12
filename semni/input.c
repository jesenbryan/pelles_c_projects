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

LRESULT handleInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, AppState* app)
{
    switch (msg)
    {
        case WM_LBUTTONDOWN:
        {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);

            screenToGL(hwnd, mx, my, &app->mouseGL.x, &app->mouseGL.y);

            app->draggingTopArc = 0;
            app->draggingBottomArc = 0;
            app->draggingInner = 0;
            app->draggingKnee = 0;
            app->draggingThigh1 = 0;
            app->draggingThigh2 = 0;
            app->draggingAnkle = 0;
            app->draggingShin1 = 0;
            app->draggingShin2 = 0;

            Point center = getCenter(app->robotScene.robot);

            Point mouse = app->mouseGL;

            // seam attach handles: pinned to the exact midpoint between
            // head and butt on X, with Y solved from the arc's actual
            // fillet circle at that exact X (circleAtX) -- same
            // construction as drawSemniHandles, so the hit-test matches
            // exactly where the handle is actually drawn
            Point headLocal = { app->robotScene.robot.headX, app->robotScene.robot.y };
            Point buttLocal = { app->robotScene.robot.buttX, app->robotScene.robot.y };
            Point bodyMidLocal = { (headLocal.x + buttLocal.x) * 0.5f, (headLocal.y + buttLocal.y) * 0.5f };

            Fillet topSeamFillet = filletFromAttachAngle(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, app->robotScene.robot.topArcAngle, MIN_ARC_R, MAX_ARC_R);
            Fillet bottomSeamFillet = filletFromAttachAngle(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, app->robotScene.robot.bottomArcAngle, MIN_ARC_R, MAX_ARC_R);

            Point topNearLocal = circleTowardPoint(topSeamFillet.center, topSeamFillet.radius, bodyMidLocal);
            Point bottomNearLocal = circleTowardPoint(bottomSeamFillet.center, bottomSeamFillet.radius, bodyMidLocal);

            Point topMidLocal = circleAtX(topSeamFillet.center, topSeamFillet.radius, bodyMidLocal.x, topNearLocal);
            Point bottomMidLocal = circleAtX(bottomSeamFillet.center, bottomSeamFillet.radius, bodyMidLocal.x, bottomNearLocal);

            Point topHandleWorld = rotatePoint(topMidLocal, center, app->robotScene.robot.angle);
            Point bottomHandleWorld = rotatePoint(bottomMidLocal, center, app->robotScene.robot.angle);

            Point innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, app->robotScene.robot.angle);

            // the leg chain (knee, thigh handles, ankle, shin handles) lives
            // in a frame that's additionally rotated by hipAngle around
            // innerCircle, independent of the whole-body angle
            Point hipPivot = app->robotScene.robot.innerCircle;
            float hipAngle = app->robotScene.robot.hipAngle;

            Point kneeWorld = jointToWorld(app->robotScene.robot.kneeCircle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // thigh arc handles: same tangent-fillet + circleAtAxisMid
            // construction used in renderer.c's drawThighHandles, so the
            // hit-test matches exactly where the handle is actually drawn
            Point thighAxisMidLocal = { (app->robotScene.robot.innerCircle.x + app->robotScene.robot.kneeCircle.x) * 0.5f,
                                         (app->robotScene.robot.innerCircle.y + app->robotScene.robot.kneeCircle.y) * 0.5f };

            Fillet thigh1Fillet = filletFromAttachAngle(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius,
                                                         app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                         app->robotScene.robot.thighArc1Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC_R);
            Point thigh1NearLocal = circleTowardPoint(thigh1Fillet.center, thigh1Fillet.radius, thighAxisMidLocal);
            Point thigh1MidLocal = circleAtAxisMid(thigh1Fillet.center, thigh1Fillet.radius, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, thigh1NearLocal);

            // thighArc2Angle uses the concave construction (bulges inward
            // instead of outward) -- see app.h's comment. circleTowardPoint
            // + circleAtAxisMid work unchanged for it (purely geometric,
            // don't care whether the fillet is internally or externally
            // tangent), only the fillet solve itself differs.
            Fillet thigh2Fillet = filletFromAttachAngleConcave(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius,
                                                                app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                                app->robotScene.robot.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
            Point thigh2NearLocal = circleTowardPoint(thigh2Fillet.center, thigh2Fillet.radius, thighAxisMidLocal);
            Point thigh2MidLocal = circleAtAxisMid(thigh2Fillet.center, thigh2Fillet.radius, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, thigh2NearLocal);

            Point thigh1World = jointToWorld(thigh1MidLocal, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            Point thigh2World = jointToWorld(thigh2MidLocal, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // same leg-local frame the drag math in WM_MOUSEMOVE uses --
            // computed once here so both thigh handles' click-capture can
            // read the mouse's perpendicular-to-axis position at the
            // moment the drag starts
            Point legLocalMouseDown = inverseRotate(inverseRotate(mouse, center, app->robotScene.robot.angle), hipPivot, hipAngle);

            // the shin (ankle, shin handles) hangs off the knee, which
            // itself hangs off the hip -- two nested joint rotations
            // before it lands in world space
            Point kneePivot = app->robotScene.robot.kneeCircle;
            float kneeAngle = app->robotScene.robot.kneeAngle;

            Point ankleWorld = nestedJointToWorld(app->robotScene.robot.ankleCircle, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // shin arc handles: same tangent-fillet + circleAtAxisMid
            // construction used for the thigh handles above (and in
            // renderer.c's drawShinHandles), just between kneeCircle and
            // ankleCircle instead of innerCircle and kneeCircle, so the
            // hit-test matches exactly where the handle is actually drawn
            Point shinAxisMidLocal = { (app->robotScene.robot.kneeCircle.x + app->robotScene.robot.ankleCircle.x) * 0.5f,
                                        (app->robotScene.robot.kneeCircle.y + app->robotScene.robot.ankleCircle.y) * 0.5f };

            Fillet shin1Fillet = filletFromAttachAngle(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                        app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius,
                                                        app->robotScene.robot.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
            Point shin1NearLocal = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, shinAxisMidLocal);
            Point shin1MidLocal = circleAtAxisMid(shin1Fillet.center, shin1Fillet.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, shin1NearLocal);

            Fillet shin2Fillet = filletFromAttachAngle(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                        app->robotScene.robot.ankleCircle, app->robotScene.robot.ankleRadius,
                                                        app->robotScene.robot.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
            Point shin2NearLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, shinAxisMidLocal);
            Point shin2MidLocal = circleAtAxisMid(shin2Fillet.center, shin2Fillet.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, shin2NearLocal);

            Point shin1World = nestedJointToWorld(shin1MidLocal, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            Point shin2World = nestedJointToWorld(shin2MidLocal, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // same leg-local frame the drag math in WM_MOUSEMOVE uses --
            // computed once here so both shin handles' click-capture can
            // read the mouse's perpendicular-to-axis position (relative to
            // the knee->ankle axis) at the moment the drag starts
            Point shinLocalMouseDown = inverseRotate(legLocalMouseDown, kneePivot, kneeAngle);

            if (isNear(mouse, topHandleWorld, ARC_HANDLE_RADIUS))
            {
                app->draggingTopArc = 1;
                app->activeHandle = 1;

                // remember where the drag started (mouse Y + current
                // angle) so WM_MOUSEMOVE can nudge the angle incrementally
                // from here instead of solving an absolute position
                app->arcDragStartMouseY = inverseRotate(mouse, center, app->robotScene.robot.angle).y;
                app->arcDragStartAngle = app->robotScene.robot.topArcAngle;
            }
            else if (isNear(mouse, bottomHandleWorld, ARC_HANDLE_RADIUS))
            {
                app->draggingBottomArc = 1;
                app->activeHandle = 2;

                app->arcDragStartMouseY = inverseRotate(mouse, center, app->robotScene.robot.angle).y;
                app->arcDragStartAngle = app->robotScene.robot.bottomArcAngle;
            }
            else if (isNear(mouse, innerWorld, HIP_HANDLE_RADIUS))
            {
                app->draggingInner = 1;
                app->activeHandle = 3;

                // remember the whole leg's offset from the hip right now,
                // so dragging the hip can carry it along as one rigid
                // piece instead of leaving it behind
                Point hip = app->robotScene.robot.innerCircle;

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
                Point knee = app->robotScene.robot.kneeCircle;

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
            app->draggingTopArc = 0;
            app->draggingBottomArc = 0;
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

            Point mouse = app->mouseGL;

            Point center = getCenter(app->robotScene.robot);
            float angle = app->robotScene.robot.angle;

            // hover state for the joint circle handles is tracked here,
            // continuously, regardless of whether anything is being
            // dragged -- that's what lets them highlight yellow just
            // from the mouse passing near them
            Point innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, angle);
            Point kneeWorld  = jointToWorld(app->robotScene.robot.kneeCircle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
            Point ankleWorld = nestedJointToWorld(app->robotScene.robot.ankleCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
            Point headWorld  = rotatePoint((Point){app->robotScene.robot.headX, app->robotScene.robot.y}, center, angle);
            Point buttWorld  = rotatePoint((Point){app->robotScene.robot.buttX, app->robotScene.robot.y}, center, angle);

            app->hoverHip   = isNear(mouse, innerWorld, HIP_HANDLE_RADIUS);
            app->hoverKnee  = isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS);
            app->hoverAnkle = isNear(mouse, ankleWorld, ANKLE_HANDLE_RADIUS);
            app->hoverHead  = isNear(mouse, headWorld, HEAD_BUTT_HANDLE_RADIUS);
            app->hoverButt  = isNear(mouse, buttWorld, HEAD_BUTT_HANDLE_RADIUS);

            if (!app->draggingTopArc && !app->draggingBottomArc &&
                !app->draggingInner &&
                !app->draggingKnee && !app->draggingThigh1 && !app->draggingThigh2 &&
                !app->draggingAnkle && !app->draggingShin1 && !app->draggingShin2)
                break;

            Point localMouse = inverseRotate(mouse, center, angle);

            Point headLocal = { app->robotScene.robot.headX, app->robotScene.robot.y };
            Point buttLocal = { app->robotScene.robot.buttX, app->robotScene.robot.y };

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
            // centerDeg (top stays negative-delta, bottom stays
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
            if (app->draggingTopArc)
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

                app->robotScene.robot.topArcAngle = range.centerDeg + delta;
                app->robotScene.robot.bottomArcAngle = range.centerDeg - delta;
            }

            if (app->draggingBottomArc)
            {
                SafeAngleRange range = filletSafeAngleRange(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, MAX_ARC_R);
                float maxDelta = range.halfWidthDeg - ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < ARC_SIDE_MARGIN_DEG) maxDelta = ARC_SIDE_MARGIN_DEG;

                float deltaY = localMouse.y - app->arcDragStartMouseY;
                float raw = app->arcDragStartAngle + deltaY * ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                // mirror image of the top arc's clamp -- locked to the
                // opposite (positive-delta) side of centerDeg
                if (delta < ARC_SIDE_MARGIN_DEG) delta = ARC_SIDE_MARGIN_DEG;
                if (delta > maxDelta) delta = maxDelta;

                app->robotScene.robot.bottomArcAngle = range.centerDeg + delta;
                app->robotScene.robot.topArcAngle = range.centerDeg - delta;
            }

            // dragging the hip carries the whole leg along as one rigid
            // piece -- each descendant point is re-applied fresh from its
            // offset (captured when the drag started) relative to the new
            // hip position, same "captured offset" pattern used below for
            // the knee drag, so the leg's shape/pose never drifts or gets
            // left behind
            if (app->draggingInner)
            {
                Point newInner = localMouse;

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
            }

            // the leg chain sits in a frame additionally rotated by hipAngle
            // around innerCircle, so undo that rotation too before storing
            // the raw local coordinates for knee/thigh/ankle/shin
            Point legLocalMouse = inverseRotate(localMouse, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle);

            // dragging the knee circle only changes the thigh's length --
            // it's constrained to slide along the hip->knee axis instead
            // of moving freely, so bending the leg is left to hipAngle
            if (app->draggingKnee)
            {
                Point newKnee = constrainToAxis(
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
            }

            // thigh arcs: same tangent-restricted, angle-driven drag as
            // the head/butt seams above (topArcAngle/bottomArcAngle), but
            // reading the mouse's perpendicular-to-axis movement
            // (perpOffsetOnAxis) instead of raw Y, since the hip->knee
            // axis isn't fixed horizontal like the head-butt axis -- it
            // rotates with hipAngle and the user can pose the leg any
            // direction. thighArc1Angle (convex) stays locked to the
            // negative-delta side of ITS centerDeg, same "stay off the
            // degenerate center" safety top/bottom uses. thighArc2Angle
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
            Point shinLocalMouse = inverseRotate(legLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle);

            // same idea for the ankle circle: constrained to the
            // knee->ankle axis so dragging it only changes the shin's
            // length, not its direction
            if (app->draggingAnkle)
                app->robotScene.robot.ankleCircle = constrainToAxis(
                    app->robotScene.robot.kneeCircle,
                    app->robotScene.robot.ankleCircle,
                    shinLocalMouse,
                    MIN_LIMB_LENGTH);

            // shin arcs: same tangent-restricted, angle-driven drag as the
            // thigh arcs above, just reading the mouse's perpendicular-to-
            // axis movement relative to the knee->ankle axis instead of
            // hip->knee (shinLocalMouse already has kneeAngle undone, same
            // way legLocalMouse has hipAngle undone for the thigh).
            // shinArc1Angle stays locked to the negative-delta side of
            // centerDeg and shinArc2Angle to the positive side, and like
            // the thigh pair, dragging one does NOT mirror the other.
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

            if (app->draggingShin2)
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

                // mirror image of shin1's clamp -- locked to the opposite
                // (positive-delta) side of centerDeg
                if (delta < SHIN_ARC_SIDE_MARGIN_DEG) delta = SHIN_ARC_SIDE_MARGIN_DEG;
                if (delta > maxDelta) delta = maxDelta;

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

            Point mouse = { gx, gy };

            Point center = getCenter(app->robotScene.robot);
            Point innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, app->robotScene.robot.angle);
            Point kneeWorld = jointToWorld(app->robotScene.robot.kneeCircle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, app->robotScene.robot.angle);
            Point headWorld = rotatePoint((Point){app->robotScene.robot.headX, app->robotScene.robot.y}, center, app->robotScene.robot.angle);
            Point buttWorld = rotatePoint((Point){app->robotScene.robot.buttX, app->robotScene.robot.y}, center, app->robotScene.robot.angle);

            float step = 2.0f;
            float radiusStep = 0.01f;

            if (isNear(mouse, innerWorld, HIP_HANDLE_RADIUS))
            {
                // rotate just the hip joint (and everything hanging off of
                // it -- knee, ankle, thigh/shin handles), not the whole
                // robot body
                if (wheelDelta > 0)
                    app->robotScene.robot.hipAngle += step;
                else
                    app->robotScene.robot.hipAngle -= step;
            }
            else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS))
            {
                // rotate just the knee joint (and the shin/ankle hanging
                // off of it), leaving the hip and the rest of the body
                // untouched
                if (wheelDelta > 0)
                    app->robotScene.robot.kneeAngle += step;
                else
                    app->robotScene.robot.kneeAngle -= step;
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

            int y = (rect.bottom / 2) - (btnHeight / 2);

            int xSave = 10;

            SetWindowPos(app->ui.hSaveButton, NULL,
                 xSave, y, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE);
        }
        break;

        case WM_CREATE:
        {
             app->ui.hSaveButton = CreateWindow(
			    L"BUTTON",           // Add L prefix
			    L"Save",             // Add L prefix
			    WS_VISIBLE | WS_CHILD,
			    10, 10, 80, 30,
			    hwnd,
			    (HMENU)ID_SAVE_BUTTON,
			    NULL,
			    NULL
			);
        }
        break;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case ID_SAVE_BUTTON:
                    // renders a handle-free frame into the back buffer and
                    // writes it out as an uncompressed 24-bit BMP -- the
                    // on-screen display (with handles) is unaffected
                    saveCanvasAsBMP("robot.bmp", app->hwndMain, app);
                    break;
            }
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
