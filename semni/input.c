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
// With the fixed-attach-point construction (geometry.h's circleHalfPoint/
// axisBulgePoint), a bulge distance is valid for ANY circle size/position
// -- there's no tangency solve to blow up or flip sides, so resizing or
// moving a circle can never make an existing bulge value invalid the way
// it could under the old angle-based construction. These functions are
// kept as a clamp-only pass (re-applying the same MIN/MAX_*_BULGE bounds
// WM_MOUSEMOVE's drag code already enforces) purely so an already-valid
// pose stays valid after a save/load round trip or hand-edited init
// values -- not because anything here can actually go wrong on its own.
static float clampSigned(float value, float minAbs, float maxAbs, int positiveSide)
{
    if (positiveSide)
    {
        if (value < minAbs) value = minAbs;
        if (value > maxAbs) value = maxAbs;
    }
    else
    {
        if (value > -minAbs) value = -minAbs;
        if (value < -maxAbs) value = -maxAbs;
    }
    return value;
}

static void adjustHeadButtArcs(AppState* app)
{
    app->robotScene.robot.seamArc1Bulge = clampSigned(app->robotScene.robot.seamArc1Bulge, MIN_ARC_BULGE, MAX_ARC_BULGE, 1);
    app->robotScene.robot.seamArc2Bulge = clampSigned(app->robotScene.robot.seamArc2Bulge, MIN_ARC_BULGE, MAX_ARC_BULGE, 0);
}

static void adjustThighArcs(AppState* app)
{
    app->robotScene.robot.thighArc1Bulge = clampSigned(app->robotScene.robot.thighArc1Bulge, MIN_THIGH_ARC_BULGE, MAX_THIGH_ARC_BULGE, 0);
    app->robotScene.robot.thighArc2Bulge = clampSigned(app->robotScene.robot.thighArc2Bulge, MIN_THIGH_ARC2_BULGE, MAX_THIGH_ARC2_BULGE, 1);
}

static void adjustShinArcs(AppState* app)
{
    app->robotScene.robot.shinArc1Bulge = clampSigned(app->robotScene.robot.shinArc1Bulge, MIN_SHIN_ARC_BULGE, MAX_SHIN_ARC_BULGE, 0);
    app->robotScene.robot.shinArc2Bulge = clampSigned(app->robotScene.robot.shinArc2Bulge, MIN_SHIN_ARC2_BULGE, MAX_SHIN_ARC2_BULGE, 1);
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

            Point center = getCenter(app->robotScene.robot);

            Point mouse = app->mouseGL;

            // seam attach handles: sit exactly at the curve's bezier
            // control point (axisBulgePoint) -- same construction as
            // drawSemniHandles, so the hit-test matches exactly where the
            // handle is actually drawn
            Point headLocal = { app->robotScene.robot.headX, app->robotScene.robot.y };
            Point buttLocal = { app->robotScene.robot.buttX, app->robotScene.robot.y };

            Point seamArc1MidLocal = axisBulgePoint(headLocal, buttLocal, app->robotScene.robot.seamArc1Bulge);
            Point seamArc2MidLocal = axisBulgePoint(headLocal, buttLocal, app->robotScene.robot.seamArc2Bulge);

            Point seamArc1HandleWorld = rotatePoint(seamArc1MidLocal, center, app->robotScene.robot.angle);
            Point seamArc2HandleWorld = rotatePoint(seamArc2MidLocal, center, app->robotScene.robot.angle);

            Point innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, app->robotScene.robot.angle);

            // the leg chain (knee, thigh handles, ankle, shin handles) lives
            // in a frame that's additionally rotated by hipAngle around
            // innerCircle, independent of the whole-body angle
            Point hipPivot = app->robotScene.robot.innerCircle;
            float hipAngle = app->robotScene.robot.hipAngle;

            Point kneeWorld = jointToWorld(app->robotScene.robot.kneeCircle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // thigh arc handles: same "sit at the bezier control point"
            // construction used in renderer.c's drawThighHandles, so the
            // hit-test matches exactly where the handle is actually drawn
            Point thigh1MidLocal = axisBulgePoint(app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.thighArc1Bulge);
            Point thigh2MidLocal = axisBulgePoint(app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.thighArc2Bulge);

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

            // shin arc handles: same "sit at the bezier control point"
            // construction used for the thigh handles above (and in
            // renderer.c's drawShinHandles), just between kneeCircle and
            // ankleCircle instead of innerCircle and kneeCircle, so the
            // hit-test matches exactly where the handle is actually drawn
            Point shin1MidLocal = axisBulgePoint(app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, app->robotScene.robot.shinArc1Bulge);
            Point shin2MidLocal = axisBulgePoint(app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, app->robotScene.robot.shinArc2Bulge);

            Point shin1World = nestedJointToWorld(shin1MidLocal, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            Point shin2World = nestedJointToWorld(shin2MidLocal, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // same leg-local frame the drag math in WM_MOUSEMOVE uses --
            // computed once here so both shin handles' click-capture can
            // read the mouse's perpendicular-to-axis position (relative to
            // the knee->ankle axis) at the moment the drag starts
            Point shinLocalMouseDown = inverseRotate(legLocalMouseDown, kneePivot, kneeAngle);

            if (isNear(mouse, seamArc1HandleWorld, ARC_HANDLE_RADIUS))
            {
                app->draggingSeamArc1 = 1;
                app->activeHandle = 1;

                // remember where the drag started (mouse's perpendicular
                // offset from the head-butt axis + current bulge) so
                // WM_MOUSEMOVE can nudge the bulge incrementally from here
                // instead of solving an absolute position
                app->arcDragStartPerp = perpOffsetOnAxis(inverseRotate(mouse, center, app->robotScene.robot.angle), headLocal, buttLocal);
                app->arcDragStartBulge = app->robotScene.robot.seamArc1Bulge;
            }
            else if (isNear(mouse, seamArc2HandleWorld, ARC_HANDLE_RADIUS))
            {
                app->draggingSeamArc2 = 1;
                app->activeHandle = 2;

                app->arcDragStartPerp = perpOffsetOnAxis(inverseRotate(mouse, center, app->robotScene.robot.angle), headLocal, buttLocal);
                app->arcDragStartBulge = app->robotScene.robot.seamArc2Bulge;
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

                // thighArc1Bulge/thighArc2Bulge/shinArc1Bulge/shinArc2Bulge
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

                // shinArc1Bulge/shinArc2Bulge need no offset -- they're
                // already knee-frame-relative distances, unaffected by
                // moving kneeCircle itself
            }
            else if (isNear(mouse, thigh1World, THIGH_HANDLE_RADIUS))
            {
                app->draggingThigh1 = 1;
                app->activeHandle = 5;

                // remember where the drag started (mouse's perpendicular-
                // to-axis offset + current bulge) so WM_MOUSEMOVE can nudge
                // the bulge incrementally from here, same idea as
                // arcDragStartPerp/arcDragStartBulge for the seam arcs
                app->thighArcDragStartPerp = perpOffsetOnAxis(legLocalMouseDown, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                app->thighArcDragStartBulge = app->robotScene.robot.thighArc1Bulge;
            }
            else if (isNear(mouse, thigh2World, THIGH_HANDLE_RADIUS))
            {
                app->draggingThigh2 = 1;
                app->activeHandle = 6;

                app->thighArcDragStartPerp = perpOffsetOnAxis(legLocalMouseDown, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                app->thighArcDragStartBulge = app->robotScene.robot.thighArc2Bulge;
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
                // current bulge), same idea as thighArcDragStartPerp/Bulge
                app->shinArcDragStartPerp = perpOffsetOnAxis(shinLocalMouseDown, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                app->shinArcDragStartBulge = app->robotScene.robot.shinArc1Bulge;
            }
            else if (isNear(mouse, shin2World, SHIN_HANDLE_RADIUS))
            {
                app->draggingShin2 = 1;
                app->activeHandle = 9;

                app->shinArcDragStartPerp = perpOffsetOnAxis(shinLocalMouseDown, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                app->shinArcDragStartBulge = app->robotScene.robot.shinArc2Bulge;
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

            // bottom-left hover label: also needs the bulge/seam handle
            // positions (seam arc 1, seam arc 2, both thigh arcs, both
            // shin arcs), which the code below only computes while a drag
            // is active -- so they're worked out fresh here too, same
            // axisBulgePoint construction WM_LBUTTONDOWN's hit-test and
            // renderer.c's drawSemniHandles/drawThighHandles/
            // drawShinHandles use, so this always lines up with where each
            // handle is actually drawn. Kept in its own "Hover"-suffixed
            // locals so it can't collide with the (differently-scoped)
            // drag-only versions of the same computation further down.
            {
                Point headLocalHover = { app->robotScene.robot.headX, app->robotScene.robot.y };
                Point buttLocalHover = { app->robotScene.robot.buttX, app->robotScene.robot.y };

                Point seamArc1MidLocalHover = axisBulgePoint(headLocalHover, buttLocalHover, app->robotScene.robot.seamArc1Bulge);
                Point seamArc2MidLocalHover = axisBulgePoint(headLocalHover, buttLocalHover, app->robotScene.robot.seamArc2Bulge);

                Point seamArc1HandleWorldHover = rotatePoint(seamArc1MidLocalHover, center, angle);
                Point seamArc2HandleWorldHover = rotatePoint(seamArc2MidLocalHover, center, angle);

                Point thigh1MidLocalHover = axisBulgePoint(app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.thighArc1Bulge);
                Point thigh2MidLocalHover = axisBulgePoint(app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.thighArc2Bulge);

                Point thigh1WorldHover = jointToWorld(thigh1MidLocalHover, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
                Point thigh2WorldHover = jointToWorld(thigh2MidLocalHover, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);

                Point shin1MidLocalHover = axisBulgePoint(app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, app->robotScene.robot.shinArc1Bulge);
                Point shin2MidLocalHover = axisBulgePoint(app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle, app->robotScene.robot.shinArc2Bulge);

                Point shin1WorldHover = nestedJointToWorld(shin1MidLocalHover, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
                Point shin2WorldHover = nestedJointToWorld(shin2MidLocalHover, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);

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
                // seamArc1Bulge/seamArc2Bulge: axisBulgePoint's sign
                // convention is measured specifically against the
                // headLocal->buttLocal axis direction (headLocal passed
                // first, buttLocal second, everywhere this is computed),
                // so swapping which physical circle headX/headRadius point
                // to would silently flip that axis direction and
                // re-interpret the tuned bulge values on the opposite side
                // -- not just relabel them, which is what caused the seam
                // handle drag to look broken/inverted.
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

            Point localMouse = inverseRotate(mouse, center, angle);

            Point headLocal = { app->robotScene.robot.headX, app->robotScene.robot.y };
            Point buttLocal = { app->robotScene.robot.buttX, app->robotScene.robot.y };

            // the seam handle sits exactly at the curve's bezier control
            // point, and bulge IS the world-unit quantity being dragged --
            // no angle conversion, no tangency solve to stay clear of.
            // Dragged incrementally along the head-butt axis's
            // perpendicular direction (perpOffsetOnAxis), same "read the
            // delta, not an absolute position" approach the old angle drag
            // used, just simpler now: bulge = (bulge when the drag
            // started) + (perpendicular offset moved since then), clamped
            // to config.h's MIN/MAX_ARC_BULGE with clampSigned so each
            // handle stays on its own side (seam arc 1 positive, seam arc
            // 2 negative) instead of crossing into the other's territory.
            //
            // The two arcs are kept symmetrical: dragging either handle
            // also updates the OTHER arc's bulge to the negated value --
            // since both arcs share the same axis midpoint, the negated
            // bulge is automatically the same distance to the opposite
            // side, so no extra clamping is needed beyond the dragged
            // handle's own.
            if (app->draggingSeamArc1)
            {
                float perpNow = perpOffsetOnAxis(localMouse, headLocal, buttLocal);
                float raw = app->arcDragStartBulge + (perpNow - app->arcDragStartPerp);

                app->robotScene.robot.seamArc1Bulge = clampSigned(raw, MIN_ARC_BULGE, MAX_ARC_BULGE, 1);
                app->robotScene.robot.seamArc2Bulge = -app->robotScene.robot.seamArc1Bulge;
            }

            if (app->draggingSeamArc2)
            {
                float perpNow = perpOffsetOnAxis(localMouse, headLocal, buttLocal);
                float raw = app->arcDragStartBulge + (perpNow - app->arcDragStartPerp);

                app->robotScene.robot.seamArc2Bulge = clampSigned(raw, MIN_ARC_BULGE, MAX_ARC_BULGE, 0);
                app->robotScene.robot.seamArc1Bulge = -app->robotScene.robot.seamArc2Bulge;
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

                // thighArc1Bulge/thighArc2Bulge don't move with the hip --
                // they're already relative to it

                app->robotScene.robot.ankleCircle.x = newInner.x + app->hipDragAnkleOffset.x;
                app->robotScene.robot.ankleCircle.y = newInner.y + app->hipDragAnkleOffset.y;

                // shinArc1Bulge/shinArc2Bulge don't move with the hip
                // either -- already relative to kneeCircle, which itself
                // just got carried along via hipDragKneeOffset above

                app->robotScene.robot.innerCircle = newInner;

                // dragging the hip carries the whole leg as a rigid
                // translation, so the thigh/shin arcs' underlying
                // distances never actually change and this ends up a
                // no-op -- called anyway for consistency (moving any
                // circle re-validates its arcs), and it's cheap since
                // clampSigned leaves an already-in-range bulge alone
                adjustThighArcs(app);
                adjustShinArcs(app);
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

                // shinArc1Bulge/shinArc2Bulge don't need re-anchoring
                // either -- already relative to kneeCircle, which just
                // moved to newKnee above

                // the hip<->knee distance just changed; re-validate the
                // thigh arcs' bulge values against the new distance (a
                // no-op in practice now -- see adjustThighArcs' comment).
                // The shin arcs are left alone (knee<->ankle distance is
                // preserved by the re-anchor above), matching the
                // thigh-only blue highlight this drag already gets
                adjustThighArcs(app);
            }

            // thigh arcs: same "bulge IS the dragged quantity" drag as the
            // head/butt seams above, reading the mouse's perpendicular-to-
            // axis movement (perpOffsetOnAxis) against the hip->knee axis
            // instead of head-butt -- needed since that axis isn't fixed
            // horizontal, it rotates with hipAngle as the user poses the
            // leg. thighArc1Bulge and thighArc2Bulge drag independently
            // (unlike the seam pair, dragging one doesn't mirror the
            // other -- the thigh's two sides aren't meant to bulge
            // symmetrically), each clamped to its own sign via clampSigned
            // (config.h's MIN/MAX_THIGH_ARC_BULGE for arc 1, MIN/
            // MAX_THIGH_ARC2_BULGE for arc 2) so they can't cross sides.
            // Nothing needs to "recenter" on a hip/knee move the way the
            // old free-point handles did: bulge is already fully relative
            // to innerCircle/kneeCircle's own axis, so axisBulgePoint just
            // adapts automatically every frame.
            if (app->draggingThigh1)
            {
                float perpNow = perpOffsetOnAxis(legLocalMouse, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                float raw = app->thighArcDragStartBulge + (perpNow - app->thighArcDragStartPerp);

                app->robotScene.robot.thighArc1Bulge = clampSigned(raw, MIN_THIGH_ARC_BULGE, MAX_THIGH_ARC_BULGE, 0);
            }

            if (app->draggingThigh2)
            {
                float perpNow = perpOffsetOnAxis(legLocalMouse, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle);
                float raw = app->thighArcDragStartBulge + (perpNow - app->thighArcDragStartPerp);

                app->robotScene.robot.thighArc2Bulge = clampSigned(raw, MIN_THIGH_ARC2_BULGE, MAX_THIGH_ARC2_BULGE, 1);
            }

            // the shin sits in a frame additionally rotated by kneeAngle
            // around kneeCircle, so undo that rotation too before storing
            // the raw local coordinates for ankle/shin
            Point shinLocalMouse = inverseRotate(legLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle);

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

                // knee<->ankle distance just changed; re-validate the shin
                // arcs' bulge values against the new distance (a no-op in
                // practice now -- see adjustShinArcs' comment)
                adjustShinArcs(app);
            }

            // shin arcs: same "bulge IS the dragged quantity" drag as the
            // thigh arcs above, just reading the mouse's perpendicular-to-
            // axis movement relative to the knee->ankle axis instead of
            // hip->knee (shinLocalMouse already has kneeAngle undone, same
            // way legLocalMouse has hipAngle undone for the thigh).
            // shinArc1Bulge and shinArc2Bulge drag independently, each
            // clamped to its own sign via clampSigned (config.h's MIN/
            // MAX_SHIN_ARC_BULGE for arc 1, MIN/MAX_SHIN_ARC2_BULGE for
            // arc 2). Nothing needs recentering on a knee/ankle move --
            // bulge is already fully relative to kneeCircle/ankleCircle's
            // own axis, so axisBulgePoint just adapts automatically every
            // frame.
            if (app->draggingShin1)
            {
                float perpNow = perpOffsetOnAxis(shinLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                float raw = app->shinArcDragStartBulge + (perpNow - app->shinArcDragStartPerp);

                app->robotScene.robot.shinArc1Bulge = clampSigned(raw, MIN_SHIN_ARC_BULGE, MAX_SHIN_ARC_BULGE, 0);
            }

            if (app->draggingShin2)
            {
                float perpNow = perpOffsetOnAxis(shinLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.ankleCircle);
                float raw = app->shinArcDragStartBulge + (perpNow - app->shinArcDragStartPerp);

                app->robotScene.robot.shinArc2Bulge = clampSigned(raw, MIN_SHIN_ARC2_BULGE, MAX_SHIN_ARC2_BULGE, 1);
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
            Point ankleWorld = nestedJointToWorld(app->robotScene.robot.ankleCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, app->robotScene.robot.angle);
            Point headWorld = rotatePoint((Point){app->robotScene.robot.headX, app->robotScene.robot.y}, center, app->robotScene.robot.angle);
            Point buttWorld = rotatePoint((Point){app->robotScene.robot.buttX, app->robotScene.robot.y}, center, app->robotScene.robot.angle);

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
