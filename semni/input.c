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

            // seam attach handles: sit exactly at the tangent point on the
            // head circle, so this is a plain circleEdge -- no derived
            // fillet math needed just to hit-test them
            Point headLocal = { app->robotScene.robot.headX, app->robotScene.robot.y };

            Point topHandleWorld = rotatePoint(circleEdge(headLocal, app->robotScene.robot.headRadius, app->robotScene.robot.topArcAngle), center, app->robotScene.robot.angle);
            Point bottomHandleWorld = rotatePoint(circleEdge(headLocal, app->robotScene.robot.headRadius, app->robotScene.robot.bottomArcAngle), center, app->robotScene.robot.angle);

            Point innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, app->robotScene.robot.angle);

            // the leg chain (knee, thigh handles, ankle, shin handles) lives
            // in a frame that's additionally rotated by hipAngle around
            // innerCircle, independent of the whole-body angle
            Point hipPivot = app->robotScene.robot.innerCircle;
            float hipAngle = app->robotScene.robot.hipAngle;

            Point kneeWorld = jointToWorld(app->robotScene.robot.kneeCircle, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            Point thigh1World = jointToWorld(app->robotScene.robot.thighCtrl1, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            Point thigh2World = jointToWorld(app->robotScene.robot.thighCtrl2, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // the shin (ankle, shin handles) hangs off the knee, which
            // itself hangs off the hip -- two nested joint rotations
            // before it lands in world space
            Point kneePivot = app->robotScene.robot.kneeCircle;
            float kneeAngle = app->robotScene.robot.kneeAngle;

            Point ankleWorld = nestedJointToWorld(app->robotScene.robot.ankleCircle, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            Point shin1World = nestedJointToWorld(app->robotScene.robot.shinCtrl1, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            Point shin2World = nestedJointToWorld(app->robotScene.robot.shinCtrl2, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            if (isNear(mouse, topHandleWorld, ARC_HANDLE_RADIUS))
            {
                app->draggingTopArc = 1;
                app->activeHandle = 1;
            }
            else if (isNear(mouse, bottomHandleWorld, ARC_HANDLE_RADIUS))
            {
                app->draggingBottomArc = 1;
                app->activeHandle = 2;
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

                app->hipDragThigh1Offset.x = app->robotScene.robot.thighCtrl1.x - hip.x;
                app->hipDragThigh1Offset.y = app->robotScene.robot.thighCtrl1.y - hip.y;

                app->hipDragThigh2Offset.x = app->robotScene.robot.thighCtrl2.x - hip.x;
                app->hipDragThigh2Offset.y = app->robotScene.robot.thighCtrl2.y - hip.y;

                app->hipDragAnkleOffset.x = app->robotScene.robot.ankleCircle.x - hip.x;
                app->hipDragAnkleOffset.y = app->robotScene.robot.ankleCircle.y - hip.y;

                app->hipDragShinCtrl1Offset.x = app->robotScene.robot.shinCtrl1.x - hip.x;
                app->hipDragShinCtrl1Offset.y = app->robotScene.robot.shinCtrl1.y - hip.y;

                app->hipDragShinCtrl2Offset.x = app->robotScene.robot.shinCtrl2.x - hip.x;
                app->hipDragShinCtrl2Offset.y = app->robotScene.robot.shinCtrl2.y - hip.y;
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

                app->kneeDragShinCtrl1Offset.x = app->robotScene.robot.shinCtrl1.x - knee.x;
                app->kneeDragShinCtrl1Offset.y = app->robotScene.robot.shinCtrl1.y - knee.y;

                app->kneeDragShinCtrl2Offset.x = app->robotScene.robot.shinCtrl2.x - knee.x;
                app->kneeDragShinCtrl2Offset.y = app->robotScene.robot.shinCtrl2.y - knee.y;
            }
            else if (isNear(mouse, thigh1World, THIGH_HANDLE_RADIUS))
            {
                app->draggingThigh1 = 1;
                app->activeHandle = 5;
            }
            else if (isNear(mouse, thigh2World, THIGH_HANDLE_RADIUS))
            {
                app->draggingThigh2 = 1;
                app->activeHandle = 6;
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
            }
            else if (isNear(mouse, shin2World, SHIN_HANDLE_RADIUS))
            {
                app->draggingShin2 = 1;
                app->activeHandle = 9;
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

            // the seam attach angle is just the polar angle of the mouse
            // around the head circle's own center, in local space -- since
            // the handle sits exactly at circleEdge(head, headRadius,
            // angle), this tracks the cursor exactly, unlike the old
            // radius-driven handle. Clamped on TWO sides: the far side
            // (via filletSafeAngleRange) stops it from flattening into a
            // line, and the near side keeps it away from centerDeg itself
            // -- a second, different degenerate point where the arc's
            // bulge collapses flat against the head-butt axis and flips to
            // the opposite side. Each handle is locked to its own side of
            // centerDeg (top stays negative-delta, bottom stays
            // positive-delta) so they can never cross into each other's
            // territory.
            if (app->draggingTopArc)
            {
                float dx = localMouse.x - headLocal.x;
                float dy = localMouse.y - headLocal.y;
                float raw = atan2f(dy, dx) * 180.0f / 3.1415926f;
                SafeAngleRange range = filletSafeAngleRange(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, MAX_ARC_R);

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                float maxDelta = range.halfWidthDeg - ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < ARC_SIDE_MARGIN_DEG) maxDelta = ARC_SIDE_MARGIN_DEG;

                if (delta > -ARC_SIDE_MARGIN_DEG) delta = -ARC_SIDE_MARGIN_DEG;
                if (delta < -maxDelta) delta = -maxDelta;

                app->robotScene.robot.topArcAngle = range.centerDeg + delta;
            }

            if (app->draggingBottomArc)
            {
                float dx = localMouse.x - headLocal.x;
                float dy = localMouse.y - headLocal.y;
                float raw = atan2f(dy, dx) * 180.0f / 3.1415926f;
                SafeAngleRange range = filletSafeAngleRange(headLocal, app->robotScene.robot.headRadius, buttLocal, app->robotScene.robot.buttRadius, MAX_ARC_R);

                float delta = raw - range.centerDeg;
                while (delta > 180.0f) delta -= 360.0f;
                while (delta < -180.0f) delta += 360.0f;

                float maxDelta = range.halfWidthDeg - ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < ARC_SIDE_MARGIN_DEG) maxDelta = ARC_SIDE_MARGIN_DEG;

                // mirror image of the top arc's clamp -- locked to the
                // opposite (positive-delta) side of centerDeg
                if (delta < ARC_SIDE_MARGIN_DEG) delta = ARC_SIDE_MARGIN_DEG;
                if (delta > maxDelta) delta = maxDelta;

                app->robotScene.robot.bottomArcAngle = range.centerDeg + delta;
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

                app->robotScene.robot.thighCtrl1.x = newInner.x + app->hipDragThigh1Offset.x;
                app->robotScene.robot.thighCtrl1.y = newInner.y + app->hipDragThigh1Offset.y;

                app->robotScene.robot.thighCtrl2.x = newInner.x + app->hipDragThigh2Offset.x;
                app->robotScene.robot.thighCtrl2.y = newInner.y + app->hipDragThigh2Offset.y;

                app->robotScene.robot.ankleCircle.x = newInner.x + app->hipDragAnkleOffset.x;
                app->robotScene.robot.ankleCircle.y = newInner.y + app->hipDragAnkleOffset.y;

                app->robotScene.robot.shinCtrl1.x = newInner.x + app->hipDragShinCtrl1Offset.x;
                app->robotScene.robot.shinCtrl1.y = newInner.y + app->hipDragShinCtrl1Offset.y;

                app->robotScene.robot.shinCtrl2.x = newInner.x + app->hipDragShinCtrl2Offset.x;
                app->robotScene.robot.shinCtrl2.y = newInner.y + app->hipDragShinCtrl2Offset.y;

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

                app->robotScene.robot.shinCtrl1.x = newKnee.x + app->kneeDragShinCtrl1Offset.x;
                app->robotScene.robot.shinCtrl1.y = newKnee.y + app->kneeDragShinCtrl1Offset.y;

                app->robotScene.robot.shinCtrl2.x = newKnee.x + app->kneeDragShinCtrl2Offset.x;
                app->robotScene.robot.shinCtrl2.y = newKnee.y + app->kneeDragShinCtrl2Offset.y;
            }

            // the thigh's bulge handles always sit at the midpoint of the
            // hip->knee segment; dragging one only adjusts how far it
            // bulges sideways, not where it sits along the thigh
            if (app->draggingThigh1)
                app->robotScene.robot.thighCtrl1 = recenterOnAxis(
                    legLocalMouse, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);

            if (app->draggingThigh2)
                app->robotScene.robot.thighCtrl2 = recenterOnAxis(
                    legLocalMouse, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);

            // the hip or knee may have just moved (dragged directly, or
            // shifted above while resizing the thigh) -- keep the thigh
            // handles centered on the new hip->knee segment either way
            if (app->draggingInner || app->draggingKnee)
            {
                app->robotScene.robot.thighCtrl1 = recenterOnAxis(
                    app->robotScene.robot.thighCtrl1, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                app->robotScene.robot.thighCtrl2 = recenterOnAxis(
                    app->robotScene.robot.thighCtrl2, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
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

            // same centering treatment for the shin's bulge handles,
            // relative to the knee->ankle segment
            if (app->draggingShin1)
                app->robotScene.robot.shinCtrl1 = recenterOnAxis(
                    shinLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);

            if (app->draggingShin2)
                app->robotScene.robot.shinCtrl2 = recenterOnAxis(
                    shinLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);

            // if the ankle just moved (dragged directly), keep the shin
            // handles centered on the new knee->ankle segment -- a knee
            // drag doesn't need this, since the whole shin (including
            // its handles) was translated as one rigid piece above
            if (app->draggingAnkle)
            {
                app->robotScene.robot.shinCtrl1 = recenterOnAxis(
                    app->robotScene.robot.shinCtrl1, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                app->robotScene.robot.shinCtrl2 = recenterOnAxis(
                    app->robotScene.robot.shinCtrl2, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
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
