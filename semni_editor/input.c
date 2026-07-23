#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <commctrl.h>
#include <math.h>

#include "app.h"
#include "input.h"
#include "geometry.h"
#include "config.h"

#include "robot.h"
#include "graphics.h"
#include "renderer.h"
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
    SafeAngleRange range1 = filletSafeAngleRange(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, MAX_SEMNI_THIGH_ARC_R);
    SafeAngleRange range2 = filletSafeAngleRangeConcave(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, MAX_THIGH_ARC2_CONCAVE_R);

    app->robotScene.robot.thighArc1Angle = clampToSafeAngleRange(app->robotScene.robot.thighArc1Angle, range1, THIGH_ARC_ANGLE_MARGIN_DEG);
    app->robotScene.robot.thighArc2Angle = clampToSafeAngleRange(app->robotScene.robot.thighArc2Angle, range2, THIGH_ARC_ANGLE_MARGIN_DEG);
}

static void adjustShinArcs(AppState* app)
{
    SafeAngleRange range1 = filletSafeAngleRange(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.footCircle, app->robotScene.robot.footRadius, MAX_SHIN_ARC_R);
    SafeAngleRange range2 = filletSafeAngleRangeConcave(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.footCircle, app->robotScene.robot.footRadius, MAX_SHIN_ARC2_CONCAVE_R);

    app->robotScene.robot.shinArc1Angle = clampToSafeAngleRange(app->robotScene.robot.shinArc1Angle, range1, SHIN_ARC_ANGLE_MARGIN_DEG);
    app->robotScene.robot.shinArc2Angle = clampToSafeAngleRange(app->robotScene.robot.shinArc2Angle, range2, SHIN_ARC_ANGLE_MARGIN_DEG);
}

// Same re-validation as adjustShinArcs, just for Rocky's own kneeCircle/
// footCircle pair -- needed since the knee handle's plain-scroll gesture
// (see WM_MOUSEWHEEL's ROBOT_KIND_ROCKY branch) can change kneeRadius,
// which feeds the shin arcs' fillet solve the same way it does for Semni.
static void adjustRockyShinArcs(AppState* app)
{
    SafeAngleRange range1 = filletSafeAngleRange(app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeRadius, app->robotScene.rocky.footCircle, app->robotScene.rocky.footRadius, MAX_SHIN_ARC_R);
    SafeAngleRange range2 = filletSafeAngleRangeConcave(app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeRadius, app->robotScene.rocky.footCircle, app->robotScene.rocky.footRadius, MAX_SHIN_ARC2_CONCAVE_R);

    app->robotScene.rocky.shinArc1Angle = clampToSafeAngleRange(app->robotScene.rocky.shinArc1Angle, range1, SHIN_ARC_ANGLE_MARGIN_DEG);
    app->robotScene.rocky.shinArc2Angle = clampToSafeAngleRange(app->robotScene.rocky.shinArc2Angle, range2, SHIN_ARC_ANGLE_MARGIN_DEG);
}

// Same re-validation as adjustThighArcs/adjustShinArcs, for Stilo's own
// hip1->feet1 chain -- Stilo now has no knee/shin/foot sub-stage any more
// (see app.h's Stilo comment), just the pair of thigh arcs directly between
// the hip and feet circles, needing its own copy since it's a separate
// struct/set of AppState fields.
static void adjustStiloThigh1Arcs(AppState* app)
{
    SafeAngleRange range1 = filletSafeAngleRange(app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Radius, app->robotScene.stilo.feet1Circle, app->robotScene.stilo.feet1Radius, MAX_SEMNI_THIGH_ARC_R);
    SafeAngleRange range2 = filletSafeAngleRangeConcave(app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Radius, app->robotScene.stilo.feet1Circle, app->robotScene.stilo.feet1Radius, MAX_THIGH_ARC2_CONCAVE_R);

    app->robotScene.stilo.thigh1Arc1Angle = clampToSafeAngleRange(app->robotScene.stilo.thigh1Arc1Angle, range1, THIGH_ARC_ANGLE_MARGIN_DEG);
    app->robotScene.stilo.thigh1Arc2Angle = clampToSafeAngleRange(app->robotScene.stilo.thigh1Arc2Angle, range2, THIGH_ARC_ANGLE_MARGIN_DEG);
}

// Same re-validation as adjustStiloThigh1Arcs above, for Stilo's SECOND leg
// (see app.h's Stilo comment) -- its own independent hip2->feet2 chain, so
// it needs its own copy of these checks against its own hip2/feet2/thigh2
// fields.
static void adjustStiloThigh2Arcs(AppState* app)
{
    SafeAngleRange range1 = filletSafeAngleRange(app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Radius, app->robotScene.stilo.feet2Circle, app->robotScene.stilo.feet2Radius, MAX_SEMNI_THIGH_ARC_R);
    SafeAngleRange range2 = filletSafeAngleRangeConcave(app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Radius, app->robotScene.stilo.feet2Circle, app->robotScene.stilo.feet2Radius, MAX_THIGH_ARC2_CONCAVE_R);

    app->robotScene.stilo.thigh2Arc1Angle = clampToSafeAngleRange(app->robotScene.stilo.thigh2Arc1Angle, range1, THIGH_ARC_ANGLE_MARGIN_DEG);
    app->robotScene.stilo.thigh2Arc2Angle = clampToSafeAngleRange(app->robotScene.stilo.thigh2Arc2Angle, range2, THIGH_ARC_ANGLE_MARGIN_DEG);
}

// Axis-aligned bounding box (in whichever robot's own RAW local, pre-
// rotation/pre-joint-angle fields -- same convention save.c's
// save*AsEquations functions already use) enclosing whichever robot kind
// is currently active, for updateRobotSizeLabel below. Approximates every
// part as its full circle's extent (center +/- radius, or a bare point for
// Rocky's rectangle corners via a 0 radius) rather than each part's exact
// trimmed-arc outline -- a slight overestimate, but it's what lets one
// function cover all three very differently-shaped robots instead of
// needing bespoke exact-outline math per kind. Since this reads the raw
// local fields, it reflects each robot's DESIGNED pose, not a mid-drag
// bend from kneeAngle/hipAngle -- consistent with the rest of this file's
// "raw fields, not the live rotated pose" convention, but means this
// specific readout won't visibly react to a kneeAngle/hipAngle-only drag.
static void robotBoundingBoxLocal(AppState* app, float* minX, float* maxX, float* minY, float* maxY)
{
    *minX = *minY = 1e9f;
    *maxX = *maxY = -1e9f;

#define ROBOT_BB_INCLUDE(cx, cy, rad) \
    do { \
        float _bbX = (cx); \
        float _bbY = (cy); \
        float _bbR = (rad); \
        if (_bbX - _bbR < *minX) *minX = _bbX - _bbR; \
        if (_bbX + _bbR > *maxX) *maxX = _bbX + _bbR; \
        if (_bbY - _bbR < *minY) *minY = _bbY - _bbR; \
        if (_bbY + _bbR > *maxY) *maxY = _bbY + _bbR; \
    } while (0)

    switch (app->robotScene.activeKind)
    {
        case ROBOT_KIND_SEMNI:
        {
            Semni* s = &app->robotScene.robot;
            ROBOT_BB_INCLUDE(s->headX, s->y, s->headRadius);
            ROBOT_BB_INCLUDE(s->buttX, s->y, s->buttRadius);
            ROBOT_BB_INCLUDE(s->innerCircle.x, s->innerCircle.y, s->innerRadius);
            ROBOT_BB_INCLUDE(s->kneeCircle.x, s->kneeCircle.y, s->kneeRadius);
            ROBOT_BB_INCLUDE(s->footCircle.x, s->footCircle.y, s->footRadius);
            break;
        }

        case ROBOT_KIND_ROCKY:
        {
            Rocky* r = &app->robotScene.rocky;
            ROBOT_BB_INCLUDE(r->bodyX - r->bodyHalfWidth, r->bodyY - r->bodyHalfHeight, 0.0f);
            ROBOT_BB_INCLUDE(r->bodyX + r->bodyHalfWidth, r->bodyY + r->bodyHalfHeight, 0.0f);
            ROBOT_BB_INCLUDE(r->kneeCircle.x, r->kneeCircle.y, r->kneeRadius);
            ROBOT_BB_INCLUDE(r->footCircle.x, r->footCircle.y, r->footRadius);
            break;
        }

        case ROBOT_KIND_STILO:
        {
            Stilo* st = &app->robotScene.stilo;
            ROBOT_BB_INCLUDE(st->headX, st->y, st->headRadius);
            ROBOT_BB_INCLUDE(st->buttX, st->y, st->buttRadius);
            ROBOT_BB_INCLUDE(st->hip1Circle.x, st->hip1Circle.y, st->hip1Radius);
            ROBOT_BB_INCLUDE(st->feet1Circle.x, st->feet1Circle.y, st->feet1Radius);
            ROBOT_BB_INCLUDE(st->hip2Circle.x, st->hip2Circle.y, st->hip2Radius);
            ROBOT_BB_INCLUDE(st->feet2Circle.x, st->feet2Circle.y, st->feet2Radius);
            break;
        }

        default:
            break;
    }

#undef ROBOT_BB_INCLUDE
}

// Refreshes the "Size: W x H mm" control-panel label (app.h's
// hRobotSizeLabel) from robotBoundingBoxLocal's bounding box, converted to
// millimeters via config.h's MM_PER_WORLD_UNIT. Rounded to the nearest
// whole mm and hand-split into an integer for wsprintf, same reason
// WM_HSCROLL's own Scale label splits its float by hand just below --
// wsprintf has no floating-point conversion at all.
static void updateRobotSizeLabel(AppState* app)
{
    if (!app->ui.hRobotSizeLabel)
        return;

    float minX, maxX, minY, maxY;
    robotBoundingBoxLocal(app, &minX, &maxX, &minY, &maxY);

    if (maxX < minX || maxY < minY)
    {
        SetWindowText(app->ui.hRobotSizeLabel, L"Size: - mm");
        return;
    }

    int widthMM = (int)((maxX - minX) * MM_PER_WORLD_UNIT + 0.5f);
    int heightMM = (int)((maxY - minY) * MM_PER_WORLD_UNIT + 0.5f);

    wchar_t buf[64];
    wsprintf(buf, L"Size: %d x %d mm", widthMM, heightMM);
    SetWindowText(app->ui.hRobotSizeLabel, buf);
}

// Keeps the knee handle honest whenever the rectangle's own size changes
// (scroll on the body handle, or dragging an edge -- see WM_MOUSEWHEEL/
// WM_MOUSEMOVE's ROBOT_KIND_ROCKY branches): shrinking the body can
// otherwise leave the knee handle sitting outside the new, smaller
// rectangle, which the free-drag clamp in WM_MOUSEMOVE only enforces
// WHILE actively dragging the knee itself, not when the body changes size
// out from under it. Only the handle's PIVOT POINT has to stay inside the
// rectangle -- the knee CIRCLE it draws (whatever kneeRadius happens to
// be) is free to poke outside the body's edges, so this clamps position
// only, straight to the rectangle's bounds, with no radius inset and no
// radius clamp.
static void clampRockyKneeToBody(AppState* app)
{
    Rocky* r = &app->robotScene.rocky;

    float minX = r->bodyX - r->bodyHalfWidth;
    float maxX = r->bodyX + r->bodyHalfWidth;
    float minY = r->bodyY - r->bodyHalfHeight;
    float maxY = r->bodyY + r->bodyHalfHeight;

    PointF oldKnee = r->kneeCircle;
    PointF newKnee = oldKnee;
    if (newKnee.x < minX) newKnee.x = minX;
    if (newKnee.x > maxX) newKnee.x = maxX;
    if (newKnee.y < minY) newKnee.y = minY;
    if (newKnee.y > maxY) newKnee.y = maxY;

    // carry the foot/shin along rigidly by the same delta, so pushing the
    // knee back inside the body doesn't also silently reshape the leg --
    // same "preserve the offset" idea as rockyKneeDragFootOffset
    PointF delta = { newKnee.x - oldKnee.x, newKnee.y - oldKnee.y };
    r->kneeCircle = newKnee;
    r->footCircle.x += delta.x;
    r->footCircle.y += delta.y;
}

// Hit-tests Rocky's 4 rectangle edges (left/right/top/bottom) against a
// point already given in the rectangle's OWN local frame (i.e. already
// run through inverseRotate around getRockyCenter -- see WM_LBUTTONDOWN/
// WM_MOUSEMOVE below), so this never has to know about rotation itself.
// A point counts as "on" an edge if it's within ROCKY_EDGE_HIT_TOLERANCE
// of that edge's line AND within a small slack margin of the edge's own
// span (so a corner doesn't ambiguously hit both edges that meet there --
// it just picks whichever is checked first, left/right before top/
// bottom). Returns ROCKY_EDGE_NONE if the point isn't near any edge.
static int hitTestRockyEdge(Rocky r, PointF localPoint)
{
    float left   = r.bodyX - r.bodyHalfWidth;
    float right  = r.bodyX + r.bodyHalfWidth;
    float bottom = r.bodyY - r.bodyHalfHeight;
    float top    = r.bodyY + r.bodyHalfHeight;

    float slackX = r.bodyHalfWidth * 0.15f + ROCKY_EDGE_HIT_TOLERANCE;
    float slackY = r.bodyHalfHeight * 0.15f + ROCKY_EDGE_HIT_TOLERANCE;

    BOOL withinY = (localPoint.y > bottom - slackY) && (localPoint.y < top + slackY);
    BOOL withinX = (localPoint.x > left - slackX) && (localPoint.x < right + slackX);

    if (withinY && fabsf(localPoint.x - left) < ROCKY_EDGE_HIT_TOLERANCE)
        return ROCKY_EDGE_LEFT;
    if (withinY && fabsf(localPoint.x - right) < ROCKY_EDGE_HIT_TOLERANCE)
        return ROCKY_EDGE_RIGHT;
    if (withinX && fabsf(localPoint.y - bottom) < ROCKY_EDGE_HIT_TOLERANCE)
        return ROCKY_EDGE_BOTTOM;
    if (withinX && fabsf(localPoint.y - top) < ROCKY_EDGE_HIT_TOLERANCE)
        return ROCKY_EDGE_TOP;

    return ROCKY_EDGE_NONE;
}

// NEW: middle-mouse drag-pan state, mirroring canvas.c's ArcSpline
// panning/panLastX/panLastY -- gives the Semni robot editor the same
// pan gesture instead of only supporting zoom.
static BOOL semniPanning = FALSE;
static int  semniPanLastX = 0, semniPanLastY = 0;

// ---------------- SEMNI CONTROL PANEL LOOK & FEEL ----------------
// The Semni buttons used to be plain WS_CHILD controls floating directly
// over the OpenGL viewport with the default tiny system font and no
// backdrop, which read as unfinished next to the ArcSpline panel's own
// solid-backed floating panel (ui.c's SemniUIPanel). This gives the
// robot editor the same treatment: a solid backdrop behind the cluster
// and a consistent "Segoe UI" font (already the app's chosen typeface --
// see canvas.c's GL text font) instead of the stock GUI font.
static HFONT g_semniUIFont = NULL;
static HFONT g_semniUITitleFont = NULL;
static BOOL  g_semniPanelClassRegistered = FALSE;

static void ensureSemniUIFonts(void)
{
    if (g_semniUIFont) return;

    g_semniUIFont = CreateFont(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_semniUITitleFont = CreateFont(
        -15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

// Registers the flat, solid-backed "backdrop" window class used for both
// hControlPanel (behind the button cluster) and hHoverPanel (behind the
// bottom-left hover label). COLOR_BTNFACE matches the ArcSpline panel's
// own background (ui.c's createUIWindow) so both editor modes share the
// same panel color. WS_EX_STATICEDGE (applied at CreateWindowEx time,
// not here) gives it a thin flat frame instead of the dated sunken-3D
// look of WS_EX_CLIENTEDGE.
static void ensureSemniPanelClassRegistered(void)
{
    if (g_semniPanelClassRegistered) return;

    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"SemniControlPanel";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    g_semniPanelClassRegistered = TRUE;
}

LRESULT handleInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, AppState* app)
{
    switch (msg)
    {
        case WM_MBUTTONDOWN:
        {
            semniPanning = TRUE;
            semniPanLastX = LOWORD(lParam);
            semniPanLastY = HIWORD(lParam);
            SetCapture(hwnd);
            return 0;
        }

        case WM_MBUTTONUP:
        {
            semniPanning = FALSE;
            ReleaseCapture();
            return 0;
        }
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
            app->draggingFoot = 0;
            app->draggingShin1 = 0;
            app->draggingShin2 = 0;
            app->draggingRockyBody = 0;
            app->draggingRockyEdge = ROCKY_EDGE_NONE;
            app->draggingRockyKnee = 0;
            app->draggingRockyFoot = 0;
            app->draggingRockyShin1 = 0;
            app->draggingRockyShin2 = 0;
            app->draggingStiloSeamArc1 = 0;
            app->draggingStiloSeamArc2 = 0;
            app->draggingStiloHip1 = 0;
            app->draggingStiloFeet1 = 0;
            app->draggingStiloThigh1Arc1 = 0;
            app->draggingStiloThigh1Arc2 = 0;
            app->draggingStiloHip2 = 0;
            app->draggingStiloFeet2 = 0;
            app->draggingStiloThigh2Arc1 = 0;
            app->draggingStiloThigh2Arc2 = 0;

            // Rocky's rectangular torso has one hip-like handle (see
            // app.h's hoverRockyBody/draggingRockyBody): hover to
            // highlight, drag to move the whole robot, plain scroll (see
            // WM_MOUSEWHEEL) to resize. It also has 4 edge midpoints
            // (hoverRockyEdge/draggingRockyEdge, see ROCKY_EDGE_* in
            // app.h) -- hover/drag one of those to stretch just that
            // dimension instead of moving or uniformly scaling. It has one
            // knee handle (hoverRockyKnee/draggingRockyKnee) where the leg
            // attaches to the rectangle -- drag it freely anywhere inside
            // the rectangle (clamped so the knee circle never pokes
            // outside the body, see WM_MOUSEMOVE), plain scroll to resize
            // kneeRadius, Shift+scroll to bend kneeAngle. And it has one
            // foot handle (hoverRockyFoot/draggingRockyFoot) at the far
            // end of the shin -- drag it along the knee->foot axis
            // (constrainToAxis, same as Semni's own foot handle) to
            // change the leg's length. Stilo (below) now has Semni's FULL
            // set of joint handles instead.
            if (app->robotScene.activeKind == ROBOT_KIND_ROCKY)
            {
                PointF rockyCenter = getRockyCenter(app->robotScene.rocky);
                PointF kneeWorld = rotatePoint(app->robotScene.rocky.kneeCircle, rockyCenter, app->robotScene.rocky.angle);
                PointF footWorld = jointToWorld(app->robotScene.rocky.footCircle, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle, rockyCenter, app->robotScene.rocky.angle);

                // Shin connector-arc handle positions -- same
                // circleAtAxisMid construction as renderer.c's drawRocky
                // (where these handles are actually drawn), so the hit-test
                // always lines up with what's on screen.
                PointF rockyShinAxisMidLocal = { (app->robotScene.rocky.kneeCircle.x + app->robotScene.rocky.footCircle.x) * 0.5f,
                                                  (app->robotScene.rocky.kneeCircle.y + app->robotScene.rocky.footCircle.y) * 0.5f };

                Fillet rockyShin1Fillet = filletFromAttachAngle(app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeRadius,
                                                                 app->robotScene.rocky.footCircle, app->robotScene.rocky.footRadius,
                                                                 app->robotScene.rocky.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
                PointF rockyShin1NearLocal = circleTowardPoint(rockyShin1Fillet.center, rockyShin1Fillet.radius, rockyShinAxisMidLocal);
                PointF rockyShin1MidLocal = circleAtAxisMid(rockyShin1Fillet.center, rockyShin1Fillet.radius, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.footCircle, rockyShin1NearLocal);

                Fillet rockyShin2Fillet = filletFromAttachAngleConcave(app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeRadius,
                                                                        app->robotScene.rocky.footCircle, app->robotScene.rocky.footRadius,
                                                                        app->robotScene.rocky.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
                PointF rockyShin2NearLocal = circleTowardPoint(rockyShin2Fillet.center, rockyShin2Fillet.radius, rockyShinAxisMidLocal);
                PointF rockyShin2MidLocal = circleAtAxisMid(rockyShin2Fillet.center, rockyShin2Fillet.radius, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.footCircle, rockyShin2NearLocal);

                PointF rockyShin1World = jointToWorld(rockyShin1MidLocal, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle, rockyCenter, app->robotScene.rocky.angle);
                PointF rockyShin2World = jointToWorld(rockyShin2MidLocal, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle, rockyCenter, app->robotScene.rocky.angle);

                // shin-local frame: undo the whole-body angle first (same
                // as the "localMouse" WM_MOUSEMOVE computes and reuses for
                // this exact purpose), then kneeAngle on top of that --
                // Rocky only has these 2 nested rotations (no separate hip
                // stage the way Semni's own legLocalMouseDown/
                // shinLocalMouseDown chain has to undo 3), but skipping
                // the body-angle step here would only happen to work while
                // rocky.angle is 0 and silently break as soon as the whole
                // body is rotated. Needed so the drag-start perpendicular-
                // offset capture reads the mouse in the same frame
                // WM_MOUSEMOVE's own update math (rockyShinLocalMouse) uses.
                PointF rockyBodyLocalMouseDown = inverseRotate(app->mouseGL, rockyCenter, app->robotScene.rocky.angle);
                PointF rockyShinLocalMouseDown = inverseRotate(rockyBodyLocalMouseDown, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle);

                if (isNear(app->mouseGL, rockyCenter, HIP_HANDLE_RADIUS))
                {
                    app->draggingRockyBody = 1;

                    // capture the leg's fixed offset from the body right
                    // now, so the drag can carry it along as one rigid
                    // piece -- same pattern as Semni's own
                    // hipDragKneeOffset/hipDragFootOffset
                    app->rockyDragKneeOffset.x = app->robotScene.rocky.kneeCircle.x - app->robotScene.rocky.bodyX;
                    app->rockyDragKneeOffset.y = app->robotScene.rocky.kneeCircle.y - app->robotScene.rocky.bodyY;

                    app->rockyDragFootOffset.x = app->robotScene.rocky.footCircle.x - app->robotScene.rocky.bodyX;
                    app->rockyDragFootOffset.y = app->robotScene.rocky.footCircle.y - app->robotScene.rocky.bodyY;
                }
                else if (isNear(app->mouseGL, kneeWorld, KNEE_HANDLE_RADIUS))
                {
                    app->draggingRockyKnee = 1;

                    // capture the shin's fixed offset from the knee right
                    // now, so dragging the knee around inside the
                    // rectangle carries the foot/shin along as one rigid
                    // piece -- same pattern as Semni's own
                    // kneeDragFootOffset
                    app->rockyKneeDragFootOffset.x = app->robotScene.rocky.footCircle.x - app->robotScene.rocky.kneeCircle.x;
                    app->rockyKneeDragFootOffset.y = app->robotScene.rocky.footCircle.y - app->robotScene.rocky.kneeCircle.y;
                }
                else if (isNear(app->mouseGL, footWorld, FOOT_HANDLE_RADIUS))
                {
                    // no offset capture needed -- nothing hangs off the
                    // foot, same as Semni's own draggingFoot
                    app->draggingRockyFoot = 1;
                }
                else if (isNear(app->mouseGL, rockyShin1World, SHIN_HANDLE_RADIUS))
                {
                    app->draggingRockyShin1 = 1;

                    // remember where the drag started (mouse's
                    // perpendicular-to-axis offset, relative to the knee->
                    // foot axis + the current angle), same idea as Semni's
                    // own shinArcDragStartPerp/shinArcDragStartAngle
                    app->rockyShinArcDragStartPerp = perpOffsetOnAxis(rockyShinLocalMouseDown, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.footCircle);
                    app->rockyShinArcDragStartAngle = app->robotScene.rocky.shinArc1Angle;
                }
                else if (isNear(app->mouseGL, rockyShin2World, SHIN_HANDLE_RADIUS))
                {
                    app->draggingRockyShin2 = 1;

                    app->rockyShinArcDragStartPerp = perpOffsetOnAxis(rockyShinLocalMouseDown, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.footCircle);
                    app->rockyShinArcDragStartAngle = app->robotScene.rocky.shinArc2Angle;
                }
                else
                {
                    // Not on the move-handle, knee handle, foot handle, or
                    // either shin handle -- check the 4 edge midpoints
                    // instead, in the rectangle's own local (unrotated)
                    // frame, same as hitTestRockyEdge expects.
                    PointF localMouseDown = inverseRotate(app->mouseGL, rockyCenter, app->robotScene.rocky.angle);
                    app->draggingRockyEdge = hitTestRockyEdge(app->robotScene.rocky, localMouseDown);
                }
                break;
            }

            // Stilo now has just two joints per leg (hip + feet, no
            // knee/shin/foot sub-stage any more -- see app.h's Stilo
            // comment) -- same hit-test construction as the ROBOT_KIND_SEMNI
            // block below, just reading/writing app->robotScene.stilo and
            // the Stilo-specific draggingStilo*/stilo* fields (see app.h)
            // instead of Semni's own, since a different struct is being
            // edited.
            if (app->robotScene.activeKind == ROBOT_KIND_STILO)
            {
                PointF stiloCenter = getStiloCenter(app->robotScene.stilo);
                PointF stiloMouse = app->mouseGL;

                PointF stiloHeadLocal = { app->robotScene.stilo.headX, app->robotScene.stilo.y };
                PointF stiloButtLocal = { app->robotScene.stilo.buttX, app->robotScene.stilo.y };
                PointF stiloBodyMidLocal = { (stiloHeadLocal.x + stiloButtLocal.x) * 0.5f, (stiloHeadLocal.y + stiloButtLocal.y) * 0.5f };

                Fillet stiloSeamArc1Fillet = filletFromAttachAngle(stiloHeadLocal, app->robotScene.stilo.headRadius, stiloButtLocal, app->robotScene.stilo.buttRadius, app->robotScene.stilo.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
                Fillet stiloSeamArc2Fillet = filletFromAttachAngle(stiloHeadLocal, app->robotScene.stilo.headRadius, stiloButtLocal, app->robotScene.stilo.buttRadius, app->robotScene.stilo.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);

                PointF stiloSeamArc1NearLocal = circleTowardPoint(stiloSeamArc1Fillet.center, stiloSeamArc1Fillet.radius, stiloBodyMidLocal);
                PointF stiloSeamArc2NearLocal = circleTowardPoint(stiloSeamArc2Fillet.center, stiloSeamArc2Fillet.radius, stiloBodyMidLocal);

                PointF stiloSeamArc1MidLocal = circleAtX(stiloSeamArc1Fillet.center, stiloSeamArc1Fillet.radius, stiloBodyMidLocal.x, stiloSeamArc1NearLocal);
                PointF stiloSeamArc2MidLocal = circleAtX(stiloSeamArc2Fillet.center, stiloSeamArc2Fillet.radius, stiloBodyMidLocal.x, stiloSeamArc2NearLocal);

                PointF stiloSeamArc1HandleWorld = rotatePoint(stiloSeamArc1MidLocal, stiloCenter, app->robotScene.stilo.angle);
                PointF stiloSeamArc2HandleWorld = rotatePoint(stiloSeamArc2MidLocal, stiloCenter, app->robotScene.stilo.angle);

                PointF stiloHip1World = rotatePoint(app->robotScene.stilo.hip1Circle, stiloCenter, app->robotScene.stilo.angle);

                PointF stiloHip1Pivot = app->robotScene.stilo.hip1Circle;
                float stiloHip1Angle = app->robotScene.stilo.hip1Angle;

                PointF stiloFeet1World = jointToWorld(app->robotScene.stilo.feet1Circle, stiloHip1Pivot, stiloHip1Angle, stiloCenter, app->robotScene.stilo.angle);

                PointF stiloThigh1AxisMidLocal = { (app->robotScene.stilo.hip1Circle.x + app->robotScene.stilo.feet1Circle.x) * 0.5f,
                                                    (app->robotScene.stilo.hip1Circle.y + app->robotScene.stilo.feet1Circle.y) * 0.5f };

                Fillet stiloThigh1Arc1Fillet = filletFromAttachAngle(app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Radius,
                                                                      app->robotScene.stilo.feet1Circle, app->robotScene.stilo.feet1Radius,
                                                                      app->robotScene.stilo.thigh1Arc1Angle, MIN_THIGH_ARC_R, MAX_SEMNI_THIGH_ARC_R);
                PointF stiloThigh1Arc1NearLocal = circleTowardPoint(stiloThigh1Arc1Fillet.center, stiloThigh1Arc1Fillet.radius, stiloThigh1AxisMidLocal);
                PointF stiloThigh1Arc1MidLocal = circleAtAxisMid(stiloThigh1Arc1Fillet.center, stiloThigh1Arc1Fillet.radius, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.feet1Circle, stiloThigh1Arc1NearLocal);

                Fillet stiloThigh1Arc2Fillet = filletFromAttachAngleConcave(app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Radius,
                                                                             app->robotScene.stilo.feet1Circle, app->robotScene.stilo.feet1Radius,
                                                                             app->robotScene.stilo.thigh1Arc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
                PointF stiloThigh1Arc2NearLocal = circleTowardPoint(stiloThigh1Arc2Fillet.center, stiloThigh1Arc2Fillet.radius, stiloThigh1AxisMidLocal);
                PointF stiloThigh1Arc2MidLocal = circleAtAxisMid(stiloThigh1Arc2Fillet.center, stiloThigh1Arc2Fillet.radius, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.feet1Circle, stiloThigh1Arc2NearLocal);

                PointF stiloThigh1Arc1World = jointToWorld(stiloThigh1Arc1MidLocal, stiloHip1Pivot, stiloHip1Angle, stiloCenter, app->robotScene.stilo.angle);
                PointF stiloThigh1Arc2World = jointToWorld(stiloThigh1Arc2MidLocal, stiloHip1Pivot, stiloHip1Angle, stiloCenter, app->robotScene.stilo.angle);

                PointF stiloLeg1LocalMouseDown = inverseRotate(inverseRotate(stiloMouse, stiloCenter, app->robotScene.stilo.angle), stiloHip1Pivot, stiloHip1Angle);

                // ---- leg 2 hit-test -- same construction as leg 1 above,
                // just reading app->robotScene.stilo's hip2/feet2/thigh2
                // fields (see app.h's Stilo comment). No seam arc handles
                // here -- those are torso-level, shared with leg 1, already
                // hit-tested above.
                PointF stiloHip2Pivot = app->robotScene.stilo.hip2Circle;
                float stiloHip2Angle = app->robotScene.stilo.hip2Angle;

                PointF stiloHip2World = rotatePoint(stiloHip2Pivot, stiloCenter, app->robotScene.stilo.angle);
                PointF stiloFeet2World = jointToWorld(app->robotScene.stilo.feet2Circle, stiloHip2Pivot, stiloHip2Angle, stiloCenter, app->robotScene.stilo.angle);

                PointF stiloThigh2AxisMidLocal = { (app->robotScene.stilo.hip2Circle.x + app->robotScene.stilo.feet2Circle.x) * 0.5f,
                                                    (app->robotScene.stilo.hip2Circle.y + app->robotScene.stilo.feet2Circle.y) * 0.5f };

                Fillet stiloThigh2Arc1Fillet = filletFromAttachAngle(app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Radius,
                                                                      app->robotScene.stilo.feet2Circle, app->robotScene.stilo.feet2Radius,
                                                                      app->robotScene.stilo.thigh2Arc1Angle, MIN_THIGH_ARC_R, MAX_SEMNI_THIGH_ARC_R);
                PointF stiloThigh2Arc1NearLocal = circleTowardPoint(stiloThigh2Arc1Fillet.center, stiloThigh2Arc1Fillet.radius, stiloThigh2AxisMidLocal);
                PointF stiloThigh2Arc1MidLocal = circleAtAxisMid(stiloThigh2Arc1Fillet.center, stiloThigh2Arc1Fillet.radius, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.feet2Circle, stiloThigh2Arc1NearLocal);

                Fillet stiloThigh2Arc2Fillet = filletFromAttachAngleConcave(app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Radius,
                                                                             app->robotScene.stilo.feet2Circle, app->robotScene.stilo.feet2Radius,
                                                                             app->robotScene.stilo.thigh2Arc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
                PointF stiloThigh2Arc2NearLocal = circleTowardPoint(stiloThigh2Arc2Fillet.center, stiloThigh2Arc2Fillet.radius, stiloThigh2AxisMidLocal);
                PointF stiloThigh2Arc2MidLocal = circleAtAxisMid(stiloThigh2Arc2Fillet.center, stiloThigh2Arc2Fillet.radius, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.feet2Circle, stiloThigh2Arc2NearLocal);

                PointF stiloThigh2Arc1World = jointToWorld(stiloThigh2Arc1MidLocal, stiloHip2Pivot, stiloHip2Angle, stiloCenter, app->robotScene.stilo.angle);
                PointF stiloThigh2Arc2World = jointToWorld(stiloThigh2Arc2MidLocal, stiloHip2Pivot, stiloHip2Angle, stiloCenter, app->robotScene.stilo.angle);

                PointF stiloLeg2LocalMouseDown = inverseRotate(inverseRotate(stiloMouse, stiloCenter, app->robotScene.stilo.angle), stiloHip2Pivot, stiloHip2Angle);

                if (isNear(stiloMouse, stiloSeamArc1HandleWorld, ARC_HANDLE_RADIUS))
                {
                    app->draggingStiloSeamArc1 = 1;
                    app->stiloArcDragStartMouseY = inverseRotate(stiloMouse, stiloCenter, app->robotScene.stilo.angle).y;
                    app->stiloArcDragStartAngle = app->robotScene.stilo.seamArc1Angle;
                }
                else if (isNear(stiloMouse, stiloSeamArc2HandleWorld, ARC_HANDLE_RADIUS))
                {
                    app->draggingStiloSeamArc2 = 1;
                    app->stiloArcDragStartMouseY = inverseRotate(stiloMouse, stiloCenter, app->robotScene.stilo.angle).y;
                    app->stiloArcDragStartAngle = app->robotScene.stilo.seamArc2Angle;
                }
                else if (isNear(stiloMouse, stiloHip1World, HIP_HANDLE_RADIUS))
                {
                    app->draggingStiloHip1 = 1;

                    PointF stiloHip1 = app->robotScene.stilo.hip1Circle;

                    app->stiloHip1DragFeetOffset.x = app->robotScene.stilo.feet1Circle.x - stiloHip1.x;
                    app->stiloHip1DragFeetOffset.y = app->robotScene.stilo.feet1Circle.y - stiloHip1.y;
                }
                else if (isNear(stiloMouse, stiloFeet1World, KNEE_HANDLE_RADIUS))
                {
                    app->draggingStiloFeet1 = 1;
                }
                else if (isNear(stiloMouse, stiloThigh1Arc1World, THIGH_HANDLE_RADIUS))
                {
                    app->draggingStiloThigh1Arc1 = 1;
                    app->stiloThigh1ArcDragStartPerp = perpOffsetOnAxis(stiloLeg1LocalMouseDown, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.feet1Circle);
                    app->stiloThigh1ArcDragStartAngle = app->robotScene.stilo.thigh1Arc1Angle;
                }
                else if (isNear(stiloMouse, stiloThigh1Arc2World, THIGH_HANDLE_RADIUS))
                {
                    app->draggingStiloThigh1Arc2 = 1;
                    app->stiloThigh1ArcDragStartPerp = perpOffsetOnAxis(stiloLeg1LocalMouseDown, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.feet1Circle);
                    app->stiloThigh1ArcDragStartAngle = app->robotScene.stilo.thigh1Arc2Angle;
                }
                else if (isNear(stiloMouse, stiloHip2World, HIP_HANDLE_RADIUS))
                {
                    app->draggingStiloHip2 = 1;

                    PointF stiloHip2 = app->robotScene.stilo.hip2Circle;

                    app->stiloHip2DragFeetOffset.x = app->robotScene.stilo.feet2Circle.x - stiloHip2.x;
                    app->stiloHip2DragFeetOffset.y = app->robotScene.stilo.feet2Circle.y - stiloHip2.y;
                }
                else if (isNear(stiloMouse, stiloFeet2World, KNEE_HANDLE_RADIUS))
                {
                    app->draggingStiloFeet2 = 1;
                }
                else if (isNear(stiloMouse, stiloThigh2Arc1World, THIGH_HANDLE_RADIUS))
                {
                    app->draggingStiloThigh2Arc1 = 1;
                    app->stiloThigh2ArcDragStartPerp = perpOffsetOnAxis(stiloLeg2LocalMouseDown, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.feet2Circle);
                    app->stiloThigh2ArcDragStartAngle = app->robotScene.stilo.thigh2Arc1Angle;
                }
                else if (isNear(stiloMouse, stiloThigh2Arc2World, THIGH_HANDLE_RADIUS))
                {
                    app->draggingStiloThigh2Arc2 = 1;
                    app->stiloThigh2ArcDragStartPerp = perpOffsetOnAxis(stiloLeg2LocalMouseDown, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.feet2Circle);
                    app->stiloThigh2ArcDragStartAngle = app->robotScene.stilo.thigh2Arc2Angle;
                }

                break;
            }

            // Per-joint hand-dragging beyond Rocky's one body handle and
            // Stilo's full handle set above only exists for Semni here --
            // this guard just stops the code below (which assumes
            // app->robotScene.robot) from running for either of them.
            if (app->robotScene.activeKind != ROBOT_KIND_SEMNI)
                break;

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

            // the leg chain (knee, thigh handles, foot, shin handles) lives
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
                                                         app->robotScene.robot.thighArc1Angle, MIN_THIGH_ARC_R, MAX_SEMNI_THIGH_ARC_R);
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

            // the shin (foot, shin handles) hangs off the knee, which
            // itself hangs off the hip -- two nested joint rotations
            // before it lands in world space
            PointF kneePivot = app->robotScene.robot.kneeCircle;
            float kneeAngle = app->robotScene.robot.kneeAngle;

            PointF footWorld = nestedJointToWorld(app->robotScene.robot.footCircle, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // shin arc handles: same tangent-fillet + circleAtAxisMid
            // construction used for the thigh handles above (and in
            // renderer.c's drawShinHandles), just between kneeCircle and
            // footCircle instead of innerCircle and kneeCircle, so the
            // hit-test matches exactly where the handle is actually drawn
            PointF shinAxisMidLocal = { (app->robotScene.robot.kneeCircle.x + app->robotScene.robot.footCircle.x) * 0.5f,
                                        (app->robotScene.robot.kneeCircle.y + app->robotScene.robot.footCircle.y) * 0.5f };

            Fillet shin1Fillet = filletFromAttachAngle(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                        app->robotScene.robot.footCircle, app->robotScene.robot.footRadius,
                                                        app->robotScene.robot.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
            PointF shin1NearLocal = circleTowardPoint(shin1Fillet.center, shin1Fillet.radius, shinAxisMidLocal);
            PointF shin1MidLocal = circleAtAxisMid(shin1Fillet.center, shin1Fillet.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.footCircle, shin1NearLocal);

            // shinArc2Angle uses the concave construction (bulges inward
            // instead of outward -- see app.h's comment), same as
            // thighArc2Angle
            Fillet shin2Fillet = filletFromAttachAngleConcave(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                               app->robotScene.robot.footCircle, app->robotScene.robot.footRadius,
                                                               app->robotScene.robot.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
            PointF shin2NearLocal = circleTowardPoint(shin2Fillet.center, shin2Fillet.radius, shinAxisMidLocal);
            PointF shin2MidLocal = circleAtAxisMid(shin2Fillet.center, shin2Fillet.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.footCircle, shin2NearLocal);

            PointF shin1World = nestedJointToWorld(shin1MidLocal, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);
            PointF shin2World = nestedJointToWorld(shin2MidLocal, kneePivot, kneeAngle, hipPivot, hipAngle, center, app->robotScene.robot.angle);

            // same leg-local frame the drag math in WM_MOUSEMOVE uses --
            // computed once here so both shin handles' click-capture can
            // read the mouse's perpendicular-to-axis position (relative to
            // the knee->foot axis) at the moment the drag starts
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

                app->hipDragFootOffset.x = app->robotScene.robot.footCircle.x - hip.x;
                app->hipDragFootOffset.y = app->robotScene.robot.footCircle.y - hip.y;
            }
            else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS))
            {
                app->draggingKnee = 1;
                app->activeHandle = 4;

                // remember the shin's offset from the knee right now, so
                // it can be pinned to this exact shape/length for the
                // whole drag instead of drifting frame by frame
                PointF knee = app->robotScene.robot.kneeCircle;

                app->kneeDragFootOffset.x = app->robotScene.robot.footCircle.x - knee.x;
                app->kneeDragFootOffset.y = app->robotScene.robot.footCircle.y - knee.y;

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
            else if (isNear(mouse, footWorld, FOOT_HANDLE_RADIUS))
            {
                app->draggingFoot = 1;
                app->activeHandle = 7;
            }
            else if (isNear(mouse, shin1World, SHIN_HANDLE_RADIUS))
            {
                app->draggingShin1 = 1;
                app->activeHandle = 8;

                // remember where the drag started (mouse's perpendicular-
                // to-axis offset, relative to the knee->foot axis + the
                // current angle), same idea as thighArcDragStartPerp/Angle
                app->shinArcDragStartPerp = perpOffsetOnAxis(shinLocalMouseDown, app->robotScene.robot.kneeCircle, app->robotScene.robot.footCircle);
                app->shinArcDragStartAngle = app->robotScene.robot.shinArc1Angle;
            }
            else if (isNear(mouse, shin2World, SHIN_HANDLE_RADIUS))
            {
                app->draggingShin2 = 1;
                app->activeHandle = 9;

                app->shinArcDragStartPerp = perpOffsetOnAxis(shinLocalMouseDown, app->robotScene.robot.kneeCircle, app->robotScene.robot.footCircle);
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
            app->draggingFoot = 0;
            app->draggingShin1 = 0;
            app->draggingShin2 = 0;
            app->draggingRockyBody = 0;
            app->draggingRockyEdge = ROCKY_EDGE_NONE;
            app->draggingRockyKnee = 0;
            app->draggingRockyFoot = 0;
            app->draggingRockyShin1 = 0;
            app->draggingRockyShin2 = 0;
            app->draggingStiloSeamArc1 = 0;
            app->draggingStiloSeamArc2 = 0;
            app->draggingStiloHip1 = 0;
            app->draggingStiloFeet1 = 0;
            app->draggingStiloThigh1Arc1 = 0;
            app->draggingStiloThigh1Arc2 = 0;
            app->draggingStiloHip2 = 0;
            app->draggingStiloFeet2 = 0;
            app->draggingStiloThigh2Arc1 = 0;
            app->draggingStiloThigh2Arc2 = 0;
            app->activeHandle = 0;

            // Whatever handle was just released, the pose may have
            // changed -- refresh the mm size readout. See
            // updateRobotSizeLabel's own comment for why this only
            // updates on release rather than live during the drag.
            updateRobotSizeLabel(app);
        }
        break;

        case WM_MOUSEMOVE:
        {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);

            if (semniPanning)
            {
                int dx = mx - semniPanLastX;
                int dy = my - semniPanLastY;

                graphicsPan(dx, dy);

                semniPanLastX = mx;
                semniPanLastY = my;

                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            screenToGL(hwnd, mx, my, &app->mouseGL.x, &app->mouseGL.y);

            // Rocky's body handle: hover highlight + drag-to-move, same
            // "capture the leg's offset once, re-apply every move" pattern
            // as Semni's own hip drag (see WM_LBUTTONDOWN's draggingInner
            // handling) -- just simpler, since Rocky's handle IS its own
            // rotation pivot (getRockyCenter), so there's no separate
            // fixed torso frame the way Semni's head/butt-defined center
            // is for its hip.
            if (app->robotScene.activeKind == ROBOT_KIND_ROCKY)
            {
                PointF rockyCenter = getRockyCenter(app->robotScene.rocky);

                // Same local frame hitTestRockyEdge expects -- computed
                // once here so it can feed both the hover check below and
                // the resize-drag branch further down.
                PointF localMouse = inverseRotate(app->mouseGL, rockyCenter, app->robotScene.rocky.angle);

                PointF kneeWorld = rotatePoint(app->robotScene.rocky.kneeCircle, rockyCenter, app->robotScene.rocky.angle);
                PointF footWorld = jointToWorld(app->robotScene.rocky.footCircle, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle, rockyCenter, app->robotScene.rocky.angle);

                app->hoverRockyBody = isNear(app->mouseGL, rockyCenter, HIP_HANDLE_RADIUS);
                app->hoverRockyKnee = !app->hoverRockyBody && isNear(app->mouseGL, kneeWorld, KNEE_HANDLE_RADIUS);
                app->hoverRockyFoot = !app->hoverRockyBody && !app->hoverRockyKnee && isNear(app->mouseGL, footWorld, FOOT_HANDLE_RADIUS);
                app->hoverRockyEdge = (app->hoverRockyBody || app->hoverRockyKnee || app->hoverRockyFoot) ? ROCKY_EDGE_NONE : hitTestRockyEdge(app->robotScene.rocky, localMouse);

                // Shin connector-arc handle positions, for the hover label
                // below and the drag-update math further down -- same
                // circleAtAxisMid construction as renderer.c's drawRocky
                // and this same WM_MOUSEMOVE's own WM_LBUTTONDOWN hit-test.
                // No stored hoverRockyShin1/2 flag, same as Semni's own
                // shin handles -- just a direct isNear check for the label.
                PointF rockyShinAxisMidLocal = { (app->robotScene.rocky.kneeCircle.x + app->robotScene.rocky.footCircle.x) * 0.5f,
                                                  (app->robotScene.rocky.kneeCircle.y + app->robotScene.rocky.footCircle.y) * 0.5f };

                Fillet rockyShin1Fillet = filletFromAttachAngle(app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeRadius,
                                                                 app->robotScene.rocky.footCircle, app->robotScene.rocky.footRadius,
                                                                 app->robotScene.rocky.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
                PointF rockyShin1NearLocal = circleTowardPoint(rockyShin1Fillet.center, rockyShin1Fillet.radius, rockyShinAxisMidLocal);
                PointF rockyShin1MidLocal = circleAtAxisMid(rockyShin1Fillet.center, rockyShin1Fillet.radius, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.footCircle, rockyShin1NearLocal);

                Fillet rockyShin2Fillet = filletFromAttachAngleConcave(app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeRadius,
                                                                        app->robotScene.rocky.footCircle, app->robotScene.rocky.footRadius,
                                                                        app->robotScene.rocky.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
                PointF rockyShin2NearLocal = circleTowardPoint(rockyShin2Fillet.center, rockyShin2Fillet.radius, rockyShinAxisMidLocal);
                PointF rockyShin2MidLocal = circleAtAxisMid(rockyShin2Fillet.center, rockyShin2Fillet.radius, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.footCircle, rockyShin2NearLocal);

                PointF rockyShin1World = jointToWorld(rockyShin1MidLocal, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle, rockyCenter, app->robotScene.rocky.angle);
                PointF rockyShin2World = jointToWorld(rockyShin2MidLocal, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle, rockyCenter, app->robotScene.rocky.angle);

                const wchar_t* rockyHoverLabel = L"";
                if (app->hoverRockyBody)
                    rockyHoverLabel = L"Body";
                else if (app->hoverRockyKnee)
                    rockyHoverLabel = L"Knee";
                else if (app->hoverRockyFoot)
                    rockyHoverLabel = L"Foot";
                else if (isNear(app->mouseGL, rockyShin1World, SHIN_HANDLE_RADIUS))
                    rockyHoverLabel = L"Shin Arc 1";
                else if (isNear(app->mouseGL, rockyShin2World, SHIN_HANDLE_RADIUS))
                    rockyHoverLabel = L"Shin Arc 2";
                else if (app->hoverRockyEdge == ROCKY_EDGE_LEFT || app->hoverRockyEdge == ROCKY_EDGE_RIGHT)
                    rockyHoverLabel = L"Body Width";
                else if (app->hoverRockyEdge == ROCKY_EDGE_TOP || app->hoverRockyEdge == ROCKY_EDGE_BOTTOM)
                    rockyHoverLabel = L"Body Height";
                SetWindowText(app->ui.hHoverLabel, rockyHoverLabel);

                if (app->draggingRockyBody)
                {
                    app->robotScene.rocky.kneeCircle.x = localMouse.x + app->rockyDragKneeOffset.x;
                    app->robotScene.rocky.kneeCircle.y = localMouse.y + app->rockyDragKneeOffset.y;

                    app->robotScene.rocky.footCircle.x = localMouse.x + app->rockyDragFootOffset.x;
                    app->robotScene.rocky.footCircle.y = localMouse.y + app->rockyDragFootOffset.y;

                    app->robotScene.rocky.bodyX = localMouse.x;
                    app->robotScene.rocky.bodyY = localMouse.y;
                }
                else if (app->draggingRockyKnee)
                {
                    // Free to move anywhere in the rectangle (not
                    // constrained to an axis the way Semni's hip->knee
                    // drag is), clamped so the handle's PIVOT POINT never
                    // leaves the body -- straight to the rectangle's own
                    // bounds, no radius inset. The knee CIRCLE itself
                    // (drawn at whatever kneeRadius happens to be) is free
                    // to stick out past the edges; only the draggable dot
                    // has to stay inside.
                    float minX = app->robotScene.rocky.bodyX - app->robotScene.rocky.bodyHalfWidth;
                    float maxX = app->robotScene.rocky.bodyX + app->robotScene.rocky.bodyHalfWidth;
                    float minY = app->robotScene.rocky.bodyY - app->robotScene.rocky.bodyHalfHeight;
                    float maxY = app->robotScene.rocky.bodyY + app->robotScene.rocky.bodyHalfHeight;

                    float newKneeX = localMouse.x;
                    if (newKneeX < minX) newKneeX = minX;
                    if (newKneeX > maxX) newKneeX = maxX;

                    float newKneeY = localMouse.y;
                    if (newKneeY < minY) newKneeY = minY;
                    if (newKneeY > maxY) newKneeY = maxY;

                    PointF newKnee = { newKneeX, newKneeY };
                    app->robotScene.rocky.kneeCircle = newKnee;

                    // re-anchor the shin to the offset captured when the
                    // drag started, so its length/shape can't drift over a
                    // long drag -- only the knee's own position within the
                    // body is changing here
                    app->robotScene.rocky.footCircle.x = newKnee.x + app->rockyKneeDragFootOffset.x;
                    app->robotScene.rocky.footCircle.y = newKnee.y + app->rockyKneeDragFootOffset.y;
                }
                else if (app->draggingRockyFoot)
                {
                    // shin-local frame: undo kneeAngle around kneeCircle,
                    // same as Semni's own shinLocalMouse -- constrainToAxis
                    // then keeps the foot sliding along the knee->foot
                    // axis so only the leg's length changes, not its
                    // bend/direction (that's kneeAngle's job).
                    PointF shinLocalMouse = inverseRotate(localMouse, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle);

                    app->robotScene.rocky.footCircle = constrainToAxis(
                        app->robotScene.rocky.kneeCircle,
                        app->robotScene.rocky.footCircle,
                        shinLocalMouse,
                        MIN_LIMB_LENGTH);

                    // knee<->foot distance just changed, so the shin arcs'
                    // fillet solve did too -- re-validate their existing
                    // angles against it, same as Semni's own adjustShinArcs
                    adjustRockyShinArcs(app);
                }
                else if (app->draggingRockyShin1)
                {
                    // Same incremental, perpendicular-offset drag math as
                    // Semni's own draggingShin1 (see this file's
                    // ROBOT_KIND_SEMNI block) -- reads the mouse's
                    // perpendicular movement relative to the knee->foot
                    // axis (in the shin-local frame, kneeAngle undone) and
                    // nudges shinArc1Angle from wherever the drag started,
                    // clamped to the convex fillet's own safe range so it
                    // can't flatten into a line or flip to the other side.
                    PointF rockyShinLocalMouse = inverseRotate(localMouse, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle);

                    SafeAngleRange range = filletSafeAngleRange(app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeRadius,
                                                                 app->robotScene.rocky.footCircle, app->robotScene.rocky.footRadius,
                                                                 MAX_SHIN_ARC_R);
                    float maxDelta = range.halfWidthDeg - SHIN_ARC_ANGLE_MARGIN_DEG;
                    if (maxDelta < SHIN_ARC_SIDE_MARGIN_DEG) maxDelta = SHIN_ARC_SIDE_MARGIN_DEG;

                    float perpNow = perpOffsetOnAxis(rockyShinLocalMouse, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.footCircle);
                    float raw = app->rockyShinArcDragStartAngle + (perpNow - app->rockyShinArcDragStartPerp) * SHIN_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                    float delta = raw - range.centerDeg;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    if (delta > -SHIN_ARC_SIDE_MARGIN_DEG) delta = -SHIN_ARC_SIDE_MARGIN_DEG;
                    if (delta < -maxDelta) delta = -maxDelta;

                    app->robotScene.rocky.shinArc1Angle = range.centerDeg + delta;
                }
                else if (app->draggingRockyShin2)
                {
                    // Concave counterpart, same as Semni's own
                    // draggingShin2 -- its own disjoint safe range
                    // (filletSafeAngleRangeConcave), no one-sided lock
                    // needed since it doesn't share shinArc1Angle's
                    // degenerate center.
                    PointF rockyShinLocalMouse = inverseRotate(localMouse, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle);

                    SafeAngleRange range = filletSafeAngleRangeConcave(app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeRadius,
                                                                        app->robotScene.rocky.footCircle, app->robotScene.rocky.footRadius,
                                                                        MAX_SHIN_ARC2_CONCAVE_R);
                    float maxDelta = range.halfWidthDeg - SHIN_ARC_ANGLE_MARGIN_DEG;
                    if (maxDelta < 0.0f) maxDelta = 0.0f;

                    float perpNow = perpOffsetOnAxis(rockyShinLocalMouse, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.footCircle);
                    float raw = app->rockyShinArcDragStartAngle + (perpNow - app->rockyShinArcDragStartPerp) * SHIN_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                    float delta = raw - range.centerDeg;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    if (delta > maxDelta) delta = maxDelta;
                    if (delta < -maxDelta) delta = -maxDelta;

                    app->robotScene.rocky.shinArc2Angle = range.centerDeg + delta;
                }
                else if (app->draggingRockyEdge == ROCKY_EDGE_LEFT || app->draggingRockyEdge == ROCKY_EDGE_RIGHT)
                {
                    // Dragging a side edge stretches the half-width only --
                    // the opposite edge (and the whole rectangle's center/
                    // leg) stays put, same as dragging a window border.
                    float newHalfWidth = fabsf(localMouse.x - app->robotScene.rocky.bodyX);
                    if (newHalfWidth < MIN_ROCKY_BODY_HALF) newHalfWidth = MIN_ROCKY_BODY_HALF;
                    if (newHalfWidth > MAX_ROCKY_BODY_HALF) newHalfWidth = MAX_ROCKY_BODY_HALF;
                    app->robotScene.rocky.bodyHalfWidth = newHalfWidth;

                    // the rectangle may have just shrunk -- keep the knee
                    // (and the foot/shin hanging off it) from ending up
                    // outside the new bounds
                    clampRockyKneeToBody(app);
                }
                else if (app->draggingRockyEdge == ROCKY_EDGE_TOP || app->draggingRockyEdge == ROCKY_EDGE_BOTTOM)
                {
                    float newHalfHeight = fabsf(localMouse.y - app->robotScene.rocky.bodyY);
                    if (newHalfHeight < MIN_ROCKY_BODY_HALF) newHalfHeight = MIN_ROCKY_BODY_HALF;
                    if (newHalfHeight > MAX_ROCKY_BODY_HALF) newHalfHeight = MAX_ROCKY_BODY_HALF;
                    app->robotScene.rocky.bodyHalfHeight = newHalfHeight;

                    // same reasoning as the width edges above
                    clampRockyKneeToBody(app);
                }
                break;
            }

            // Stilo now has just two joints per leg (hip + feet) worth of
            // hover/drag feedback -- same construction as the
            // ROBOT_KIND_SEMNI block below, operating on
            // app->robotScene.stilo and the Stilo-specific hoverStilo*/
            // draggingStilo*/stilo* fields instead of Semni's own. Stilo
            // has no View Segments overlay (see drawStilo's comment), so
            // hoveredCircleSegment/hoveredBodyCircle are just blanked here,
            // same as the old "not Semni" fallback used to do for it.
            if (app->robotScene.activeKind == ROBOT_KIND_STILO)
            {
                app->hoveredCircleSegment = -1;
                app->hoveredBodyCircle = -1;

                PointF stiloMouse = app->mouseGL;

                PointF stiloCenter = getStiloCenter(app->robotScene.stilo);
                float stiloAngle = app->robotScene.stilo.angle;

                PointF stiloHip1World  = rotatePoint(app->robotScene.stilo.hip1Circle, stiloCenter, stiloAngle);
                PointF stiloFeet1World = jointToWorld(app->robotScene.stilo.feet1Circle, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Angle, stiloCenter, stiloAngle);
                PointF stiloHeadWorld  = rotatePoint((PointF){app->robotScene.stilo.headX, app->robotScene.stilo.y}, stiloCenter, stiloAngle);
                PointF stiloButtWorld  = rotatePoint((PointF){app->robotScene.stilo.buttX, app->robotScene.stilo.y}, stiloCenter, stiloAngle);

                app->hoverStiloHip1  = isNear(stiloMouse, stiloHip1World, HIP_HANDLE_RADIUS);
                app->hoverStiloFeet1 = isNear(stiloMouse, stiloFeet1World, KNEE_HANDLE_RADIUS);
                app->hoverStiloHead = isNear(stiloMouse, stiloHeadWorld, HEAD_BUTT_HANDLE_RADIUS);
                app->hoverStiloButt = isNear(stiloMouse, stiloButtWorld, HEAD_BUTT_HANDLE_RADIUS);

                // leg 2's own hip/feet hover -- same construction as leg 1's
                // above, against app->robotScene.stilo's hip2/feet2 fields.
                // No leg-2 equivalent of hoverStiloHead/hoverStiloButt --
                // those are torso-level, shared with leg 1.
                PointF stiloHip2World  = rotatePoint(app->robotScene.stilo.hip2Circle, stiloCenter, stiloAngle);
                PointF stiloFeet2World = jointToWorld(app->robotScene.stilo.feet2Circle, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Angle, stiloCenter, stiloAngle);

                app->hoverStiloHip2  = isNear(stiloMouse, stiloHip2World, HIP_HANDLE_RADIUS);
                app->hoverStiloFeet2 = isNear(stiloMouse, stiloFeet2World, KNEE_HANDLE_RADIUS);

                // hover label -- same construction/priority order as
                // Semni's own hover label block below
                {
                    PointF stiloHeadLocalHover = { app->robotScene.stilo.headX, app->robotScene.stilo.y };
                    PointF stiloButtLocalHover = { app->robotScene.stilo.buttX, app->robotScene.stilo.y };
                    PointF stiloBodyMidLocalHover = { (stiloHeadLocalHover.x + stiloButtLocalHover.x) * 0.5f, (stiloHeadLocalHover.y + stiloButtLocalHover.y) * 0.5f };

                    Fillet stiloSeamArc1FilletHover = filletFromAttachAngle(stiloHeadLocalHover, app->robotScene.stilo.headRadius, stiloButtLocalHover, app->robotScene.stilo.buttRadius, app->robotScene.stilo.seamArc1Angle, MIN_ARC_R, MAX_ARC_R);
                    Fillet stiloSeamArc2FilletHover = filletFromAttachAngle(stiloHeadLocalHover, app->robotScene.stilo.headRadius, stiloButtLocalHover, app->robotScene.stilo.buttRadius, app->robotScene.stilo.seamArc2Angle, MIN_ARC_R, MAX_ARC_R);

                    PointF stiloSeamArc1NearLocalHover = circleTowardPoint(stiloSeamArc1FilletHover.center, stiloSeamArc1FilletHover.radius, stiloBodyMidLocalHover);
                    PointF stiloSeamArc2NearLocalHover = circleTowardPoint(stiloSeamArc2FilletHover.center, stiloSeamArc2FilletHover.radius, stiloBodyMidLocalHover);

                    PointF stiloSeamArc1MidLocalHover = circleAtX(stiloSeamArc1FilletHover.center, stiloSeamArc1FilletHover.radius, stiloBodyMidLocalHover.x, stiloSeamArc1NearLocalHover);
                    PointF stiloSeamArc2MidLocalHover = circleAtX(stiloSeamArc2FilletHover.center, stiloSeamArc2FilletHover.radius, stiloBodyMidLocalHover.x, stiloSeamArc2NearLocalHover);

                    PointF stiloSeamArc1HandleWorldHover = rotatePoint(stiloSeamArc1MidLocalHover, stiloCenter, stiloAngle);
                    PointF stiloSeamArc2HandleWorldHover = rotatePoint(stiloSeamArc2MidLocalHover, stiloCenter, stiloAngle);

                    PointF stiloThigh1AxisMidLocalHover = { (app->robotScene.stilo.hip1Circle.x + app->robotScene.stilo.feet1Circle.x) * 0.5f,
                                                             (app->robotScene.stilo.hip1Circle.y + app->robotScene.stilo.feet1Circle.y) * 0.5f };

                    Fillet stiloThigh1Arc1FilletHover = filletFromAttachAngle(app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Radius, app->robotScene.stilo.feet1Circle, app->robotScene.stilo.feet1Radius, app->robotScene.stilo.thigh1Arc1Angle, MIN_THIGH_ARC_R, MAX_SEMNI_THIGH_ARC_R);
                    PointF stiloThigh1Arc1NearLocalHover = circleTowardPoint(stiloThigh1Arc1FilletHover.center, stiloThigh1Arc1FilletHover.radius, stiloThigh1AxisMidLocalHover);
                    PointF stiloThigh1Arc1MidLocalHover = circleAtAxisMid(stiloThigh1Arc1FilletHover.center, stiloThigh1Arc1FilletHover.radius, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.feet1Circle, stiloThigh1Arc1NearLocalHover);

                    Fillet stiloThigh1Arc2FilletHover = filletFromAttachAngleConcave(app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Radius, app->robotScene.stilo.feet1Circle, app->robotScene.stilo.feet1Radius, app->robotScene.stilo.thigh1Arc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
                    PointF stiloThigh1Arc2NearLocalHover = circleTowardPoint(stiloThigh1Arc2FilletHover.center, stiloThigh1Arc2FilletHover.radius, stiloThigh1AxisMidLocalHover);
                    PointF stiloThigh1Arc2MidLocalHover = circleAtAxisMid(stiloThigh1Arc2FilletHover.center, stiloThigh1Arc2FilletHover.radius, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.feet1Circle, stiloThigh1Arc2NearLocalHover);

                    PointF stiloThigh1Arc1WorldHover = jointToWorld(stiloThigh1Arc1MidLocalHover, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Angle, stiloCenter, stiloAngle);
                    PointF stiloThigh1Arc2WorldHover = jointToWorld(stiloThigh1Arc2MidLocalHover, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Angle, stiloCenter, stiloAngle);

                    // leg 2's own thigh arc hover -- same construction as
                    // leg 1's above, against the hip2/feet2/thigh2 fields
                    PointF stiloThigh2AxisMidLocalHover = { (app->robotScene.stilo.hip2Circle.x + app->robotScene.stilo.feet2Circle.x) * 0.5f,
                                                             (app->robotScene.stilo.hip2Circle.y + app->robotScene.stilo.feet2Circle.y) * 0.5f };

                    Fillet stiloThigh2Arc1FilletHover = filletFromAttachAngle(app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Radius, app->robotScene.stilo.feet2Circle, app->robotScene.stilo.feet2Radius, app->robotScene.stilo.thigh2Arc1Angle, MIN_THIGH_ARC_R, MAX_SEMNI_THIGH_ARC_R);
                    PointF stiloThigh2Arc1NearLocalHover = circleTowardPoint(stiloThigh2Arc1FilletHover.center, stiloThigh2Arc1FilletHover.radius, stiloThigh2AxisMidLocalHover);
                    PointF stiloThigh2Arc1MidLocalHover = circleAtAxisMid(stiloThigh2Arc1FilletHover.center, stiloThigh2Arc1FilletHover.radius, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.feet2Circle, stiloThigh2Arc1NearLocalHover);

                    Fillet stiloThigh2Arc2FilletHover = filletFromAttachAngleConcave(app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Radius, app->robotScene.stilo.feet2Circle, app->robotScene.stilo.feet2Radius, app->robotScene.stilo.thigh2Arc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
                    PointF stiloThigh2Arc2NearLocalHover = circleTowardPoint(stiloThigh2Arc2FilletHover.center, stiloThigh2Arc2FilletHover.radius, stiloThigh2AxisMidLocalHover);
                    PointF stiloThigh2Arc2MidLocalHover = circleAtAxisMid(stiloThigh2Arc2FilletHover.center, stiloThigh2Arc2FilletHover.radius, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.feet2Circle, stiloThigh2Arc2NearLocalHover);

                    PointF stiloThigh2Arc1WorldHover = jointToWorld(stiloThigh2Arc1MidLocalHover, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Angle, stiloCenter, stiloAngle);
                    PointF stiloThigh2Arc2WorldHover = jointToWorld(stiloThigh2Arc2MidLocalHover, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Angle, stiloCenter, stiloAngle);

                    const wchar_t* stiloHoverLabel = L"";

                    if (isNear(stiloMouse, stiloSeamArc1HandleWorldHover, ARC_HANDLE_RADIUS))
                        stiloHoverLabel = L"Seam Arc 1";
                    else if (isNear(stiloMouse, stiloSeamArc2HandleWorldHover, ARC_HANDLE_RADIUS))
                        stiloHoverLabel = L"Seam Arc 2";
                    else if (app->hoverStiloHip1)
                        stiloHoverLabel = L"Hip 1";
                    else if (isNear(stiloMouse, stiloThigh1Arc1WorldHover, THIGH_HANDLE_RADIUS))
                        stiloHoverLabel = L"Thigh 1 Arc 1";
                    else if (isNear(stiloMouse, stiloThigh1Arc2WorldHover, THIGH_HANDLE_RADIUS))
                        stiloHoverLabel = L"Thigh 1 Arc 2";
                    else if (app->hoverStiloFeet1)
                        stiloHoverLabel = L"Feet 1";
                    else if (app->hoverStiloHip2)
                        stiloHoverLabel = L"Hip 2";
                    else if (isNear(stiloMouse, stiloThigh2Arc1WorldHover, THIGH_HANDLE_RADIUS))
                        stiloHoverLabel = L"Thigh 2 Arc 1";
                    else if (isNear(stiloMouse, stiloThigh2Arc2WorldHover, THIGH_HANDLE_RADIUS))
                        stiloHoverLabel = L"Thigh 2 Arc 2";
                    else if (app->hoverStiloFeet2)
                        stiloHoverLabel = L"Feet 2";
                    else if (app->hoverStiloHead)
                        stiloHoverLabel = L"Butt";
                    else if (app->hoverStiloButt)
                        stiloHoverLabel = L"Head";

                    SetWindowText(app->ui.hHoverLabel, stiloHoverLabel);
                }

                if (!app->draggingStiloSeamArc1 && !app->draggingStiloSeamArc2 &&
                    !app->draggingStiloHip1 && !app->draggingStiloFeet1 &&
                    !app->draggingStiloThigh1Arc1 && !app->draggingStiloThigh1Arc2 &&
                    !app->draggingStiloHip2 && !app->draggingStiloFeet2 &&
                    !app->draggingStiloThigh2Arc1 && !app->draggingStiloThigh2Arc2)
                    break;

                PointF stiloLocalMouse = inverseRotate(stiloMouse, stiloCenter, stiloAngle);

                PointF stiloHeadLocal = { app->robotScene.stilo.headX, app->robotScene.stilo.y };
                PointF stiloButtLocal = { app->robotScene.stilo.buttX, app->robotScene.stilo.y };

                if (app->draggingStiloSeamArc1)
                {
                    SafeAngleRange range = filletSafeAngleRange(stiloHeadLocal, app->robotScene.stilo.headRadius, stiloButtLocal, app->robotScene.stilo.buttRadius, MAX_ARC_R);
                    float maxDelta = range.halfWidthDeg - ARC_ANGLE_MARGIN_DEG;
                    if (maxDelta < ARC_SIDE_MARGIN_DEG) maxDelta = ARC_SIDE_MARGIN_DEG;

                    float deltaY = stiloLocalMouse.y - app->stiloArcDragStartMouseY;
                    float raw = app->stiloArcDragStartAngle + deltaY * ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                    float delta = raw - range.centerDeg;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    if (delta > -ARC_SIDE_MARGIN_DEG) delta = -ARC_SIDE_MARGIN_DEG;
                    if (delta < -maxDelta) delta = -maxDelta;

                    app->robotScene.stilo.seamArc1Angle = range.centerDeg + delta;
                    app->robotScene.stilo.seamArc2Angle = range.centerDeg - delta;
                }

                if (app->draggingStiloSeamArc2)
                {
                    SafeAngleRange range = filletSafeAngleRange(stiloHeadLocal, app->robotScene.stilo.headRadius, stiloButtLocal, app->robotScene.stilo.buttRadius, MAX_ARC_R);
                    float maxDelta = range.halfWidthDeg - ARC_ANGLE_MARGIN_DEG;
                    if (maxDelta < ARC_SIDE_MARGIN_DEG) maxDelta = ARC_SIDE_MARGIN_DEG;

                    float deltaY = stiloLocalMouse.y - app->stiloArcDragStartMouseY;
                    float raw = app->stiloArcDragStartAngle + deltaY * ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                    float delta = raw - range.centerDeg;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    if (delta < ARC_SIDE_MARGIN_DEG) delta = ARC_SIDE_MARGIN_DEG;
                    if (delta > maxDelta) delta = maxDelta;

                    app->robotScene.stilo.seamArc2Angle = range.centerDeg + delta;
                    app->robotScene.stilo.seamArc1Angle = range.centerDeg - delta;
                }

                if (app->draggingStiloHip1)
                {
                    PointF newHip1 = stiloLocalMouse;

                    app->robotScene.stilo.feet1Circle.x = newHip1.x + app->stiloHip1DragFeetOffset.x;
                    app->robotScene.stilo.feet1Circle.y = newHip1.y + app->stiloHip1DragFeetOffset.y;

                    app->robotScene.stilo.hip1Circle = newHip1;

                    adjustStiloThigh1Arcs(app);
                }

                PointF stiloLeg1LocalMouse = inverseRotate(stiloLocalMouse, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Angle);

                if (app->draggingStiloFeet1)
                {
                    app->robotScene.stilo.feet1Circle = constrainToAxis(
                        app->robotScene.stilo.hip1Circle,
                        app->robotScene.stilo.feet1Circle,
                        stiloLeg1LocalMouse,
                        MIN_LIMB_LENGTH);

                    adjustStiloThigh1Arcs(app);
                }

                if (app->draggingStiloThigh1Arc1)
                {
                    SafeAngleRange range = filletSafeAngleRange(app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Radius,
                                                                 app->robotScene.stilo.feet1Circle, app->robotScene.stilo.feet1Radius,
                                                                 MAX_SEMNI_THIGH_ARC_R);
                    float maxDelta = range.halfWidthDeg - THIGH_ARC_ANGLE_MARGIN_DEG;
                    if (maxDelta < THIGH_ARC_SIDE_MARGIN_DEG) maxDelta = THIGH_ARC_SIDE_MARGIN_DEG;

                    float perpNow = perpOffsetOnAxis(stiloLeg1LocalMouse, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.feet1Circle);
                    float raw = app->stiloThigh1ArcDragStartAngle + (perpNow - app->stiloThigh1ArcDragStartPerp) * THIGH_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                    float delta = raw - range.centerDeg;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    if (delta > -THIGH_ARC_SIDE_MARGIN_DEG) delta = -THIGH_ARC_SIDE_MARGIN_DEG;
                    if (delta < -maxDelta) delta = -maxDelta;

                    app->robotScene.stilo.thigh1Arc1Angle = range.centerDeg + delta;
                }

                if (app->draggingStiloThigh1Arc2)
                {
                    SafeAngleRange range = filletSafeAngleRangeConcave(app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Radius,
                                                                        app->robotScene.stilo.feet1Circle, app->robotScene.stilo.feet1Radius,
                                                                        MAX_THIGH_ARC2_CONCAVE_R);
                    float maxDelta = range.halfWidthDeg - THIGH_ARC_ANGLE_MARGIN_DEG;
                    if (maxDelta < 0.0f) maxDelta = 0.0f;

                    float perpNow = perpOffsetOnAxis(stiloLeg1LocalMouse, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.feet1Circle);
                    float raw = app->stiloThigh1ArcDragStartAngle + (perpNow - app->stiloThigh1ArcDragStartPerp) * THIGH_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                    float delta = raw - range.centerDeg;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    if (delta > maxDelta) delta = maxDelta;
                    if (delta < -maxDelta) delta = -maxDelta;

                    app->robotScene.stilo.thigh1Arc2Angle = range.centerDeg + delta;
                }

                // ---- leg 2 drag updates -- same construction as leg 1's
                // above, against the hip2/feet2/thigh2 fields. No seam-arc
                // dragging here -- that's torso-level, shared with leg 1,
                // already handled above.
                if (app->draggingStiloHip2)
                {
                    PointF newHip2 = stiloLocalMouse;

                    app->robotScene.stilo.feet2Circle.x = newHip2.x + app->stiloHip2DragFeetOffset.x;
                    app->robotScene.stilo.feet2Circle.y = newHip2.y + app->stiloHip2DragFeetOffset.y;

                    app->robotScene.stilo.hip2Circle = newHip2;

                    adjustStiloThigh2Arcs(app);
                }

                PointF stiloLeg2LocalMouse = inverseRotate(stiloLocalMouse, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Angle);

                if (app->draggingStiloFeet2)
                {
                    app->robotScene.stilo.feet2Circle = constrainToAxis(
                        app->robotScene.stilo.hip2Circle,
                        app->robotScene.stilo.feet2Circle,
                        stiloLeg2LocalMouse,
                        MIN_LIMB_LENGTH);

                    adjustStiloThigh2Arcs(app);
                }

                if (app->draggingStiloThigh2Arc1)
                {
                    SafeAngleRange range = filletSafeAngleRange(app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Radius,
                                                                 app->robotScene.stilo.feet2Circle, app->robotScene.stilo.feet2Radius,
                                                                 MAX_SEMNI_THIGH_ARC_R);
                    float maxDelta = range.halfWidthDeg - THIGH_ARC_ANGLE_MARGIN_DEG;
                    if (maxDelta < THIGH_ARC_SIDE_MARGIN_DEG) maxDelta = THIGH_ARC_SIDE_MARGIN_DEG;

                    float perpNow = perpOffsetOnAxis(stiloLeg2LocalMouse, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.feet2Circle);
                    float raw = app->stiloThigh2ArcDragStartAngle + (perpNow - app->stiloThigh2ArcDragStartPerp) * THIGH_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                    float delta = raw - range.centerDeg;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    if (delta > -THIGH_ARC_SIDE_MARGIN_DEG) delta = -THIGH_ARC_SIDE_MARGIN_DEG;
                    if (delta < -maxDelta) delta = -maxDelta;

                    app->robotScene.stilo.thigh2Arc1Angle = range.centerDeg + delta;
                }

                if (app->draggingStiloThigh2Arc2)
                {
                    SafeAngleRange range = filletSafeAngleRangeConcave(app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Radius,
                                                                        app->robotScene.stilo.feet2Circle, app->robotScene.stilo.feet2Radius,
                                                                        MAX_THIGH_ARC2_CONCAVE_R);
                    float maxDelta = range.halfWidthDeg - THIGH_ARC_ANGLE_MARGIN_DEG;
                    if (maxDelta < 0.0f) maxDelta = 0.0f;

                    float perpNow = perpOffsetOnAxis(stiloLeg2LocalMouse, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.feet2Circle);
                    float raw = app->stiloThigh2ArcDragStartAngle + (perpNow - app->stiloThigh2ArcDragStartPerp) * THIGH_ARC_DRAG_SENSITIVITY_DEG_PER_UNIT;

                    float delta = raw - range.centerDeg;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    if (delta > maxDelta) delta = maxDelta;
                    if (delta < -maxDelta) delta = -maxDelta;

                    app->robotScene.stilo.thigh2Arc2Angle = range.centerDeg + delta;
                }

                break;
            }

            // Per-joint hover/drag feedback beyond Rocky's one body handle
            // and Stilo's full set above only exists for Semni here -- see
            // WM_LBUTTONDOWN's matching guard.
            if (app->robotScene.activeKind != ROBOT_KIND_SEMNI)
            {
                SetWindowText(app->ui.hHoverLabel, L"");
                app->hoveredCircleSegment = -1;
                app->hoveredBodyCircle = -1;
                break;
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
            PointF footWorld = nestedJointToWorld(app->robotScene.robot.footCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
            PointF headWorld  = rotatePoint((PointF){app->robotScene.robot.headX, app->robotScene.robot.y}, center, angle);
            PointF buttWorld  = rotatePoint((PointF){app->robotScene.robot.buttX, app->robotScene.robot.y}, center, angle);

            app->hoverHip   = isNear(mouse, innerWorld, HIP_HANDLE_RADIUS);
            app->hoverKnee  = isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS);
            app->hoverFoot = isNear(mouse, footWorld, FOOT_HANDLE_RADIUS);
            app->hoverHead  = isNear(mouse, headWorld, HEAD_BUTT_HANDLE_RADIUS);
            app->hoverButt  = isNear(mouse, buttWorld, HEAD_BUTT_HANDLE_RADIUS);

            // View Segments hover: which SINGLE circle -- out of both the
            // 6 fillet circles (computeSemniCircleSegments) and the 5
            // always-visible body circles (computeSemniBodyCircles) -- is
            // nearest the mouse, within a small pick tolerance. Same idea
            // as the ArcSpline canvas's findHoveredSegment, gated the same
            // way canvas.c gates its own hover detection on
            // canvas.showSegments. Picks against each circle's EDGE
            // (|distToCenter - radius|), not its interior, since that's
            // what's actually drawn -- a circle outline isn't a filled
            // shape.
            //
            // The two candidate sets are searched against ONE shared
            // bestDist rather than independently, so exactly one circle
            // (fillet OR body, never both) ends up highlighted even where
            // a fillet and a body circle happen to sit close together --
            // whichever loop finds a closer match resets the other kind's
            // result back to -1.
            if (app->showCircleSegments)
            {
                CircleSegment fillets[NUM_ROBOT_CIRCLE_SEGMENTS];
                computeSemniCircleSegments(app->robotScene.robot, fillets);

                CircleSegment bodySegs[NUM_ROBOT_BODY_CIRCLES];
                computeSemniBodyCircles(app->robotScene.robot, bodySegs);

                // World-unit pick radius that corresponds to a constant
                // on-screen size, same reasoning as ArcSpline's "tolerance
                // = 0.05 * canvas.zoom" -- except Semni's zoom convention
                // is inverted (see graphicsGetZoom's comment: bigger means
                // zoomed IN here, unlike canvas.zoom), so this divides
                // instead of multiplies, and also folds in the robot size
                // slider (graphicsGetRobotScale) since that scales the
                // view the same way zoom does.
                float effectiveZoom = graphicsGetZoom() * graphicsGetRobotScale();
                float tolerance = 0.05f / effectiveZoom;

                int bestFillet = -1;
                int bestBody = -1;
                float bestDist = tolerance;

                for (int i = 0; i < NUM_ROBOT_CIRCLE_SEGMENTS; i++)
                {
                    float dx = mouse.x - fillets[i].center.x;
                    float dy = mouse.y - fillets[i].center.y;
                    float distToCenter = sqrtf(dx * dx + dy * dy);
                    float distToEdge = fabsf(distToCenter - fillets[i].radius);

                    if (distToEdge < bestDist)
                    {
                        bestDist = distToEdge;
                        bestFillet = i;
                        bestBody = -1;
                    }
                }

                for (int i = 0; i < NUM_ROBOT_BODY_CIRCLES; i++)
                {
                    float dx = mouse.x - bodySegs[i].center.x;
                    float dy = mouse.y - bodySegs[i].center.y;
                    float distToCenter = sqrtf(dx * dx + dy * dy);
                    float distToEdge = fabsf(distToCenter - bodySegs[i].radius);

                    if (distToEdge < bestDist)
                    {
                        bestDist = distToEdge;
                        bestBody = i;
                        bestFillet = -1;
                    }
                }

                app->hoveredCircleSegment = bestFillet;
                app->hoveredBodyCircle = bestBody;
            }
            else
            {
                app->hoveredCircleSegment = -1;
                app->hoveredBodyCircle = -1;
            }

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

                Fillet thigh1FilletHover = filletFromAttachAngle(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.thighArc1Angle, MIN_THIGH_ARC_R, MAX_SEMNI_THIGH_ARC_R);
                PointF thigh1NearLocalHover = circleTowardPoint(thigh1FilletHover.center, thigh1FilletHover.radius, thighAxisMidLocalHover);
                PointF thigh1MidLocalHover = circleAtAxisMid(thigh1FilletHover.center, thigh1FilletHover.radius, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, thigh1NearLocalHover);

                Fillet thigh2FilletHover = filletFromAttachAngleConcave(app->robotScene.robot.innerCircle, app->robotScene.robot.innerRadius, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.thighArc2Angle, MIN_THIGH_ARC_R, MAX_THIGH_ARC2_CONCAVE_R);
                PointF thigh2NearLocalHover = circleTowardPoint(thigh2FilletHover.center, thigh2FilletHover.radius, thighAxisMidLocalHover);
                PointF thigh2MidLocalHover = circleAtAxisMid(thigh2FilletHover.center, thigh2FilletHover.radius, app->robotScene.robot.innerCircle, app->robotScene.robot.kneeCircle, thigh2NearLocalHover);

                PointF thigh1WorldHover = jointToWorld(thigh1MidLocalHover, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
                PointF thigh2WorldHover = jointToWorld(thigh2MidLocalHover, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);

                PointF shinAxisMidLocalHover = { (app->robotScene.robot.kneeCircle.x + app->robotScene.robot.footCircle.x) * 0.5f,
                                                 (app->robotScene.robot.kneeCircle.y + app->robotScene.robot.footCircle.y) * 0.5f };

                Fillet shin1FilletHover = filletFromAttachAngle(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.footCircle, app->robotScene.robot.footRadius, app->robotScene.robot.shinArc1Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC_R);
                PointF shin1NearLocalHover = circleTowardPoint(shin1FilletHover.center, shin1FilletHover.radius, shinAxisMidLocalHover);
                PointF shin1MidLocalHover = circleAtAxisMid(shin1FilletHover.center, shin1FilletHover.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.footCircle, shin1NearLocalHover);

                Fillet shin2FilletHover = filletFromAttachAngleConcave(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius, app->robotScene.robot.footCircle, app->robotScene.robot.footRadius, app->robotScene.robot.shinArc2Angle, MIN_SHIN_ARC_R, MAX_SHIN_ARC2_CONCAVE_R);
                PointF shin2NearLocalHover = circleTowardPoint(shin2FilletHover.center, shin2FilletHover.radius, shinAxisMidLocalHover);
                PointF shin2MidLocalHover = circleAtAxisMid(shin2FilletHover.center, shin2FilletHover.radius, app->robotScene.robot.kneeCircle, app->robotScene.robot.footCircle, shin2NearLocalHover);

                PointF shin1WorldHover = nestedJointToWorld(shin1MidLocalHover, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);
                PointF shin2WorldHover = nestedJointToWorld(shin2MidLocalHover, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, angle);

                // priority mirrors WM_LBUTTONDOWN's hit-test order
                // (seamArc1, seamArc2, hip, knee, thigh1, thigh2, foot,
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
                else if (app->hoverFoot)
                    hoverLabel = L"Foot";
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
                !app->draggingFoot && !app->draggingShin1 && !app->draggingShin2)
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

                app->robotScene.robot.footCircle.x = newInner.x + app->hipDragFootOffset.x;
                app->robotScene.robot.footCircle.y = newInner.y + app->hipDragFootOffset.y;

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
            // the raw local coordinates for knee/thigh/foot/shin
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
                app->robotScene.robot.footCircle.x = newKnee.x + app->kneeDragFootOffset.x;
                app->robotScene.robot.footCircle.y = newKnee.y + app->kneeDragFootOffset.y;

                // shinArc1Angle/shinArc2Angle don't need re-anchoring
                // either -- already relative to kneeCircle, which just
                // moved to newKnee above

                // the hip<->knee distance just changed, so the thigh arcs'
                // fillet solve did too -- re-validate their existing
                // angles against it. The shin arcs are left alone
                // (knee<->foot distance is preserved by the re-anchor
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
                                                             MAX_SEMNI_THIGH_ARC_R);
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
            // the raw local coordinates for foot/shin
            PointF shinLocalMouse = inverseRotate(legLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle);

            // same idea for the foot circle: constrained to the
            // knee->foot axis so dragging it only changes the shin's
            // length, not its direction
            if (app->draggingFoot)
            {
                app->robotScene.robot.footCircle = constrainToAxis(
                    app->robotScene.robot.kneeCircle,
                    app->robotScene.robot.footCircle,
                    shinLocalMouse,
                    MIN_LIMB_LENGTH);

                // knee<->foot distance just changed, so the shin arcs'
                // fillet solve did too -- re-validate their existing
                // angles against it
                adjustShinArcs(app);
            }

            // shin arcs: same tangent-restricted, angle-driven drag as the
            // thigh arcs above, just reading the mouse's perpendicular-to-
            // axis movement relative to the knee->foot axis instead of
            // hip->knee (shinLocalMouse already has kneeAngle undone, same
            // way legLocalMouse has hipAngle undone for the thigh).
            // shinArc1Angle (convex) stays locked to the negative-delta
            // side of ITS centerDeg, same "stay off the degenerate center"
            // safety seam arc 1/2 and thighArc1Angle use. shinArc2Angle
            // (concave) drags against a totally different, disjoint safe
            // range (see its own block below) so it doesn't need that
            // same one-sided lock. Dragging one never mirrors the other.
            // Nothing needs recentering on a knee/foot move -- the angle
            // is already fully relative to kneeCircle/footCircle, so the
            // fillet solve just adapts automatically every frame.
            if (app->draggingShin1)
            {
                SafeAngleRange range = filletSafeAngleRange(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                             app->robotScene.robot.footCircle, app->robotScene.robot.footRadius,
                                                             MAX_SHIN_ARC_R);
                float maxDelta = range.halfWidthDeg - SHIN_ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < SHIN_ARC_SIDE_MARGIN_DEG) maxDelta = SHIN_ARC_SIDE_MARGIN_DEG;

                float perpNow = perpOffsetOnAxis(shinLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.footCircle);
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
            // opposite side of kneeCircle, facing footCircle. No
            // side-lock needed: shinArc1Angle's range and this one don't
            // share a degenerate center, so this just clamps symmetrically
            // to whichever side of ITS OWN center the drag reaches.
            if (app->draggingShin2)
            {
                SafeAngleRange range = filletSafeAngleRangeConcave(app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeRadius,
                                                                    app->robotScene.robot.footCircle, app->robotScene.robot.footRadius,
                                                                    MAX_SHIN_ARC2_CONCAVE_R);
                float maxDelta = range.halfWidthDeg - SHIN_ARC_ANGLE_MARGIN_DEG;
                if (maxDelta < 0.0f) maxDelta = 0.0f;

                float perpNow = perpOffsetOnAxis(shinLocalMouse, app->robotScene.robot.kneeCircle, app->robotScene.robot.footCircle);
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

            // Rocky's body handle: plain scroll while hovering it resizes
            // the rectangle (both half-width and half-height together),
            // same "hover the joint circle handle, scroll to resize"
            // interaction as Semni's own hip circle (see the innerRadius
            // branch further below) -- no Shift-gated rotate here, since
            // Rocky's whole-body angle has no dedicated scroll gesture yet
            // (only the keyboard Left/Right, see WM_KEYDOWN).
            if (app->robotScene.activeKind == ROBOT_KIND_ROCKY)
            {
                PointF rockyCenter = getRockyCenter(app->robotScene.rocky);
                PointF kneeWorld = rotatePoint(app->robotScene.rocky.kneeCircle, rockyCenter, app->robotScene.rocky.angle);
                PointF footWorld = jointToWorld(app->robotScene.rocky.footCircle, app->robotScene.rocky.kneeCircle, app->robotScene.rocky.kneeAngle, rockyCenter, app->robotScene.rocky.angle);

                // wheel messages pack the modifier keys into the low word
                // of wParam, same as WM_MOUSEMOVE -- see Semni's own
                // shiftHeld read further below
                int shiftHeld = (LOWORD(wParam) & MK_SHIFT) != 0;

                if (isNear(mouse, rockyCenter, HIP_HANDLE_RADIUS))
                {
                    float sizeStep = 0.02f;
                    if (wheelDelta > 0)
                    {
                        app->robotScene.rocky.bodyHalfWidth += sizeStep;
                        app->robotScene.rocky.bodyHalfHeight += sizeStep;
                    }
                    else
                    {
                        app->robotScene.rocky.bodyHalfWidth -= sizeStep;
                        app->robotScene.rocky.bodyHalfHeight -= sizeStep;
                    }

                    if (app->robotScene.rocky.bodyHalfWidth < MIN_ROCKY_BODY_HALF)
                        app->robotScene.rocky.bodyHalfWidth = MIN_ROCKY_BODY_HALF;
                    if (app->robotScene.rocky.bodyHalfWidth > MAX_ROCKY_BODY_HALF)
                        app->robotScene.rocky.bodyHalfWidth = MAX_ROCKY_BODY_HALF;

                    if (app->robotScene.rocky.bodyHalfHeight < MIN_ROCKY_BODY_HALF)
                        app->robotScene.rocky.bodyHalfHeight = MIN_ROCKY_BODY_HALF;
                    if (app->robotScene.rocky.bodyHalfHeight > MAX_ROCKY_BODY_HALF)
                        app->robotScene.rocky.bodyHalfHeight = MAX_ROCKY_BODY_HALF;

                    // shrinking the body can leave the knee (and the
                    // foot/shin hanging off it) outside the new bounds --
                    // pull it back in if so
                    clampRockyKneeToBody(app);
                }
                else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS) && shiftHeld)
                {
                    // rotate the knee joint -- bends the foot/shin around
                    // kneeCircle, leaving the rectangle and the knee's own
                    // position untouched. Gated behind Shift for the same
                    // reason as Semni's own knee/hip rotate: a plain
                    // scroll while hovering the handle falls through to
                    // the ordinary view-zoom below instead of bending the
                    // leg by accident.
                    float step = 2.0f;
                    if (wheelDelta > 0)
                        app->robotScene.rocky.kneeAngle += step;
                    else
                        app->robotScene.rocky.kneeAngle -= step;
                }
                else if (isNear(mouse, kneeWorld, KNEE_HANDLE_RADIUS))
                {
                    // plain scroll (no Shift) resizes the knee circle
                    // itself instead of bending -- same idea as the body
                    // handle above and Semni's own knee handle
                    float radiusStep = 0.01f;
                    if (wheelDelta > 0)
                        app->robotScene.rocky.kneeRadius += radiusStep;
                    else
                        app->robotScene.rocky.kneeRadius -= radiusStep;

                    if (app->robotScene.rocky.kneeRadius < MIN_R)
                        app->robotScene.rocky.kneeRadius = MIN_R;
                    if (app->robotScene.rocky.kneeRadius > MAX_R)
                        app->robotScene.rocky.kneeRadius = MAX_R;

                    // knee radius feeds the shin arcs' fillet solve --
                    // re-validate their existing angles against the new size
                    adjustRockyShinArcs(app);
                }
                else if (isNear(mouse, footWorld, FOOT_HANDLE_RADIUS))
                {
                    // plain scroll on the foot handle resizes it -- no
                    // Shift-gated rotate branch needed, same reasoning as
                    // Semni's own foot handle: it's the last joint in the
                    // chain, so there's nothing further down to bend
                    float radiusStep = 0.01f;
                    if (wheelDelta > 0)
                        app->robotScene.rocky.footRadius += radiusStep;
                    else
                        app->robotScene.rocky.footRadius -= radiusStep;

                    if (app->robotScene.rocky.footRadius < MIN_R)
                        app->robotScene.rocky.footRadius = MIN_R;
                    if (app->robotScene.rocky.footRadius > MAX_R)
                        app->robotScene.rocky.footRadius = MAX_R;

                    // foot radius feeds the shin arcs' fillet solve too --
                    // re-validate their existing angles against the new size
                    adjustRockyShinArcs(app);
                }
                else
                {
                    float factor = (wheelDelta > 0) ? ZOOM_STEP : (1.0f / ZOOM_STEP);
                    graphicsZoom(factor);
                }
                break;
            }

            // Stilo now has just a hip + feet handle per leg's worth of
            // scroll-to-rotate/resize (no knee/foot/shin sub-stage any
            // more -- see app.h's Stilo comment) -- same construction as
            // the ROBOT_KIND_SEMNI block below, operating on
            // app->robotScene.stilo instead of Semni's own.
            if (app->robotScene.activeKind == ROBOT_KIND_STILO)
            {
                PointF stiloCenter = getStiloCenter(app->robotScene.stilo);
                PointF stiloHip1World  = rotatePoint(app->robotScene.stilo.hip1Circle, stiloCenter, app->robotScene.stilo.angle);
                PointF stiloFeet1World = jointToWorld(app->robotScene.stilo.feet1Circle, app->robotScene.stilo.hip1Circle, app->robotScene.stilo.hip1Angle, stiloCenter, app->robotScene.stilo.angle);
                PointF stiloHeadWorld = rotatePoint((PointF){app->robotScene.stilo.headX, app->robotScene.stilo.y}, stiloCenter, app->robotScene.stilo.angle);
                PointF stiloButtWorld = rotatePoint((PointF){app->robotScene.stilo.buttX, app->robotScene.stilo.y}, stiloCenter, app->robotScene.stilo.angle);

                // leg 2's own hip/feet -- same construction as leg 1's
                // above, against the hip2/feet2 fields. No leg-2 equivalent
                // of stiloHeadWorld/stiloButtWorld -- those are torso-level,
                // shared with leg 1.
                PointF stiloHip2World  = rotatePoint(app->robotScene.stilo.hip2Circle, stiloCenter, app->robotScene.stilo.angle);
                PointF stiloFeet2World = jointToWorld(app->robotScene.stilo.feet2Circle, app->robotScene.stilo.hip2Circle, app->robotScene.stilo.hip2Angle, stiloCenter, app->robotScene.stilo.angle);

                float stiloStep = 2.0f;
                float stiloRadiusStep = 0.01f;

                int stiloShiftHeld = (LOWORD(wParam) & MK_SHIFT) != 0;

                if (isNear(mouse, stiloHip1World, HIP_HANDLE_RADIUS) && stiloShiftHeld)
                {
                    if (wheelDelta > 0)
                        app->robotScene.stilo.hip1Angle += stiloStep;
                    else
                        app->robotScene.stilo.hip1Angle -= stiloStep;
                }
                else if (isNear(mouse, stiloHip1World, HIP_HANDLE_RADIUS))
                {
                    if (wheelDelta > 0)
                        app->robotScene.stilo.hip1Radius += stiloRadiusStep;
                    else
                        app->robotScene.stilo.hip1Radius -= stiloRadiusStep;

                    if (app->robotScene.stilo.hip1Radius < MIN_R)
                        app->robotScene.stilo.hip1Radius = MIN_R;
                    if (app->robotScene.stilo.hip1Radius > MAX_R)
                        app->robotScene.stilo.hip1Radius = MAX_R;

                    adjustStiloThigh1Arcs(app);
                }
                else if (isNear(mouse, stiloFeet1World, KNEE_HANDLE_RADIUS))
                {
                    if (wheelDelta > 0)
                        app->robotScene.stilo.feet1Radius += stiloRadiusStep;
                    else
                        app->robotScene.stilo.feet1Radius -= stiloRadiusStep;

                    if (app->robotScene.stilo.feet1Radius < MIN_R)
                        app->robotScene.stilo.feet1Radius = MIN_R;
                    if (app->robotScene.stilo.feet1Radius > MAX_R)
                        app->robotScene.stilo.feet1Radius = MAX_R;

                    adjustStiloThigh1Arcs(app);
                }
                else if (isNear(mouse, stiloHeadWorld, HEAD_BUTT_HANDLE_RADIUS))
                {
                    if (wheelDelta > 0)
                        app->robotScene.stilo.headRadius += stiloRadiusStep;
                    else
                        app->robotScene.stilo.headRadius -= stiloRadiusStep;

                    if (app->robotScene.stilo.headRadius < MIN_R)
                        app->robotScene.stilo.headRadius = MIN_R;
                    if (app->robotScene.stilo.headRadius > MAX_R)
                        app->robotScene.stilo.headRadius = MAX_R;
                }
                else if (isNear(mouse, stiloButtWorld, HEAD_BUTT_HANDLE_RADIUS))
                {
                    if (wheelDelta > 0)
                        app->robotScene.stilo.buttRadius += stiloRadiusStep;
                    else
                        app->robotScene.stilo.buttRadius -= stiloRadiusStep;

                    if (app->robotScene.stilo.buttRadius < MIN_R)
                        app->robotScene.stilo.buttRadius = MIN_R;
                    if (app->robotScene.stilo.buttRadius > MAX_R)
                        app->robotScene.stilo.buttRadius = MAX_R;
                }
                else if (isNear(mouse, stiloHip2World, HIP_HANDLE_RADIUS) && stiloShiftHeld)
                {
                    if (wheelDelta > 0)
                        app->robotScene.stilo.hip2Angle += stiloStep;
                    else
                        app->robotScene.stilo.hip2Angle -= stiloStep;
                }
                else if (isNear(mouse, stiloHip2World, HIP_HANDLE_RADIUS))
                {
                    if (wheelDelta > 0)
                        app->robotScene.stilo.hip2Radius += stiloRadiusStep;
                    else
                        app->robotScene.stilo.hip2Radius -= stiloRadiusStep;

                    if (app->robotScene.stilo.hip2Radius < MIN_R)
                        app->robotScene.stilo.hip2Radius = MIN_R;
                    if (app->robotScene.stilo.hip2Radius > MAX_R)
                        app->robotScene.stilo.hip2Radius = MAX_R;

                    adjustStiloThigh2Arcs(app);
                }
                else if (isNear(mouse, stiloFeet2World, KNEE_HANDLE_RADIUS))
                {
                    if (wheelDelta > 0)
                        app->robotScene.stilo.feet2Radius += stiloRadiusStep;
                    else
                        app->robotScene.stilo.feet2Radius -= stiloRadiusStep;

                    if (app->robotScene.stilo.feet2Radius < MIN_R)
                        app->robotScene.stilo.feet2Radius = MIN_R;
                    if (app->robotScene.stilo.feet2Radius > MAX_R)
                        app->robotScene.stilo.feet2Radius = MAX_R;

                    adjustStiloThigh2Arcs(app);
                }
                else
                {
                    float factor = (wheelDelta > 0) ? ZOOM_STEP : (1.0f / ZOOM_STEP);
                    graphicsZoom(factor);
                }
                break;
            }

            // Per-joint scroll-to-rotate/resize beyond Rocky's one body
            // handle and Stilo's full set above only exists for Semni here
            // -- see WM_LBUTTONDOWN's matching guard.
            if (app->robotScene.activeKind != ROBOT_KIND_SEMNI)
            {
                float factor = (wheelDelta > 0) ? ZOOM_STEP : (1.0f / ZOOM_STEP);
                graphicsZoom(factor);
                break;
            }

            PointF center = getCenter(app->robotScene.robot);
            PointF innerWorld = rotatePoint(app->robotScene.robot.innerCircle, center, app->robotScene.robot.angle);
            PointF kneeWorld = jointToWorld(app->robotScene.robot.kneeCircle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, app->robotScene.robot.angle);
            PointF footWorld = nestedJointToWorld(app->robotScene.robot.footCircle, app->robotScene.robot.kneeCircle, app->robotScene.robot.kneeAngle, app->robotScene.robot.innerCircle, app->robotScene.robot.hipAngle, center, app->robotScene.robot.angle);
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
                // it -- knee, foot, thigh/shin handles), not the whole
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
                // rotate just the knee joint (and the shin/foot hanging
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
                // <-> knee) and the shin arcs' (knee <-> foot), so both
                // pairs get re-validated
                adjustThighArcs(app);
                adjustShinArcs(app);
            }
            else if (isNear(mouse, footWorld, FOOT_HANDLE_RADIUS))
            {
                // plain scroll on the foot/foot handle resizes it -- no
                // Shift-gated rotate branch needed here, unlike the hip and
                // knee above: the foot is the last joint in the chain, so
                // there's nothing further down for a rotation to carry
                // along, and it can just always resize on scroll like the
                // head/butt handles below
                if (wheelDelta > 0)
                    app->robotScene.robot.footRadius += radiusStep;
                else
                    app->robotScene.robot.footRadius -= radiusStep;

                if (app->robotScene.robot.footRadius < MIN_R)
                    app->robotScene.robot.footRadius = MIN_R;
                if (app->robotScene.robot.footRadius > MAX_R)
                    app->robotScene.robot.footRadius = MAX_R;

                // foot radius feeds the shin arcs' fillet solve --
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

        // A view-only zoom (the "else" branch just above) doesn't change
        // the robot's actual size, but every OTHER branch in this handler
        // does (a radius nudge) -- cheaper to just always refresh than to
        // thread a "did this actually change geometry" flag out of every
        // branch above.
        updateRobotSizeLabel(app);
        break;

        case WM_HSCROLL:
        {
            // Only message this trackbar sends -- fires on every drag step,
            // arrow-key nudge, and click-on-the-track, not just release, so
            // the robot resizes live as the slider moves.
            if ((HWND)lParam == app->ui.hScaleSlider)
            {
                int pos = (int)SendMessage(app->ui.hScaleSlider, TBM_GETPOS, 0, 0);
                graphicsSetRobotScale(pos / 100.0f);

                // Live value in the label instead of a static "Scale" --
                // matches the ArcSpline panel's "Thickness: N px" label
                // (ui.c). wsprintf has no float conversion, so the
                // pos (25-100) is split into whole/hundredths by hand
                // (e.g. pos=75 -> "0.75", pos=100 -> "1.00").
                wchar_t scaleBuf[32];
                wsprintf(scaleBuf, L"Scale: %d.%02d", pos / 100, pos % 100);
                SetWindowText(app->ui.hScaleLabel, scaleBuf);

                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        break;

        case WM_KEYDOWN:
        {
            // Ctrl+Numpad0: reset the view (zoom to 100%, pan back to
            // center) -- same shortcut/convention as the ArcSpline canvas's
            // own Ctrl+Numpad0 (see canvas.c's WM_KEYDOWN) and Simulation's
            // (sim_camera.h's simCameraReset). Deliberately VK_NUMPAD0, not
            // the top-row '0' (see canvas.c's WM_KEYDOWN for why those are
            // different VK codes). Also deliberately leaves
            // graphicsSetRobotScale's "size" slider alone -- that's the
            // robot's own configured size, not a camera position, so this
            // shouldn't undo it.
            if (wParam == VK_NUMPAD0 && (GetAsyncKeyState(VK_CONTROL) & 0x8000))
            {
                graphicsResetView();
                InvalidateRect(hwnd, NULL, FALSE);
                break;
            }

            // Whole-body rotate (Left/Right) and move (Up/Down) act on
            // whichever robot is currently active (see app.h's RobotKind).
            // Rocky still relies on this for its whole-body pose (it has
            // no whole-body drag handle of its own); Stilo now ALSO has
            // Semni's full per-joint dragging (see WM_LBUTTONDOWN/
            // WM_MOUSEMOVE/WM_MOUSEWHEEL), but keeps this same whole-body
            // nudge too, same as Semni does below.
            switch (app->robotScene.activeKind)
            {
                case ROBOT_KIND_ROCKY:
                {
                    if (wParam == VK_LEFT)
                        app->robotScene.rocky.angle += 2.0f;

                    if (wParam == VK_RIGHT)
                        app->robotScene.rocky.angle -= 2.0f;

                    if (wParam == VK_UP || wParam == VK_DOWN)
                    {
                        int ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                        float step = ctrlHeld ? 0.01f : 0.05f;
                        if (wParam == VK_DOWN) step = -step;

                        // Move the whole robot (rectangle + leg) together
                        app->robotScene.rocky.bodyY += step;
                        app->robotScene.rocky.kneeCircle.y += step;
                        app->robotScene.rocky.footCircle.y += step;
                    }
                    break;
                }

                case ROBOT_KIND_STILO:
                {
                    if (wParam == VK_LEFT)
                        app->robotScene.stilo.angle += 2.0f;

                    if (wParam == VK_RIGHT)
                        app->robotScene.stilo.angle -= 2.0f;

                    if (wParam == VK_UP || wParam == VK_DOWN)
                    {
                        int ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                        float step = ctrlHeld ? 0.01f : 0.05f;
                        if (wParam == VK_DOWN) step = -step;

                        app->robotScene.stilo.y += step;
                        app->robotScene.stilo.hip1Circle.y += step;
                        app->robotScene.stilo.feet1Circle.y += step;

                        // leg 2 hangs off the same shared torso Y above --
                        // carry it along the same whole-body nudge, same
                        // "y/hip1Circle.y/feet1Circle.y" pattern as leg 1
                        // just above
                        app->robotScene.stilo.hip2Circle.y += step;
                        app->robotScene.stilo.feet2Circle.y += step;
                    }
                    break;
                }

                case ROBOT_KIND_SEMNI:
                default:
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
                        app->robotScene.robot.footCircle.y += step;
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
                        app->robotScene.robot.footCircle.y -= step;
                    }
                    break;
                }
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

            // ---- top-right control panel ----
            // Everything below is laid out relative to one panel rect
            // (hControlPanel) instead of each control computing its own
            // right-aligned offset independently -- that's what used to
            // leave Standing/Home/Save/Mirror at four different widths
            // with no shared edge. Two equal-width columns (colW) now
            // line up across every row, and the panel's solid backdrop
            // (see WM_CREATE) reads as one grouped control, not four
            // buttons floating loose over the viewport.
            int outerMargin = 12;
            int pad = 14;
            int rowGap = 8;
            int colGap = 8;
            int btnH = 30;
            int titleH = 20;
            int sliderH = 24;
            int comboRowH = 24;   // closed-box footprint in the layout; the control itself is taller (see below) so its dropdown list has room to show all 3 entries

            int contentW = 240;
            int colW = (contentW - colGap) / 2;

            int panelW = contentW + pad * 2;

            int relYTitle    = pad;
            int relYSelector = relYTitle    + titleH    + rowGap;  // robot picker (Semni/Rocky/Stilo)
            int relYRow1     = relYSelector + comboRowH + rowGap;  // Home | Standing
            int relYRow1b    = relYRow1     + btnH      + rowGap;  // Set Home | Set Standing
            int relYRow2     = relYRow1b    + btnH      + rowGap;  // Mirror Leg (own full-width row -- used to share this row with the now-removed Save button, see canvas.c's ID_SAVE for where Save moved)
            int relYMirror2  = relYRow2     + btnH      + rowGap;  // Mirror Leg 2 (Stilo only, inert otherwise)
            int relYWeight   = relYMirror2  + btnH      + rowGap;  // Body Wt | Leg Wt (Rocky only, inert otherwise)
            int relYSize     = relYWeight   + btnH      + rowGap;  // Size: W x H mm (live readout, all robot kinds)
            int relYScale    = relYSize     + btnH       + rowGap; // Scale label + slider
            int relYSeg      = relYScale    + sliderH    + rowGap; // View Segments
            int relYDebug    = relYSeg      + btnH       + rowGap; // Debug Log
            int panelH       = relYDebug    + btnH       + pad;

            int panelX = rect.right - outerMargin - panelW;
            int panelY = outerMargin;

            // HWND_BOTTOM (not SWP_NOZORDER) pins this at the bottom of
            // the Z-order on every resize -- belt-and-suspenders on top of
            // creation order (see WM_CREATE) so the backdrop can never end
            // up above the real controls and swallow their clicks.
            SetWindowPos(app->ui.hControlPanel, HWND_BOTTOM,
                 panelX, panelY, panelW, panelH,
                 SWP_NOACTIVATE);

            SetWindowPos(app->ui.hPanelTitle, NULL,
                 panelX + pad, panelY + relYTitle, contentW, titleH,
                 SWP_NOZORDER);

            int col1X = panelX + pad;
            int col2X = col1X + colW + colGap;

            // Height here (120) is the combo box's OWN window height, not
            // the row slot -- for a CBS_DROPDOWNLIST, Windows sizes the
            // closed box from the font metrics alone and uses the rest of
            // this height for how tall the dropped-down list can get, so
            // this needs to be taller than comboRowH (the layout slot
            // above) for all 3 entries to be visible when it's opened.
            SetWindowPos(app->ui.hRobotSelector, NULL,
                 col1X, panelY + relYSelector, contentW, 120,
                 SWP_NOZORDER);

            // Swapped from their original Standing-left/Home-right order.
            SetWindowPos(app->ui.hHomePositionButton, NULL,
                 col1X, panelY + relYRow1, colW, btnH,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hStandingPositionButton, NULL,
                 col2X, panelY + relYRow1, colW, btnH,
                 SWP_NOZORDER);

            // Set Home / Set Standing sit directly under the button whose
            // target pose they capture, so the column each is in matches
            // up with Home/Standing above.
            SetWindowPos(app->ui.hSetHomeButton, NULL,
                 col1X, panelY + relYRow1b, colW, btnH,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hSetStandingButton, NULL,
                 col2X, panelY + relYRow1b, colW, btnH,
                 SWP_NOZORDER);

            // Now the row's only occupant (Save moved to File > Save) --
            // full content width instead of sharing col1X/col2X with it.
            SetWindowPos(app->ui.hMirrorButton, NULL,
                 col1X, panelY + relYRow2, contentW, btnH,
                 SWP_NOZORDER);

            // Full content width, own row -- only meaningful for Stilo
            // (mirrors its leg 2 only, see ID_MIRROR_LEG2_BUTTON), so it
            // doesn't share a row/column with the single-leg Mirror button
            // above it.
            SetWindowPos(app->ui.hMirrorButton2, NULL,
                 col1X, panelY + relYMirror2, contentW, btnH,
                 SWP_NOZORDER);

            // Body Wt | Leg Wt -- each column holds its own short label +
            // edit box pair (label width picked just wide enough for
            // "Body Wt"/"Leg Wt" at this font, edit box takes the rest of
            // the column), same two-column row as Home/Standing etc. above.
            int weightLabelW = 50;
            int weightEditW = colW - weightLabelW - colGap;

            SetWindowPos(app->ui.hBodyWeightLabel, NULL,
                 col1X, panelY + relYWeight + (btnH - 20) / 2, weightLabelW, 20,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hBodyWeightEdit, NULL,
                 col1X + weightLabelW + colGap, panelY + relYWeight + (btnH - 22) / 2, weightEditW, 22,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hLegWeightLabel, NULL,
                 col2X, panelY + relYWeight + (btnH - 20) / 2, weightLabelW, 20,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hLegWeightEdit, NULL,
                 col2X + weightLabelW + colGap, panelY + relYWeight + (btnH - 22) / 2, weightEditW, 22,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hRobotSizeLabel, NULL,
                 col1X, panelY + relYSize + (btnH - 20) / 2, contentW, 20,
                 SWP_NOZORDER);

            // Scale label + slider share the full content width, same as
            // the two-button rows above them.
            int scaleLabelWidth = 70;
            int sliderWidth = contentW - scaleLabelWidth - colGap;

            SetWindowPos(app->ui.hScaleLabel, NULL,
                 col1X, panelY + relYScale + (sliderH - 20) / 2, scaleLabelWidth, 20,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hScaleSlider, NULL,
                 col1X + scaleLabelWidth + colGap, panelY + relYScale, sliderWidth, sliderH,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hViewSegmentsButton, NULL,
                 col1X, panelY + relYSeg, contentW, btnH,
                 SWP_NOZORDER);

            SetWindowPos(app->ui.hDebugLogButton, NULL,
                 col1X, panelY + relYDebug, contentW, btnH,
                 SWP_NOZORDER);

            // ---- bottom-left hover status strip ----
            int hoverPad = 8;
            int hoverLabelW = 280;
            int hoverLabelH = 20;
            int hoverPanelW = hoverLabelW + hoverPad * 2;
            int hoverPanelH = hoverLabelH + hoverPad * 2;
            int hoverPanelX = outerMargin;
            int hoverPanelY = rect.bottom - outerMargin - hoverPanelH;

            // Same HWND_BOTTOM pinning as hControlPanel above.
            SetWindowPos(app->ui.hHoverPanel, HWND_BOTTOM,
                 hoverPanelX, hoverPanelY, hoverPanelW, hoverPanelH,
                 SWP_NOACTIVATE);

            SetWindowPos(app->ui.hHoverLabel, NULL,
                 hoverPanelX + hoverPad, hoverPanelY + hoverPad, hoverLabelW, hoverLabelH,
                 SWP_NOZORDER);

            // Any resize (a live border drag, minimize/restore, or
            // toggling fullscreen/maximize) can leave one of these child
            // controls' own clip region stale for a frame or two, since
            // the OpenGL viewport behind them repaints the whole client
            // rect continuously in the render loop and that repaint can
            // race the SetWindowPos calls above -- the robot picker combo
            // box in particular has been seen to vanish under the GL
            // repaint until a later hover forces Windows to repaint it on
            // its own. Force that repaint right now instead of waiting on
            // it.
            //
            // Deliberately targets each child individually rather than
            // RedrawWindow(hwnd, ..., RDW_ALLCHILDREN) on the whole window:
            // that form also marks hwnd's OWN client area invalid, and
            // RDW_UPDATENOW would then dispatch hwnd's WM_PAINT (which
            // calls renderCombinedFrame's covering GL SwapBuffers)
            // synchronously in the very same batch -- so it could end up
            // repainting the children and then immediately re-covering
            // them again before this call even returns, silently undoing
            // the fix. Only invalidating the children (never hwnd itself)
            // means this can only ever un-cover them, not re-cover them.
            HWND rockyPanelChildren[] = {
                app->ui.hControlPanel,
                app->ui.hPanelTitle,
                app->ui.hRobotSelector,
                app->ui.hStandingPositionButton,
                app->ui.hHomePositionButton,
                app->ui.hSetStandingButton,
                app->ui.hSetHomeButton,
                app->ui.hMirrorButton,
                app->ui.hMirrorButton2,
                app->ui.hBodyWeightLabel,
                app->ui.hBodyWeightEdit,
                app->ui.hLegWeightLabel,
                app->ui.hLegWeightEdit,
                app->ui.hRobotSizeLabel,
                app->ui.hScaleLabel,
                app->ui.hScaleSlider,
                app->ui.hViewSegmentsButton,
                app->ui.hDebugLogButton,
                app->ui.hHoverPanel,
                app->ui.hHoverLabel,
            };
            int childCount = sizeof(rockyPanelChildren) / sizeof(rockyPanelChildren[0]);
            for (int i = 0; i < childCount; i++)
            {
                if (rockyPanelChildren[i])
                {
                    InvalidateRect(rockyPanelChildren[i], NULL, TRUE);
                    UpdateWindow(rockyPanelChildren[i]);
                }
            }
        }
        break;

        case WM_CREATE:
        {
             // -1 = nothing hovered. Otherwise these would sit at their
             // zero-initialized default (a valid circle index, 0) until
             // the first WM_MOUSEMOVE recomputes them, which could briefly
             // mislabel seam arc 1 / the head circle as hovered before the
             // mouse ever moves.
             app->hoveredCircleSegment = -1;
             app->hoveredBodyCircle = -1;

             ensureSemniUIFonts();
             ensureSemniPanelClassRegistered();

             // Backdrop behind the whole top-right button cluster, created
             // first so every real control below (created after it) draws
             // on top of it -- see ensureSemniPanelClassRegistered's
             // comment. Real position/size is computed in WM_SIZE, which
             // fires once right after WM_CREATE.
             app->ui.hControlPanel = CreateWindowEx(
                WS_EX_STATICEDGE,
                L"SemniControlPanel",
                L"",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                NULL,
                NULL,
                NULL
            );

             app->ui.hPanelTitle = CreateWindow(
                L"STATIC",
                L"Robot Controls",
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                0, 0, 10, 10,
                hwnd,
                NULL,
                NULL,
                NULL
            );
             SendMessage(app->ui.hPanelTitle, WM_SETFONT, (WPARAM)g_semniUITitleFont, TRUE);

             // Robot selector: picks which of the three robots (Semni/
             // Rocky/Stilo, see app.h's RobotKind) every control below
             // this acts on. CBS_DROPDOWNLIST (not a plain CBS_DROPDOWN)
             // so it's pick-one-from-a-list only, no free text entry.
             // Index order here MUST match RobotKind's own 0/1/2 values --
             // WM_COMMAND's CBN_SELCHANGE handler casts CB_GETCURSEL's
             // result straight to RobotKind.
             app->ui.hRobotSelector = CreateWindow(
                L"COMBOBOX",
                L"",
                WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_ROBOT_SELECTOR,
                NULL,
                NULL
            );
             SendMessage(app->ui.hRobotSelector, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);
             SendMessage(app->ui.hRobotSelector, CB_ADDSTRING, 0, (LPARAM)L"Semni");
             SendMessage(app->ui.hRobotSelector, CB_ADDSTRING, 0, (LPARAM)L"Rocky");
             SendMessage(app->ui.hRobotSelector, CB_ADDSTRING, 0, (LPARAM)L"Stilo");
             SendMessage(app->ui.hRobotSelector, CB_SETCURSEL, (WPARAM)app->robotScene.activeKind, 0);

             app->ui.hStandingPositionButton = CreateWindow(
                L"BUTTON",
                L"Standing",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_STANDING_POSITION_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hStandingPositionButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             app->ui.hHomePositionButton = CreateWindow(
                L"BUTTON",
                L"Home",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_HOME_POSITION_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hHomePositionButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             // Captures the CURRENT pose as the new Standing/Home target
             // (see app.h's hSetStandingButton/hSetHomeButton comment and
             // input.c's ID_SET_STANDING_BUTTON/ID_SET_HOME_BUTTON below) --
             // a row of their own, directly under the Standing/Home
             // buttons they modify.
             app->ui.hSetStandingButton = CreateWindow(
                L"BUTTON",
                L"Set Standing",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_SET_STANDING_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hSetStandingButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             app->ui.hSetHomeButton = CreateWindow(
                L"BUTTON",
                L"Set Home",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_SET_HOME_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hSetHomeButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             // Save itself is no longer a button here -- it's been folded
             // into the shared File > Save menu item (see canvas.c's
             // WM_COMMAND ID_SAVE handling), which now dispatches on
             // editorModeState.currentMode/app->robotScene.activeKind the
             // same way this button used to.
             app->ui.hMirrorButton = CreateWindow(
                L"BUTTON",
                L"Mirror Leg",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_MIRROR_LEG_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hMirrorButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             // Mirrors Stilo's leg 2 only (see ID_MIRROR_LEG2_BUTTON) --
             // always created/shown alongside hMirrorButton above, same
             // "harmlessly inert unless applicable" convention as
             // hViewSegmentsButton, rather than hidden/disabled per robot
             // kind (see app.h's ID_MIRROR_LEG2_BUTTON comment).
             app->ui.hMirrorButton2 = CreateWindow(
                L"BUTTON",
                L"Mirror Leg 2",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_MIRROR_LEG2_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hMirrorButton2, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             // Body/Leg Weight: plain text entry boxes feeding Rocky's own
             // bodyWeight/legWeight fields (see app.h's Rocky comment) --
             // read via GetWindowText from the File > Save handler
             // (canvas.c) rather than an EN_CHANGE handler, since the only
             // thing that ever reads them is the save-to-Rob.txt/Arm.txt path (save.c's
             // saveRockyAsRobArm). Harmlessly inert for Semni/Stilo, same
             // "always created, only meaningful for one robot kind"
             // convention as hMirrorButton2/hViewSegmentsButton.
             app->ui.hBodyWeightLabel = CreateWindow(
                L"STATIC",
                L"Body Wt",
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                0, 0, 10, 10,
                hwnd,
                NULL,
                NULL,
                NULL
            );
             SendMessage(app->ui.hBodyWeightLabel, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             app->ui.hBodyWeightEdit = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"1.0",
                WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_BODY_WEIGHT_EDIT,
                NULL,
                NULL
            );
             SendMessage(app->ui.hBodyWeightEdit, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             app->ui.hLegWeightLabel = CreateWindow(
                L"STATIC",
                L"Leg Wt",
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                0, 0, 10, 10,
                hwnd,
                NULL,
                NULL,
                NULL
            );
             SendMessage(app->ui.hLegWeightLabel, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             app->ui.hLegWeightEdit = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"1.0",
                WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_LEG_WEIGHT_EDIT,
                NULL,
                NULL
            );
             SendMessage(app->ui.hLegWeightEdit, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             // Live real-world size readout (see config.h's
             // MM_PER_WORLD_UNIT and this file's updateRobotSizeLabel) --
             // full content width, own row, refreshed once right below
             // once app's fields have their real (post-initAppState)
             // values rather than left showing whatever placeholder text
             // it's created with.
             app->ui.hRobotSizeLabel = CreateWindow(
                L"STATIC",
                L"Size: - mm",
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                0, 0, 10, 10,
                hwnd,
                NULL,
                NULL,
                NULL
            );
             SendMessage(app->ui.hRobotSizeLabel, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);
             updateRobotSizeLabel(app);

             // Robot size slider: 0.25 - 1.0 (see ROBOT_SCALE_MIN/MAX in
             // config.h), mapped to an integer trackbar range of 25-100
             // (WM_HSCROLL below divides the position back down by 100).
             // Starts at 50 (scale 0.5), matching graphics.c's
             // g_robotScale default. Label shows the live value (see
             // WM_HSCROLL below), matching the ArcSpline panel's
             // "Thickness: N px" label (ui.c).
             app->ui.hScaleLabel = CreateWindow(
                L"STATIC",
                L"Scale: 0.50",
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                0, 0, 10, 10,
                hwnd,
                NULL,
                NULL,
                NULL
            );
             SendMessage(app->ui.hScaleLabel, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             app->ui.hScaleSlider = CreateWindow(
                TRACKBAR_CLASS,
                L"",
                WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_SCALE_SLIDER,
                NULL,
                NULL
            );

             SendMessage(app->ui.hScaleSlider, TBM_SETRANGE, TRUE, MAKELONG(25, 100));
             SendMessage(app->ui.hScaleSlider, TBM_SETPOS, TRUE, 50);

             // View Segments toggle: every curve on Semni (seam/thigh/shin
             // arcs) is a trimmed segment of some circle -- this reveals
             // the full circle behind each one. BS_AUTOCHECKBOX |
             // BS_PUSHLIKE gives it the same "stays visually pressed while
             // checked" look as the ArcSpline canvas's own View Segments
             // button (ui.c), and the button IS the toggle state (read via
             // BM_GETCHECK in WM_COMMAND below) rather than a separately
             // tracked bool driving it.
             app->ui.hViewSegmentsButton = CreateWindow(
                L"BUTTON",
                L"View Segments",
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_VIEW_SEGMENTS_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hViewSegmentsButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             // Debug Log: dumps the current robot pose as app_init.c-style
             // assignments to the console (printRobotAsInit, robot.c) on
             // demand -- replaces the old "print every 1s while the mouse
             // moves" background log in WM_MOUSEMOVE, which spammed the
             // console constantly whether you wanted a dump or not.
             app->ui.hDebugLogButton = CreateWindow(
                L"BUTTON",
                L"Debug Log",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_DEBUG_LOG_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hDebugLogButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             // Small backdrop behind the bottom-left hover label so the
             // status text stays legible no matter what color the 3D
             // scene behind it happens to be -- created before the label
             // itself so the label draws on top of it.
             app->ui.hHoverPanel = CreateWindowEx(
                WS_EX_STATICEDGE,
                L"SemniControlPanel",
                L"",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                NULL,
                NULL,
                NULL
            );

             // bottom-left hover status label
             app->ui.hHoverLabel = CreateWindow(
                L"STATIC",
                L"",
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                0, 0, 10, 10,
                hwnd,
                NULL,
                NULL,
                NULL
            );
             SendMessage(app->ui.hHoverLabel, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);
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
                case ID_ROBOT_SELECTOR:
                    if (HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        int sel = (int)SendMessage(app->ui.hRobotSelector, CB_GETCURSEL, 0, 0);
                        if (sel >= 0 && sel < ROBOT_KIND_COUNT)
                            app->robotScene.activeKind = (RobotKind)sel;

                        // Clear any drag/hover state left over from
                        // whichever robot was active before -- meaningful
                        // for Semni and Rocky's body handle right now (see
                        // WM_LBUTTONDOWN/WM_MOUSEMOVE's own activeKind
                        // guards), but harmless to always reset.
                        app->draggingSeamArc1 = 0;
                        app->draggingSeamArc2 = 0;
                        app->draggingInner = 0;
                        app->draggingKnee = 0;
                        app->draggingThigh1 = 0;
                        app->draggingThigh2 = 0;
                        app->draggingFoot = 0;
                        app->draggingShin1 = 0;
                        app->draggingShin2 = 0;
                        app->draggingRockyBody = 0;
                        app->hoverRockyBody = 0;
                        app->draggingRockyEdge = ROCKY_EDGE_NONE;
                        app->hoverRockyEdge = ROCKY_EDGE_NONE;
                        app->draggingRockyKnee = 0;
                        app->hoverRockyKnee = 0;
                        app->draggingRockyFoot = 0;
                        app->hoverRockyFoot = 0;
                        app->activeHandle = 0;
                        app->hoveredCircleSegment = -1;
                        app->hoveredBodyCircle = -1;
                        SetWindowText(app->ui.hHoverLabel, L"");

                        // Switching kind swaps in a differently-sized
                        // robot entirely -- refresh the mm readout to match.
                        updateRobotSizeLabel(app);

                        SetFocus(app->hwndMain);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;

                case ID_MIRROR_LEG_BUTTON:
                    switch (app->robotScene.activeKind)
                    {
                        case ROBOT_KIND_ROCKY:
                            mirrorRockyLeg(&app->robotScene.rocky);
                            break;

                        case ROBOT_KIND_STILO:
                            mirrorStiloLeg(&app->robotScene.stilo);
                            break;

                        case ROBOT_KIND_SEMNI:
                        default:
                            mirrorHipLeg(&app->robotScene.robot);
                            break;
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    SetFocus(app->hwndMain);  // return focus for keyboard input
                    break;

                case ID_MIRROR_LEG2_BUTTON:
                    // Only meaningful for Stilo (its leg 2, see app.h's
                    // Stilo comment) -- harmlessly inert for Semni/Rocky,
                    // same convention as ID_VIEW_SEGMENTS_BUTTON being inert
                    // outside Semni, rather than hiding/disabling the
                    // button per robot kind (see app.h's
                    // ID_MIRROR_LEG2_BUTTON comment).
                    if (app->robotScene.activeKind == ROBOT_KIND_STILO)
                    {
                        mirrorStiloLeg2(&app->robotScene.stilo);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    SetFocus(app->hwndMain);  // return focus for keyboard input
                    break;

                case ID_STANDING_POSITION_BUTTON:
				    // Prefer a user-saved custom pose (see ID_SET_STANDING_
				    // BUTTON below) if one exists on disk for whichever
				    // robot is active -- only falls back to the hardcoded
				    // app_init.c default when no such file has ever been
				    // saved (loadXPoseFromFile returns 0 and leaves the
				    // robot untouched in that case, so the init* call below
				    // is what actually applies the default pose).
				    switch (app->robotScene.activeKind)
				    {
				        case ROBOT_KIND_ROCKY:
				            if (!loadRockyPoseFromFile("rocky_standing.txt", &app->robotScene.rocky))
				                initRockyStandingPosition(app);
				            break;

				        case ROBOT_KIND_STILO:
				            // Stilo's struct just grew a knee stage
				            // (kneeCircle/kneeRadius/kneeAngle) and its shin
				            // arcs this session -- an OLDER stilo_standing.txt
				            // saved before that (back when Stilo went straight
				            // from hip to foot, no knee at all) has no
				            // KNEE_*/SHIN_ARC*/THIGH_ARC* keys, so loading it
				            // alone would leave those fields at whatever they
				            // were before this call (zero, on a fresh launch)
				            // instead of a real pose -- exactly what causes an
				            // extra hip-sized dot with no visible shin/foot.
				            // Seeding with the hardcoded default FIRST
				            // guarantees every field starts at a real value;
				            // the load on top only overwrites whichever keys
				            // the file actually has.
				            initStiloStandingPosition(app);
				            loadStiloPoseFromFile("stilo_standing.txt", &app->robotScene.stilo);
				            break;

				        case ROBOT_KIND_SEMNI:
				        default:
				            if (!loadRobotPoseFromFile("semni_standing.txt", &app->robotScene.robot))
				                initStandingPosition(app);
				            break;
				    }
				    InvalidateRect(hwnd, NULL, FALSE);
				    SetFocus(app->hwndMain);
				    break;

				case ID_HOME_POSITION_BUTTON:
				    // Same "custom file first, hardcoded default as
				    // fallback" pattern as Standing above.
				    switch (app->robotScene.activeKind)
				    {
				        case ROBOT_KIND_ROCKY:
				            if (!loadRockyPoseFromFile("rocky_home.txt", &app->robotScene.rocky))
				                initRockyHomePosition(app);
				            break;

				        case ROBOT_KIND_STILO:
				            // same "seed the full default first, then load
				            // on top" fix as Standing above, for the same
				            // reason (an older stilo_home.txt predating
				            // Stilo's knee stage is missing those keys).
				            initStiloHomePosition(app);
				            loadStiloPoseFromFile("stilo_home.txt", &app->robotScene.stilo);
				            break;

				        case ROBOT_KIND_SEMNI:
				        default:
				            if (!loadRobotPoseFromFile("semni_home.txt", &app->robotScene.robot))
				                initHomePosition(app);
				            break;
				    }
				    InvalidateRect(hwnd, NULL, FALSE);
				    SetFocus(app->hwndMain);
				    break;

				case ID_SET_STANDING_BUTTON:
				    // Captures the CURRENT pose (not the default -- whatever
				    // is on screen right now) as the new Standing target for
				    // whichever robot is active, persisted to its own file
				    // so ID_STANDING_POSITION_BUTTON above picks it up from
				    // now on, including after restarting the app.
				    switch (app->robotScene.activeKind)
				    {
				        case ROBOT_KIND_ROCKY:
				            saveRockyAsEquations("rocky_standing.txt", app);
				            break;

				        case ROBOT_KIND_STILO:
				            saveStiloAsEquations("stilo_standing.txt", app);
				            break;

				        case ROBOT_KIND_SEMNI:
				        default:
				            saveRobotAsEquations("semni_standing.txt", app);
				            break;
				    }
				    SetFocus(app->hwndMain);
				    break;

				case ID_SET_HOME_BUTTON:
				    // Same idea as Set Standing above, for Home.
				    switch (app->robotScene.activeKind)
				    {
				        case ROBOT_KIND_ROCKY:
				            saveRockyAsEquations("rocky_home.txt", app);
				            break;

				        case ROBOT_KIND_STILO:
				            saveStiloAsEquations("stilo_home.txt", app);
				            break;

				        case ROBOT_KIND_SEMNI:
				        default:
				            saveRobotAsEquations("semni_home.txt", app);
				            break;
				    }
				    SetFocus(app->hwndMain);
				    break;

                case ID_VIEW_SEGMENTS_BUTTON:
                {
                    // BS_AUTOCHECKBOX already flipped its own check state
                    // before this notification fires, so read it back
                    // rather than tracking a separate bool -- same pattern
                    // as the ArcSpline canvas's hViewSegBtn (ui.c). Only
                    // has a visible effect while Semni is the active robot
                    // -- Rocky/Stilo don't have a View Segments overlay
                    // yet (see drawRocky/drawStilo's own comments), so the
                    // toggle is harmlessly inert for them right now rather
                    // than disabled outright.
                    BOOL nowChecked = (SendMessage(app->ui.hViewSegmentsButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    app->showCircleSegments = nowChecked;
                    SetFocus(app->hwndMain);
                    break;
                }

                case ID_DEBUG_LOG_BUTTON:
                    // dump the current robot pose as app_init.c-style
                    // assignments, so it can be copied straight in as the
                    // new starting pose once it's been shaped by hand
                    switch (app->robotScene.activeKind)
                    {
                        case ROBOT_KIND_ROCKY:
                            printRockyAsInit(app->robotScene.rocky);
                            break;

                        case ROBOT_KIND_STILO:
                            printStiloAsInit(app->robotScene.stilo);
                            break;

                        case ROBOT_KIND_SEMNI:
                        default:
                            printRobotAsInit(app->robotScene.robot);
                            break;
                    }
                    SetFocus(app->hwndMain);
                    break;
            }
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
