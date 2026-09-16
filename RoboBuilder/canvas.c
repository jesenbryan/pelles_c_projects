#include "canvas.h"
#include "ui.h"            // For SetComparisonModeUI
#include "pipeline.h"      // For RunUploadPipeline
#include "canvas_bridge.h" // For canvasToImage
#include "bmp_ui.h"        // For saveBMP_UI
#include "render.h"        // For renderSegmentsToImage
#include "editor_mode.h"   // For switchEditorMode -- Design Mode > Robot (Semni) launches the robot editor
#include "graphics.h"      // For graphicsOnResize/screenToGL -- renderCombinedFrame draws both subsystems
#include "renderer.h"      // For renderRobotScene -- the Semni half of renderCombinedFrame
#include "robot.h"         // For translateRobot -- dragging the robot into position in Simulation mode
#include "config.h"        // For INACTIVE_MODE_DIM_ALPHA
#include "sim_camera.h"    // Simulation mode's own independent zoom/pan (see sim_camera.h)
#include "save.h"          // For saveCanvasAsBMP/saveRockyAsEquations/saveRockyAsRobArm/saveSemniAsRobLeg/saveStiloAsRobLeg -- File > Save (ID_SAVE below) now dispatches to these directly, folded in from the old Robot editor Save button. saveRobotAsEquations/saveStiloAsEquations are declared here too but no longer called from this file -- they're still used by input.c's Standing/Home pose buttons (Poses\*.txt).
#include "help_dialog.h"   // For showControlsHelpDialog -- Help > Controls Help... (ID_HELP below)
#include "env_export.h"   // For saveEnvironmentSegmentsAsTxt (moved out of this file)
#include "debug.h"        // For ClearConsoleLog -- Simulation's Reset button (ID_RESET_ROBOT below)
#include <math.h>
#include <string.h>
#include <stdlib.h>        // For wcstod (Rocky's Body/Leg Weight edit boxes, read in ID_SAVE below)
#include <wchar.h>
#include <stdio.h>         // For fopen/fprintf/fscanf -- Environment autosave (saveEnvironmentAutosave/loadEnvironmentAutosave below)

HWND hWndGL = NULL;
int glWindowWidth = 800;
int glWindowHeight = 600;
GLuint fontBase = 0;

CanvasState canvas = { .zoom = 1.0f };
AppMode appMode = APP_MODE_DESIGN;

// Starts on Environment (not Robot) so the app opens straight into a
// usable canvas -- Robot is now a real destination (see the ID_LAYER_ROBOT
// handling in WM_COMMAND below), not the placeholder it used to be, and
// selecting it switches the whole app into the Semni robot editor rather
// than just tagging strokes.
DesignLayer designLayer = LAYER_ENVIRONMENT;

// Whether renderCombinedFrame HIDES whichever Design-mode layer (Robot or
// Environment) ISN'T the active one entirely (opacity 0) instead of just
// dimming it (the normal INACTIVE_MODE_DIM_ALPHA fade) -- see
// renderCombinedFrame's own dimAmount comment. On by default -- toggled
// from the View > Hide Inactive Layer menu item (main.c's buildMainMenu,
// WM_COMMAND's ID_TOGGLE_HIDE_INACTIVE handling below) for anyone who'd
// rather see the inactive layer's normal dim fade instead of it vanishing
// outright. Simulation is unaffected either way -- it already always draws
// both layers at full opacity regardless of this flag (see
// renderCombinedFrame's own simulationActive handling).
BOOL hideInactiveLayer = TRUE;

float segmentPointsWorld[MAX_SEGMENT_POINTS * 2];   // NEW
int   segmentStarts[MAX_ARC_SEGMENTS];              // NEW
int   segmentCounts[MAX_ARC_SEGMENTS];              // NEW

float segmentCircleCenterWorld[MAX_ARC_SEGMENTS * 2]; // NEW: ghost circle centers
float segmentCircleRadiusWorld[MAX_ARC_SEGMENTS];     // NEW: ghost circle radii (world X axis)
float segmentCircleRadiusWorldY[MAX_ARC_SEGMENTS];    // NEW: ghost circle radii (world Y axis) -
                                                       // needed because "stretched" (BMP) mode scales
                                                       // x/y independently, so the ghost circle is
                                                       // really an ellipse in world space

// Each reconstructed segment's original stroke radius, in raw SOURCE-IMAGE
// pixels -- the same unit strokeThickness[] above is already in. See
// canvas_bridge.c's setSegmentOverlay for how it's derived (measured from
// the pre-thinning raster, since thinningZhangSuen collapses every stroke
// to a 1px skeleton before arc-fitting ever runs, destroying its width).
// Used by the segment ghost overlay below (with the exact same
// *canvas.zoom/glWindowWidth constant-screen-pixel-width formula real
// strokes use) and render.c's renderSegmentsToImage, so the
// "reconstructed" drawing reads as the same weight as whatever it was fit
// from, instead of one hardcoded flat width regardless of the original.
float segmentAvgRadiusPx[MAX_ARC_SEGMENTS];

// Same original-stroke radius as segmentAvgRadiusPx above, but converted
// into a genuine WORLD-SPACE (EWS) length instead of a raw pixel count --
// see canvas_bridge.c's setSegmentOverlay for the derivation. Used by
// Simulation's ground collision (pointCollidesWithAnyEnvironmentStroke
// below) as EXTRA tolerance on top of the robot's own radius, so a falling
// robot's edge stops at this segment's actual RENDERED surface instead of
// its bare mathematical centerline -- without this, collision and
// rendering silently disagree about where "the ground" actually is by
// roughly this segment's own half-thickness.
float segmentThicknessWorld[MAX_ARC_SEGMENTS];

// NEW: which segment (if any) the mouse is currently hovering over.
// -1 means "none". Declared here (before ResetCanvas) since it's referenced there.
static int hoveredSegment = -1;

// NEW: endpoint-snap - lets the user hover near the start/end point of an
// existing stroke and have the NEXT stroke's start point snap exactly onto
// it, so straight-line strokes can be chained end-to-end into a polyline.
static BOOL  snapEndpointAvailable = FALSE;
static float snapEndpointX = 0.0f, snapEndpointY = 0.0f;

// Simulation mode only: which of the robot's rotatable joints the cursor
// is currently hovering -- a computeSemniBodyCircles (renderer.h) index,
// 2 (hip) or 3 (knee), or -1 for neither. Kept current every WM_MOUSEMOVE
// (see findHoveredJointSim) and read by WM_MOUSEWHEEL to decide whether a
// scroll should rotate that joint instead of zooming the camera.
static int simHoveredJoint = -1;

// NEW: state for the "hover top-right corner to reveal the UI panel" behavior
static BOOL hotZoneHighlighted = FALSE; // cursor is currently inside the corner hot zone
static BOOL uiShown            = FALSE; // panel is at least partially faded in
static int  uiAlpha             = 0;    // current fade alpha, 0 (invisible) - 255 (opaque)

// The Clear/Thickness/Color/Trace/View Segments/Comparison Mode panel
// (hWndUI, ui.c) is Design > Environment-only tooling -- none of it applies
// to Robot design mode or Simulation. WM_TIMER's hot-zone check below
// already refuses to show it outside Design > Environment, but that check
// only runs while canvas.c's own WndProc is actually receiving WM_TIMER --
// which it ISN'T while Robot design mode is active (editorModeState.
// currentMode routes WM_TIMER to input.c's handleInput instead once Robot
// takes over the shared window -- see main.c's WndProcShared). So if the
// panel was already showing right when the user switches to Robot mode (or
// Simulation, which forces ArcSpline to be "active" but still isn't
// Environment), it would otherwise stay stuck on screen indefinitely with
// no WM_TIMER ever ticking to fade it back out. Called directly from the
// mode-switch WM_COMMAND handling below instead, so hiding it doesn't
// depend on the timer being alive at all.
static void HideUIPanelImmediately(void)
{
    if (hWndUI) ShowWindow(hWndUI, SW_HIDE);
    uiShown = FALSE;
    uiAlpha = 0;
    hotZoneHighlighted = FALSE;
}

// Environment autosave (Feature: whatever is drawn in the Environment
// design layer is saved automatically, so closing and reopening the app
// restores the last drawn environment). Distinct from the EnvExport
// folder's Env.txt/Env.bmp files File > Save writes (env_export.c's
// saveEnvironmentSegmentsAsTxt) -- those are a lossy, SCALED arc-fit
// export meant for an external consumer to read, not a faithful
// round-trippable copy of the raw strokes. This instead dumps the exact
// raw stroke data (strokeStarts/strokeThickness/strokeColor/strokeLayer/
// points) needed to reconstruct canvas's drawing state byte-for-byte, in
// canvas.c's own native (unscaled) world units.
//
// Written to Autosave\Environment.txt, its own top-level folder separate
// from EnvExport/RockyExport/etc, so it's obviously not one of the
// exported files meant for another program to read -- it's this app's own
// internal save-state.
#define ENV_AUTOSAVE_FOLDER "Autosave"
#define ENV_AUTOSAVE_PATH   "Autosave\\Environment.txt"

// Writes the CURRENT drawing (every stroke/point in strokeStarts/
// strokeThickness/strokeColor/strokeLayer/points) to ENV_AUTOSAVE_PATH.
// Called automatically whenever the drawing actually changes (WM_LBUTTONUP
// finishing a stroke, ResetCanvas clearing everything) rather than only on
// an explicit File > Save, so the app can restore whatever was last drawn
// even if it's closed without ever touching File > Save.
static void saveEnvironmentAutosave(void)
{
    CreateDirectoryA(ENV_AUTOSAVE_FOLDER, NULL);

    FILE* f = fopen(ENV_AUTOSAVE_PATH, "w");
    if (!f) return; // best-effort -- a failed autosave shouldn't interrupt drawing

    fprintf(f, "%d\n", canvas.strokeCount);
    for (int s = 0; s < canvas.strokeCount; s++)
    {
        fprintf(f, "%d %.6f %lu %d\n", strokeStarts[s], strokeThickness[s],
                (unsigned long)strokeColor[s], (int)strokeLayer[s]);
    }

    fprintf(f, "%d\n", canvas.pointCount);
    for (int i = 0; i < canvas.pointCount; i += 2)
        fprintf(f, "%.6f %.6f\n", points[i], points[i + 1]);

    fclose(f);
}

// Restores whatever saveEnvironmentAutosave last wrote, straight into the
// same strokeStarts/strokeThickness/strokeColor/strokeLayer/points/canvas
// state it was read from -- called once, from WM_CREATE, right as the app
// starts up. If ENV_AUTOSAVE_PATH doesn't exist yet (first ever launch) or
// is malformed/truncated, this just leaves the canvas in its normal empty
// startup state instead of partially applying a corrupt read -- every
// early-return below happens BEFORE canvas.strokeCount/pointCount are
// ever touched, so a bad file can't leave the canvas in a half-loaded,
// inconsistent state.
static void loadEnvironmentAutosave(void)
{
    FILE* f = fopen(ENV_AUTOSAVE_PATH, "r");
    if (!f) return;

    int strokeCountRead = 0;
    if (fscanf(f, "%d", &strokeCountRead) != 1) { fclose(f); return; }
    if (strokeCountRead < 0) strokeCountRead = 0;
    if (strokeCountRead > MAX_STROKES) strokeCountRead = MAX_STROKES;

    for (int s = 0; s < strokeCountRead; s++)
    {
        int start, layer;
        float thick;
        unsigned long colorVal;
        if (fscanf(f, "%d %f %lu %d", &start, &thick, &colorVal, &layer) != 4) { fclose(f); return; }
        strokeStarts[s] = start;
        strokeThickness[s] = thick;
        strokeColor[s] = (COLORREF)colorVal;
        strokeLayer[s] = (DesignLayer)layer;
    }

    int pointCountRead = 0;
    if (fscanf(f, "%d", &pointCountRead) != 1) { fclose(f); return; }
    if (pointCountRead < 0) pointCountRead = 0;
    if (pointCountRead > MAX_POINTS) pointCountRead = MAX_POINTS;

    for (int i = 0; i < pointCountRead; i += 2)
    {
        float x, y;
        if (fscanf(f, "%f %f", &x, &y) != 2) { fclose(f); return; }
        points[i] = x;
        points[i + 1] = y;
    }

    fclose(f);

    // Only commit the counts once every point/stroke has been read
    // successfully -- see this function's own comment on why every
    // early-return above happens before this line.
    canvas.strokeCount = strokeCountRead;
    canvas.pointCount = pointCountRead;
}

void ResetCanvas(void)
{
    canvas.pointCount = 0;
    canvas.strokeCount = 0;
    canvas.hasBackgroundImage = FALSE;
    canvas.panX = 0.0f;
    canvas.panY = 0.0f;
    canvas.zoom = 1.0f;
    canvas.showSegments = FALSE;        // NEW
    canvas.segmentResultCount = 0;      // NEW
    canvas.comparisonMode = FALSE;      // NEW
    hoveredSegment = -1;                // NEW: avoid a stale highlight index
    snapEndpointAvailable = FALSE;      // NEW: avoid a stale endpoint-snap highlight
    branchMarkerCount = 0;              // NEW: avoid stale branch-point markers
	UpdateProjection();

	// Persist the cleared state too, so hitting Clear right before closing
	// the app is remembered as "empty" on next launch instead of the
	// autosave silently keeping whatever was drawn before the Clear.
	saveEnvironmentAutosave();
}

GLuint canvasTexture = 0;

float bgLeft = -1.0f, bgRight = 1.0f, bgBottom = -1.0f, bgTop = 1.0f; // NEW

float branchMarkersWorld[MAX_BRANCH_MARKERS * 2];
int   branchMarkerCount = 0;

// NEW: pan state
static BOOL panning = FALSE;
static int  panLastX = 0, panLastY = 0;

// Simulation mode: dragging anywhere (instead of drawing a new stroke)
// moves the WHOLE robot rigidly, letting the user set its starting
// position within the environment. Uses screenToGL (graphics.h) rather
// than this file's own canvas.zoom/panX/panY conversion -- the robot is
// rendered through Semni's own, entirely separate zoom/pan (graphics.c's
// g_zoom/g_panX/g_panY), so only screenToGL is guaranteed to agree with
// wherever the robot is actually drawn on screen.
//
// The "am I dragging right now" flag itself lives in app.draggingRobotSim
// (app.h), not as a static here, so renderRobot (renderer.c) can also see
// it to render the robot's outline in blue while a drag is in progress --
// only the drag's own last-position bookkeeping needs to stay local to
// this file.
static float dragRobotLastWX = 0.0f, dragRobotLastWY = 0.0f;

// app.hoveringRobotSim (app.h) tracks whether the cursor is over the
// robot's body in Simulation mode -- updated every WM_MOUSEMOVE below (the
// only place the current cursor position is known) and read back by
// WM_SETCURSOR (whose lParam/wParam don't carry a position) to only show
// the move cursor, and by WM_LBUTTONDOWN to only start a whole-robot drag,
// when the mouse is genuinely over the robot rather than anywhere on the
// canvas. Lives on app (like draggingRobotSim above) rather than as a
// local static here, so renderRobot (renderer.c) can also read it to
// highlight the robot yellow on hover.

// NEW: shift-line "hold still to snap" state - lets the user snap to a
// horizontal / vertical / 45-degree diagonal line just by holding the
// cursor still for a couple of seconds, without needing to also hold Ctrl
// (see the instant Ctrl+Shift snap in WM_MOUSEMOVE for the immediate
// version). A truly stationary cursor never generates WM_MOUSEMOVE
// messages, so the dwell check itself has to live in WM_TIMER, which is
// what actually notices time passing.
#define SHIFT_HOLD_SNAP_MS       700    // how long to hold still before it snaps
#define SHIFT_HOLD_TOLERANCE_DEG 5.0f   // how close to 0/45/90/135 deg counts as "close enough"
#define SHIFT_HOLD_JITTER_PX     3      // cursor motion below this still counts as "holding still"

static BOOL  shiftHoldActive       = FALSE; // TRUE while a shift-line drag is in progress
static BOOL  shiftHoldSnapped      = FALSE; // TRUE once the dwell timer has locked the angle
static int   shiftHoldStrokeStart  = 0;     // points[] index of this stroke's first (x,y)
static int   shiftHoldPixelX       = 0;     // last raw cursor position (screen px)
static int   shiftHoldPixelY       = 0;
static float shiftHoldWorldX       = 0.0f;  // last raw cursor position (world), pre-snap
static float shiftHoldWorldY       = 0.0f;
static DWORD shiftHoldLastMoveTick = 0;     // GetTickCount() at the last meaningful cursor move
static float shiftHoldSnapAngleRad = 0.0f;  // locked angle, valid only if shiftHoldSnapped

// Snapshot of the in-progress stroke's freehand points from the instant
// Shift was first pressed this drag (see updateDrawingPoint) -- Shift
// truncates points[] down to just the stroke's start point and re-derives
// a straight line from there every move, which would otherwise permanently
// throw away whatever freehand path came before Shift was pressed.
// Restored verbatim the moment Shift is released, so releasing it goes
// back to exactly what was drawn before, not wherever the straight line
// happened to end up. Sized like points[] itself since the in-progress
// stroke could in principle be almost that long.
static float shiftHoldFreehandBackup[MAX_POINTS];
static int   shiftHoldFreehandBackupCount = 0;

float points[MAX_POINTS];
int strokeStarts[MAX_STROKES];
float strokeThickness[MAX_STROKES];
COLORREF strokeColor[MAX_STROKES];
DesignLayer strokeLayer[MAX_STROKES];
BOOL drawing = FALSE;

static HGLRC hRC;
static HDC hDC;

// "Slow Motion" toggle (see hSlowMotionBtn/ID_SLOW_MOTION, WM_CREATE/
// WM_COMMAND below) -- 1.0 is normal speed, SIMULATION_SLOW_MOTION_SCALE
// (config.h) while checked. Read by advanceAutoGravity (scales the elapsed
// real time physics integrates over) and applyGravityStep's slope-
// alignment step, so both the fall and the tipping-into-a-slope settle
// down together by the same factor, without touching the underlying
// acceleration/terminal velocity/alignment-rate constants themselves.
// Declared up here (rather than next to autoGravityActive further down,
// where it's toggled) because applyGravityStep -- defined earlier in this
// file than that -- already needs to read it.
static float simTimeScale = 1.0f;

// The Simulation-mode-only "Slow Motion" button itself -- a plain WS_CHILD
// of hWndGL (unlike hWndUI's buttons, which live in their own floating
// hover-reveal popup that's Design > Environment-only and explicitly
// hidden the instant Simulation is entered, see HideUIPanelImmediately).
// Created once in WM_CREATE, shown/hidden alongside entering/leaving
// Simulation mode in WM_COMMAND's ID_MODE_SIMULATION handling. Anchored to
// the client area's top-left corner, which needs no WM_SIZE repositioning
// since that corner's offset from the origin never changes on resize.
static HWND hSlowMotionBtn = NULL;

// Same idea as hSlowMotionBtn just above, for the "Walk" toggle (Shift+W --
// see gaitActive/advanceGait). Positioned directly below Slow Motion's
// button (see WM_CREATE), shown/hidden alongside it.
static HWND hWalkBtn = NULL;

// "Reset" -- a plain (non-toggle) push button directly below Walk, same
// hidden-until-Simulation treatment. Restores whichever robot was active
// back to the exact pose (position/angle/kneeAngle/etc -- see
// simulationStartSnapshot below) it had at the MOMENT Simulation mode was
// entered, not some separate hardcoded "factory" pose -- "starting
// position" means the run's own starting point, the same way a game level
// restart returns you to where that attempt began, not to a fixed origin.
static HWND hResetBtn = NULL;

// Whole-RobotScene snapshot (Semni + Rocky + Stilo + activeKind, plain
// value struct -- see app.h -- so a raw copy is safe, no pointers inside
// any of the three) taken once, right when Simulation mode is entered
// (WM_COMMAND's ID_MODE_SIMULATION handling below). ID_RESET_ROBOT copies
// it straight back over app.robotScene. haveSimulationStartSnapshot exists
// so Reset can no-op safely if it's ever somehow clicked before that
// capture has happened (shouldn't be reachable -- the button stays
// hidden until Simulation mode is entered, which is exactly when the
// capture runs -- but a silent no-op is a lot safer than restoring
// whatever garbage a zero-initialized RobotScene would contain).
static RobotScene simulationStartSnapshot;
static BOOL haveSimulationStartSnapshot = FALSE;

HDC canvasGetHDC(void)
{
    return hDC;
}

static void segmentGhostColor(int index, float* r, float* g, float* b)
{
    // Cycle through a handful of distinguishable hues per segment
    static const float palette[6][3] = {
        {0.85f, 0.20f, 0.20f}, {0.20f, 0.55f, 0.85f}, {0.20f, 0.75f, 0.35f},
        {0.85f, 0.55f, 0.15f}, {0.60f, 0.30f, 0.80f}, {0.20f, 0.75f, 0.75f}
    };
    int i = index % 6;
    *r = palette[i][0];
    *g = palette[i][1];
    *b = palette[i][2];
}

static void drawMarkerDisc(float cx, float cy, float r, float red, float green, float blue, float alpha)
{
    const int segments = 20;
    glColor4f(red, green, blue, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float theta = (2.0f * 3.14159265f * i) / segments;
        glVertex2f(cx + r * cosf(theta), cy + r * sinf(theta));
    }
    glEnd();
}

// Distance from a world-space point to a world-space line segment.
static float distPointToSegment(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax;
    float dy = by - ay;
    float len2 = dx * dx + dy * dy;

    if (len2 < 1e-9f) {
        float ddx = px - ax, ddy = py - ay;
        return sqrtf(ddx * ddx + ddy * ddy);
    }

    float t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float projX = ax + t * dx;
    float projY = ay + t * dy;
    float ddx = px - projX, ddy = py - projY;
    return sqrtf(ddx * ddx + ddy * ddy);
}

// Same projection math as distPointToSegment above, but also hands back
// WHERE on the segment the closest point actually is -- needed wherever a
// caller wants to draw a marker at the contact point, not just test a
// distance.
static float closestPointOnSegment(float px, float py, float ax, float ay, float bx, float by,
                                    float* outX, float* outY, float* outT)
{
    float dx = bx - ax;
    float dy = by - ay;
    float len2 = dx * dx + dy * dy;

    float t;
    if (len2 < 1e-9f) {
        t = 0.0f;
    } else {
        t = ((px - ax) * dx + (py - ay) * dy) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }

    float projX = ax + t * dx;
    float projY = ay + t * dy;
    if (outX) *outX = projX;
    if (outY) *outY = projY;
    if (outT) *outT = t;

    float ddx = px - projX, ddy = py - projY;
    return sqrtf(ddx * ddx + ddy * ddy);
}

// TRUE if segment (p1x,p1y)-(p2x,p2y) properly crosses segment
// (p3x,p3y)-(p4x,p4y) in their interiors. Boundary-touching cases
// (collinear overlap, an endpoint exactly on the other segment) are
// deliberately left uncaught here -- they're already correctly picked up
// by distSegmentToSegment's endpoint-distance fallback below (the
// distance comes out ~0 either way), and a general-purpose intersection
// routine would need to special-case several degenerate configurations
// this collision system never actually produces.
static BOOL segmentsProperlyIntersect(float p1x, float p1y, float p2x, float p2y,
                                       float p3x, float p3y, float p4x, float p4y)
{
    float d1 = (p4x - p3x) * (p1y - p3y) - (p4y - p3y) * (p1x - p3x);
    float d2 = (p4x - p3x) * (p2y - p3y) - (p4y - p3y) * (p2x - p3x);
    float d3 = (p2x - p1x) * (p3y - p1y) - (p2y - p1y) * (p3x - p1x);
    float d4 = (p2x - p1x) * (p4y - p1y) - (p2y - p1y) * (p4x - p1x);

    return ((d1 > 0.0f) != (d2 > 0.0f)) && ((d3 > 0.0f) != (d4 > 0.0f));
}

// Exact (no sampling/discretization) minimum distance between segment A
// (ax0,ay0)-(ax1,ay1) and segment B (bx0,by0)-(bx1,by1), with the closest
// point ON SEGMENT A written to *outAX/*outAY. Two non-crossing 2D
// segments always have their true closest point pair at one segment's
// ENDPOINT against the other -- checking all 4 endpoint-vs-opposite-
// segment distances and keeping the smallest is exact, not an
// approximation. segmentsProperlyIntersect above covers the one case
// that check alone would miss: segments crossing cleanly through each
// other's interior, where the true distance is 0 but neither segment has
// an endpoint anywhere near the crossing (exactly the case a fast-moving
// edge sweeping through a thin line in one step would hit -- this is
// what let robotCollidesWithEnvironment's Rocky rectangle-edge test go
// straight through a thin ground line between two discrete point samples
// before this function replaced that sampling loop entirely).
static float distSegmentToSegment(float ax0, float ay0, float ax1, float ay1,
                                   float bx0, float by0, float bx1, float by1,
                                   float* outAX, float* outAY, float* outAT)
{
    if (segmentsProperlyIntersect(ax0, ay0, ax1, ay1, bx0, by0, bx1, by1))
    {
        // The exact crossing point isn't worth solving for here -- for a
        // visual marker, the two segments are already essentially on top
        // of each other, so segment A's own midpoint reads as "right at
        // the contact" closely enough.
        if (outAX) *outAX = (ax0 + ax1) * 0.5f;
        if (outAY) *outAY = (ay0 + ay1) * 0.5f;
        if (outAT) *outAT = 0.5f;
        return 0.0f;
    }

    float bestDist, bestX, bestY, bestT;

    // A's own endpoints against segment B -- the closest point ON A is
    // trivially the endpoint itself (t=0 or t=1).
    bestDist = distPointToSegment(ax0, ay0, bx0, by0, bx1, by1);
    bestX = ax0; bestY = ay0; bestT = 0.0f;

    {
        float d = distPointToSegment(ax1, ay1, bx0, by0, bx1, by1);
        if (d < bestDist) { bestDist = d; bestX = ax1; bestY = ay1; bestT = 1.0f; }
    }

    // B's endpoints against segment A -- the closest point ON A here is
    // wherever that endpoint projects onto A, t included.
    {
        float projX, projY, projT;
        float d = closestPointOnSegment(bx0, by0, ax0, ay0, ax1, ay1, &projX, &projY, &projT);
        if (d < bestDist) { bestDist = d; bestX = projX; bestY = projY; bestT = projT; }
    }
    {
        float projX, projY, projT;
        float d = closestPointOnSegment(bx1, by1, ax0, ay0, ax1, ay1, &projX, &projY, &projT);
        if (d < bestDist) { bestDist = d; bestX = projX; bestY = projY; bestT = projT; }
    }

    if (outAX) *outAX = bestX;
    if (outAY) *outAY = bestY;
    if (outAT) *outAT = bestT;
    return bestDist;
}

// Converts a point in the robot's own Simulation-mode world space into the
// ArcSpline environment's Simulation-mode world space. The robot's joints
// (Semni's own coordinate system, see graphics.c) and the environment's
// traced strokes (this file's points[], canvas.zoom/panX/panY's own
// coordinate system) are two completely different, unrelated numeric
// spaces -- they only visually line up on screen because Simulation mode
// drives both subsystems' projections from the SAME sim_camera zoom/pan
// (see UpdateProjection and graphics.c's applyProjection). Both
// projections ultimately map their own world space to the same shared NDC
// (-1..1) before OpenGL ever gets involved, so routing a point through
// that shared NDC -- world(robot) -> ndc -> world(env) -- is what lets
// gravity's ground-collision check (below) compare the robot's body
// circles against the environment's strokes directly, in one consistent
// frame, instead of comparing two unrelated sets of raw numbers.
static void robotPointToEnvWorld(float rx, float ry, float* ex, float* ey)
{
    float aspect = (float)glWindowWidth / (float)glWindowHeight;

    // Robot's own current half-extent + pan, computed exactly the way
    // graphics.c's applyProjection/screenToGL do (including the robot-size
    // slider, graphicsGetRobotScale) -- has to match or the converted
    // point won't actually correspond to where the robot is drawn.
    float robotZoom = simCameraGetZoom() * graphicsGetRobotScale();
    float robotHalfY = ROBOT_VIEW_HALF_EXTENT / robotZoom;
    float robotHalfX = robotHalfY * aspect;
    float robotPanX, robotPanY;
    simCameraGetWorldPan(robotHalfX, robotHalfY, &robotPanX, &robotPanY);

    // Environment's own current half-extent + pan, computed exactly the
    // way UpdateProjection above does.
    float envZoom = 1.0f / simCameraGetZoom();
    float envHalfX, envHalfY;
    if (aspect >= 1.0f) { envHalfX = aspect * envZoom; envHalfY = envZoom; }
    else                { envHalfX = envZoom; envHalfY = envZoom / aspect; }
    float envPanX, envPanY;
    simCameraGetWorldPan(envHalfX, envHalfY, &envPanX, &envPanY);

    float ndcX = (rx - robotPanX) / robotHalfX;
    float ndcY = (ry - robotPanY) / robotHalfY;

    *ex = ndcX * envHalfX + envPanX;
    *ey = ndcY * envHalfY + envPanY;
}

// Same idea as robotPointToEnvWorld, but for a LENGTH (e.g. a circle's
// radius) rather than a point -- pan cancels out for a pure length/vector
// conversion, so this is just a uniform scale by the ratio of the two
// subsystems' half-extents. Using the Y half-extent specifically is
// arbitrary but safe: both subsystems derive halfX from halfY via the
// exact same `aspect` value (see robotPointToEnvWorld), so the X and Y
// ratios are always identical -- this is a uniform scale, not a stretch.
static float robotLengthToEnvWorld(float rlen)
{
    float aspect = (float)glWindowWidth / (float)glWindowHeight;

    float robotZoom = simCameraGetZoom() * graphicsGetRobotScale();
    float robotHalfY = ROBOT_VIEW_HALF_EXTENT / robotZoom;

    float envZoom = 1.0f / simCameraGetZoom();
    float envHalfY = (aspect >= 1.0f) ? envZoom : (envZoom / aspect);

    return rlen * (envHalfY / robotHalfY);
}

// Single source of truth for the environment line's Simulation-mode
// half-width, in env-world-space units -- used by BOTH the renderer (the
// Comparison Mode ribbon and its raw-stroke fallback, canvas.c's main
// paint routine) and collision (pointCollidesWithAnyEnvironmentStroke,
// just below) so the two can never drift apart the way they did before:
// collision was treating the environment as a bare zero-width centerline
// while the ribbon was drawn extending a visible half-width above (and
// below) that same centerline, so even mathematically exact contact at
// the centerline still looked like sinking into the top half of the
// rendered line. Now both sides ask this one function for the same
// answer, so contact always looks flush right where the line's own drawn
// surface actually is.
//
// robotLengthToEnvWorld(SIMULATION_ARC_COLLISION_THICKNESS) is the base
// value (note: this ratio is independent of simCameraGetZoom() -- it
// cancels out algebraically, by design, since neither collision nor this
// line's "true" thickness should change just because the camera zoomed),
// but that alone often comes out under a screen pixel wide, which flickers
// in and out of visibility as sub-pixel rounding shifts with zoom/pan.
// SIM_ENV_LINE_MIN_PIXELS (config.h) is a floor on top of it, converted
// from on-screen pixels into world units at the CURRENT zoom (unlike the
// base value, this floor DOES depend on zoom -- the whole point is a
// constant apparent size on screen), so the line never thins down to an
// invisible sliver at any zoom level.
static float simEnvLineHalfWidthWorld(void)
{
    float fixedHalfWidth = robotLengthToEnvWorld(SIMULATION_ARC_COLLISION_THICKNESS);

    float aspect = (float)glWindowWidth / (float)glWindowHeight;
    float zoom = 1.0f / simCameraGetZoom();
    float envHalfY = (aspect >= 1.0f) ? zoom : (zoom / aspect);
    // glOrtho maps 2*envHalfY world units to glWindowHeight pixels
    // (UpdateProjection) -- so this is world units per on-screen pixel,
    // vertically.
    float worldPerPixelY = (2.0f * envHalfY) / (float)glWindowHeight;
    float minHalfWidth = (SIM_ENV_LINE_MIN_PIXELS * worldPerPixelY) * 0.5f;

    return (fixedHalfWidth > minHalfWidth) ? fixedHalfWidth : minHalfWidth;
}

// TRUE if the env-world-space point (ecx, ecy) comes within eRadius of the
// RECONSTRUCTED (arc-fitted) environment, not the raw hand-drawn strokes --
// segmentPointsWorld/segmentStarts/segmentCounts (canvas_bridge.c's
// setSegmentOverlay), the same poly-line-sampled arc data the Comparison
// Mode/View Segments ghost overlay renders. Each fitted arc is still
// walked as consecutive point-pairs here (distPointToSegment doesn't care
// whether those points came from a raw stroke or a sampled arc), so this
// is a drop-in swap of the DATA SOURCE, not the collision math itself.
//
// Deliberately not filtered by layer -- unlike the raw strokes this used
// to read from, segments have no per-segment layer tag (setSegmentOverlay
// doesn't carry one through), but that's moot in practice: the Robot
// layer can't have strokes drawn into it at all (see WM_LBUTTONDOWN), so
// canvasToImage's rasterization -- and therefore every fitted segment --
// only ever comes from Environment-layer content anyway.
//
// Requires a trace to have actually run first (see the ID_MODE_SIMULATION
// handler in WM_COMMAND, which re-traces automatically on every entry into
// Simulation so this never runs stale or empty against strokes drawn since
// the last manual Trace press). If canvas.segmentResultCount is 0 (nothing
// traceable, or tracing never ran), the loop below just does nothing and
// this returns FALSE -- same as "no strokes" used to behave.
//
// Tests against a small FIXED epsilon on top of eRadius (the CALLER's own
// radius, e.g. the robot's body circle) -- segmentPointsWorld is only the
// fitted arc's bare mathematical CENTERLINE, so some padding is still
// needed here (same reasoning the connecting fillet arcs have their own
// SIMULATION_ARC_COLLISION_THICKNESS pad for: a contact test against a
// bare curve with literally zero thickness would let the robot visibly
// clip through it depending on the local angle of approach).
//
// Previously this added segmentThicknessWorld[s] instead -- the
// RECONSTRUCTION's own measured stroke half-width (canvas_bridge.c's
// avgRadiusPx-derived value, matching what the Comparison Mode ribbon
// renders at). That seemed right at first (matching the VISIBLE line's
// real surface instead of its invisible centerline), but it's actually
// the wrong quantity for physics: it's just however wide the mouse
// happened to be dragged while drawing that stroke, with zero relation to
// the robot's own size. A big, deliberately-thick pen stroke is fine for
// a large robot but reads as an oversized ledge for a small one (Rocky at
// its default size sitting ~18% of its own body height above a normal
// stroke, purely because the stroke was drawn thick relative to Rocky
// specifically) -- and there's no stroke width a user could pick that
// works uniformly across every robot size. Using the SAME small constant
// the arcs already use makes the environment consistently near-centerline
// for collision regardless of pen pressure or robot scale, matching how
// the fillet arcs already treat their own bare curves.
//
// Debug: TRUE suppresses [COLLIDE]/[GRAVITY] prints for the CURRENT
// applyGravityStep call -- set by applyGravityStep itself based on whether
// the robot was already resting last tick, so the console doesn't scroll
// forever while Shift+G keeps re-confirming a robot that already landed.
static BOOL gSuppressGravityDebug = FALSE;

static BOOL pointCollidesWithAnyEnvironmentStroke(float ecx, float ecy, float eRadius)
{
    // combinedRadius = eRadius + the environment line's OWN rendered
    // half-width (simEnvLineHalfWidthWorld, defined just above). This
    // used to be just eRadius alone, on the theory that adding anything
    // else was double-counting the same "bare curve, no radius" padding
    // arcs/edges already get from their caller -- but that reasoning
    // missed that the environment's line is ALSO drawn with its own
    // visible half-width (centerline +/- simEnvLineHalfWidthWorld, see
    // the Comparison Mode ribbon), not as a bare zero-width centerline.
    // Treating the environment as zero-width for collision while
    // rendering it with real width meant a robot could settle exactly at
    // the centerline -- mathematically flush contact -- while still
    // visibly overlapping the top half of the drawn line, since the
    // line's actual top surface sits a half-width ABOVE that centerline.
    // Adding the same half-width back in here (now that render and
    // collision both source it from the one shared function) fixes that:
    // a real body circle passes its own true radius plus this env
    // half-width, and an arc/edge sample adds SIMULATION_ARC_COLLISION_
    // THICKNESS's own padding on top of that same env half-width -- which
    // is correct, not double-counting, because those are two genuinely
    // different drawn thicknesses (the robot's own curve stroke, and the
    // environment's own line stroke) that both have to be accounted for
    // to land contact exactly on the two visible surfaces meeting, not on
    // either one's invisible centerline.
    float envHalfWidth = simEnvLineHalfWidthWorld();

    for (int s = 0; s < canvas.segmentResultCount; s++)
    {
        int start = segmentStarts[s];
        int count = segmentCounts[s];
        if (count < 2) continue;

        float combinedRadius = eRadius + envHalfWidth;

        for (int i = 0; i < count - 1; i++)
        {
            float ax = segmentPointsWorld[(start + i) * 2];
            float ay = segmentPointsWorld[(start + i) * 2 + 1];
            float bx = segmentPointsWorld[(start + (i + 1)) * 2];
            float by = segmentPointsWorld[(start + (i + 1)) * 2 + 1];

            float dist = distPointToSegment(ecx, ecy, ax, ay, bx, by);
            if (dist < combinedRadius)
            {
                if (!gSuppressGravityDebug)
                    printf("[COLLIDE] seg=%d line=(%.5f,%.5f)-(%.5f,%.5f) pt=(%.5f,%.5f) dist=%.5f eRadius=%.5f combined=%.5f\n",
                           s, ax, ay, bx, by, ecx, ecy, dist, eRadius, combinedRadius);
                return TRUE;
            }
        }
    }

    return FALSE;
}

// Debug/indicator ONLY -- never used by anything that actually moves the
// robot (robotCollidesWithEnvironment, dropActiveRobotToRest, and
// resolveUpwardIfPenetrating all keep calling
// pointCollidesWithAnyEnvironmentStroke directly, unpadded, so none of
// gravity/settle's real physics changes here). See
// SIMULATION_CONTACT_VISUAL_TOLERANCE's own comment (config.h) for why a
// perfectly normal resting contact point still needs this extra slop to
// actually show up red instead of reading as a miss.
static float simContactVisualToleranceWorld(void)
{
    return robotLengthToEnvWorld(SIMULATION_CONTACT_VISUAL_TOLERANCE);
}

static BOOL pointVisuallyContactsEnvironmentStroke(float ecx, float ecy, float eRadius)
{
    return pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius + simContactVisualToleranceWorld());
}

// Edge counterpart to pointCollidesWithAnyEnvironmentStroke, for a
// world-space robot EDGE (ax,ay)-(bx,by) rather than a single point --
// used by Rocky's rectangle sides (the actual boundary of a real filled
// shape, same as a circle's own surface, so eRadius is always 0.0f here
// -- see the original per-sample call site's own comment on why edges
// need zero extra robot-side padding). Exact via distSegmentToSegment,
// not sampled: replaces what used to be a per-edge point-sampling loop
// (fixed, then adaptively grown up to a several-thousand-sample safety
// cap -- see git history for that attempt) with ONE closed-form distance
// check per environment piece -- both correct at any zoom (no
// discretization gap for a fast/long edge to slip through) and far
// cheaper (a piece is now tested once instead of up to thousands of
// times). If outT is non-NULL and this returns TRUE, it's filled with
// the parametric position (0=caller's A start, 1=A's end) of the closest
// point ON THE CALLER'S EDGE -- callers that only need yes/no
// (robotCollidesWithEnvironment) pass NULL; callers drawing a contact
// marker (the live/debug dot overlays) use it to place a single dot
// exactly where contact is happening, by re-applying this same t to
// their own robot-space edge endpoints.
static BOOL edgeCollidesWithAnyEnvironmentStroke(float ax, float ay, float bx, float by,
                                                  float eRadius, float* outT)
{
    float envHalfWidth = simEnvLineHalfWidthWorld();
    float combinedRadius = eRadius + envHalfWidth;

    for (int s = 0; s < canvas.segmentResultCount; s++)
    {
        int start = segmentStarts[s];
        int count = segmentCounts[s];
        if (count < 2) continue;

        for (int i = 0; i < count - 1; i++)
        {
            float ex0 = segmentPointsWorld[(start + i) * 2];
            float ey0 = segmentPointsWorld[(start + i) * 2 + 1];
            float ex1 = segmentPointsWorld[(start + (i + 1)) * 2];
            float ey1 = segmentPointsWorld[(start + (i + 1)) * 2 + 1];

            float t;
            float dist = distSegmentToSegment(ax, ay, bx, by, ex0, ey0, ex1, ey1, NULL, NULL, &t);

            if (dist < combinedRadius)
            {
                if (!gSuppressGravityDebug)
                    printf("[COLLIDE] seg=%d line=(%.5f,%.5f)-(%.5f,%.5f) edge=(%.5f,%.5f)-(%.5f,%.5f) dist=%.5f eRadius=%.5f combined=%.5f\n",
                           s, ex0, ey0, ex1, ey1, ax, ay, bx, by, dist, eRadius, combinedRadius);
                // t is the parametric position (0=A's start, 1=A's end)
                // of the closest point ON THE CALLER'S EDGE -- handed
                // back instead of raw env-space coordinates so a caller
                // that wants to draw a marker in ROBOT-space can just
                // re-apply this same t to its own robot-space endpoints
                // (rectEdges[e].start/end) rather than having to invert
                // the robot<->env world mapping.
                if (outT) *outT = t;
                return TRUE;
            }
        }
    }

    return FALSE;
}

// Visual-tolerance counterpart to edgeCollidesWithAnyEnvironmentStroke,
// same reasoning as pointVisuallyContactsEnvironmentStroke just above --
// display-only, never used by anything that actually moves the robot.
static BOOL edgeVisuallyContactsEnvironmentStroke(float ax, float ay, float bx, float by,
                                                   float eRadius, float* outT)
{
    return edgeCollidesWithAnyEnvironmentStroke(ax, ay, bx, by, eRadius + simContactVisualToleranceWorld(), outT);
}

// Debug-only companion to pointCollidesWithAnyEnvironmentStroke: returns
// the raw geometric distance from (ecx, ecy) to the NEAREST environment
// segment's centerline, with no radius/padding applied at all -- unlike
// the real collision check, this doesn't stop early and doesn't care
// about any combinedRadius, it just answers "how far is this point from
// the drawn line." Exists so the [GRAVITY] per-body-part dump below can
// report an actual signed gap (dist - combinedRadius: positive = still
// floating, negative = overlapping, ~0 = flush) for each part instead of
// a bottom_y estimate that can silently go stale if the real collision
// formula changes and this debug math doesn't get updated to match --
// exactly what happened when simEnvLineHalfWidthWorld's env-side padding
// was added to collision but the old bottom_y print was never updated to
// include it, making logs look like there was a mystery gap that was
// really just this print lagging behind the actual math.
static float nearestEnvDistance(float ecx, float ecy)
{
    float best = -1.0f;

    for (int s = 0; s < canvas.segmentResultCount; s++)
    {
        int start = segmentStarts[s];
        int count = segmentCounts[s];
        if (count < 2) continue;

        for (int i = 0; i < count - 1; i++)
        {
            float ax = segmentPointsWorld[(start + i) * 2];
            float ay = segmentPointsWorld[(start + i) * 2 + 1];
            float bx = segmentPointsWorld[(start + (i + 1)) * 2];
            float by = segmentPointsWorld[(start + (i + 1)) * 2 + 1];

            float dist = distPointToSegment(ecx, ecy, ax, ay, bx, by);
            if (best < 0.0f || dist < best)
                best = dist;
        }
    }

    return best; // -1.0f if there are no traced segments at all
}

// Simulation mode's mass-center drop line (renderer.c's
// drawSimulationMassCenterDropLine) needs to know where a straight drop
// from `from` (robot-local space) actually reaches the environment.
// Marches straight down (decreasing robot-local Y, same X) and returns
// the robot-local Y where it makes contact.
//
// Sphere-traces rather than stepping by a fixed increment: nearestEnvDistance
// already returns the TRUE distance from any point to the closest traced
// segment, so nothing anywhere around that point is closer than that --
// advancing by exactly that distance each iteration can never step clean
// over a genuinely thin environment line the way a fixed coarse step
// could (miss it by landing a sample on either side without ever testing
// a point close enough to register), and it still converges in only a
// handful of iterations for the common case of ground not too far below.
// nearestEnvDistance itself works in env-world units, not robot-local
// ones, so each step is converted back via robotLengthToEnvWorld's own
// scale ratio (see its comment -- this mapping is a uniform scale, no
// rotation, so a straight vertical march in robot-local space really
// does correspond to a straight vertical march in env-world space too).
//
// SIM_MASS_CENTER_DROP_MAX_LENGTH/MAX_ITERATIONS (config.h) bound the
// search so a robot with nothing drawn underneath it doesn't search
// forever -- it just draws a line of that capped length instead.
static float simFindGroundBelowRobotPoint(PointF from)
{
    float envScale = robotLengthToEnvWorld(1.0f); // env-world units per one robot-local unit
    if (envScale <= 0.0f)
        return from.y - SIM_MASS_CENTER_DROP_MAX_LENGTH; // defensive -- zoom/scale should never be <= 0

    float touchThreshold = simEnvLineHalfWidthWorld();
    float traveled = 0.0f;

    for (int i = 0; i < SIM_MASS_CENTER_DROP_MAX_ITERATIONS; i++)
    {
        float y = from.y - traveled;
        float ecx, ecy;
        robotPointToEnvWorld(from.x, y, &ecx, &ecy);

        float envDist = nearestEnvDistance(ecx, ecy);
        if (envDist < 0.0f)
            break; // nothing traced in the environment at all -- fall back below

        if (envDist <= touchThreshold)
            return y; // close enough to call it contact

        traveled += envDist / envScale;
        if (traveled >= SIM_MASS_CENTER_DROP_MAX_LENGTH)
            break;
    }

    return from.y - SIM_MASS_CENTER_DROP_MAX_LENGTH;
}

// Rocky's knee/foot circles used to be tested as a single analytic point
// (the circle's own center, radius folded into the collision padding) --
// exact for a bare yes/no, but for DRAWING it meant synthesizing one
// "nearest point" direction and hoping it was the only contact. That
// broke down for exactly the case a resting/settled robot actually hits:
// the knee AND foot circles can BOTH be genuinely touching the ground at
// once, and picking a single global "closest point" per circle has no
// way to represent two separate circles' contacts independently going
// missing from view, nor a single circle touching in two places (a
// notch/corner).
//
// Used ONLY for the illustrative debug overlay (drawSimulationContactDebug,
// the 'C'-toggle one) -- samples the circle's own circumference, exactly
// like Rocky's shin fillet arcs already do (computeRockyArcPoints/
// ARC_SAMPLE_COUNT), purely so that overlay can show a green/red ring the
// same way it already shows arcs and rectangle-edge samples. This is
// NOT used for the real-time indicator (drawSimulationLiveContactDots,
// see findCircleContactDirections below for that) -- a fixed 41-point
// ring has the exact same coarse-discretization problem the rectangle
// edges originally had (see edgeCollidesWithAnyEnvironmentStroke's own
// comment): a genuinely tangent contact point can fall in the gap
// between two adjacent samples, at high zoom, with EVERY sample reading
// as "too far," which is why callers must pad each sample's own test
// with eArcThickness (the same forgiving stand-in thickness arcs already
// use for exactly this reason), not test them at zero radius.
static void computeCircleSamplePoints(PointF center, float radius, PointF outPts[ARC_SAMPLE_COUNT])
{
    for (int i = 0; i < ARC_SAMPLE_COUNT; i++)
    {
        float theta = (2.0f * 3.14159265f * i) / ARC_SAMPLE_COUNT;
        outPts[i].x = center.x + radius * cosf(theta);
        outPts[i].y = center.y + radius * sinf(theta);
    }
}

// Max distinct simultaneous contact points findCircleContactDirections
// (below) will ever report for one circle -- a robot's leg circle only
// realistically touches a small handful of separate places at once (a
// V-shaped notch is 2; more than that would mean an extremely jagged
// hand-drawn environment), so this is a generous cap, not a tuned limit.
#define MAX_CIRCLE_CONTACT_POINTS 4

// EXACT (no sampling, no discretization gap at any zoom) counterpart to
// computeCircleSamplePoints, used by the always-on live-dot renderer
// (drawSimulationLiveContactDots) where reliability matters more than
// illustrative detail. Walks every environment piece directly and groups
// consecutive in-range pieces (within combinedRadius -- eRadius here is
// the circle's OWN real radius, same padding formula
// pointCollidesWithAnyEnvironmentStroke and its visual-tolerance wrapper
// use) into one "run" per stroke -- an ORDINARY single contact against a
// long or gently curved line, in-range across many consecutive pieces,
// still collapses to exactly one reported point (a run only ends when a
// piece goes OUT of range, or the pieces belong to a different traced
// stroke); only a genuinely separate contact area starts a second run.
// Each run contributes its own single closest point. Returns the number
// of contacts found (0 if none), filling up to maxContacts slots in
// outDirX/outDirY with unit directions from (ecx,ecy) toward each
// contact point, so the caller can place each dot by re-applying that
// direction, scaled by the shape's OWN robot-space radius, to its own
// robot-space center -- same reasoning already used for the rectangle
// edges' parametric t.
static int findCircleContactDirections(float ecx, float ecy, float eRadius,
                                        float outDirX[], float outDirY[], int maxContacts)
{
    float combinedRadius = eRadius + simContactVisualToleranceWorld() + simEnvLineHalfWidthWorld();
    int count = 0;

    for (int s = 0; s < canvas.segmentResultCount && count < maxContacts; s++)
    {
        int start = segmentStarts[s];
        int pieceCount = segmentCounts[s];
        if (pieceCount < 2) continue;

        BOOL inRun = FALSE;
        float bestDist = 0.0f, bestX = 0.0f, bestY = 0.0f;

        for (int i = 0; i < pieceCount - 1; i++)
        {
            float ax = segmentPointsWorld[(start + i) * 2];
            float ay = segmentPointsWorld[(start + i) * 2 + 1];
            float bx = segmentPointsWorld[(start + (i + 1)) * 2];
            float by = segmentPointsWorld[(start + (i + 1)) * 2 + 1];

            float px, py;
            float dist = closestPointOnSegment(ecx, ecy, ax, ay, bx, by, &px, &py, NULL);

            if (dist < combinedRadius)
            {
                if (!inRun || dist < bestDist)
                {
                    bestDist = dist;
                    bestX = px;
                    bestY = py;
                }
                inRun = TRUE;
            }
            else if (inRun)
            {
                if (count < maxContacts)
                {
                    float dx = bestX - ecx, dy = bestY - ecy;
                    float len = sqrtf(dx * dx + dy * dy);
                    outDirX[count] = (len > 1e-6f) ? dx / len : 0.0f;
                    outDirY[count] = (len > 1e-6f) ? dy / len : -1.0f;
                    count++;
                }
                inRun = FALSE;
            }
        }

        if (inRun && count < maxContacts)
        {
            float dx = bestX - ecx, dy = bestY - ecy;
            float len = sqrtf(dx * dx + dy * dy);
            outDirX[count] = (len > 1e-6f) ? dx / len : 0.0f;
            outDirY[count] = (len > 1e-6f) ? dy / len : -1.0f;
            count++;
        }
    }

    return count;
}

// TRUE if any of the robot's 5 body circles (head/butt/hip/knee/foot --
// see computeSemniBodyCircles, renderer.h) OR its 6 connecting fillet arcs
// (seam1/2, thigh1/2, shin1/2 -- see computeSemniArcPoints) overlaps any
// Environment-layer stroke, in the robot's CURRENT pose/position. Used by
// the G ("gravity") hotkey below to stop the robot from sinking through
// whatever ground the user has drawn. The arcs are sampled as poly-lines
// (computeSemniArcPoints walks the exact same trimmed curve drawArc
// renders, not the full untrimmed fillet circle -- that would be far too
// generous) and tested with a fixed stand-in thickness, since -- unlike
// the circles -- they're drawn as bare curves with no radius of their own.
// Generalized over all three robot kinds (see app.h's RobotKind) so
// Simulation mode's gravity/collision/drag all act on whichever robot is
// currently active (app.robotScene.activeKind), not just Semni. Every
// call site below always passed app.robotScene.robot -- the live global
// state, never a hypothetical detached copy -- so there was nothing lost
// dropping the parameter and reading app.robotScene directly per kind.
//
// Semni's branch is untouched line-for-line from the original
// single-robot version. Stilo (6 body circles + 6 fillet arcs, same
// "circles + connecting curves" shape as Semni, just a different count)
// follows the identical pattern. Rocky additionally has a rectangular
// torso (no equivalent in Semni/Stilo) with no radius of its own, so its
// 4 edges are sampled the same way the fillet arcs are -- bare outline
// curves padded by SIMULATION_ARC_COLLISION_THICKNESS -- rather than left
// untested, which would let the box visually sink through the ground
// while only the leg's circles were ever checked.
static BOOL robotCollidesWithEnvironment(void)
{
    float eArcThickness = robotLengthToEnvWorld(SIMULATION_ARC_COLLISION_THICKNESS);

    switch (app.robotScene.activeKind)
    {
        case ROBOT_KIND_ROCKY:
        {
            // Testing-only toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
            // ID_ROCKY_TOGGLE_BODY_BUTTON, see app.h's Rocky comment) --
            // a hidden part is dropped out of collision entirely, not
            // just rendering (drawRocky has the matching guard), so
            // gravity/settle behavior for the remaining part alone can
            // actually be tested without the hidden part's own geometry
            // silently still anchoring/blocking it.
            if (!app.robotScene.rocky.legHidden)
            {
                CircleSegment bodyCircles[NUM_ROCKY_BODY_CIRCLES];
                computeRockyBodyCircles(app.robotScene.rocky, bodyCircles);

                for (int c = 0; c < NUM_ROCKY_BODY_CIRCLES; c++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                    float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);

                    if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius))
                        return TRUE;
                }

                PointF arcPts[NUM_ROCKY_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
                int arcCounts[NUM_ROCKY_CIRCLE_SEGMENTS];
                computeRockyArcPoints(app.robotScene.rocky, arcPts, arcCounts);

                // Was a per-SAMPLE-POINT test (each of the ARC_SAMPLE_COUNT
                // points making up the arc's polyline, individually padded
                // by eArcThickness and checked in isolation) -- a real
                // report showed the shin arc visibly resting against the
                // ground with NO contact point marked there at all, because
                // the true touch point fell between two consecutive
                // samples: neither sample's own tiny padded circle happened
                // to overlap the ground line, even though the polyline
                // SEGMENT connecting them clearly did. Exactly the same
                // discretization gap the rectangle edges used to have (see
                // edgeCollidesWithAnyEnvironmentStroke's own comment) --
                // fixed there by testing exact edges instead of sampled
                // points, so it's fixed here the same way: test each
                // consecutive PAIR of arc samples as a line segment via
                // edgeCollidesWithAnyEnvironmentStroke (still padded by
                // eArcThickness, same as before -- the curve still has no
                // real thickness of its own), instead of each sample alone.
                // drawArc renders this exact same polyline, so this now
                // tests precisely what's actually drawn on screen, with no
                // gap for a touch point to hide in between samples.
                for (int a = 0; a < NUM_ROCKY_CIRCLE_SEGMENTS; a++)
                {
                    for (int i = 0; i + 1 < arcCounts[a]; i++)
                    {
                        float ax, ay, bx, by;
                        robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ax, &ay);
                        robotPointToEnvWorld(arcPts[a][i + 1].x, arcPts[a][i + 1].y, &bx, &by);

                        if (edgeCollidesWithAnyEnvironmentStroke(ax, ay, bx, by, eArcThickness, NULL))
                            return TRUE;
                    }
                }
            }

            // Unlike the fillet arcs just above -- bare 1D curves with no
            // inherent thickness of their own, which is why they need
            // eArcThickness as a stand-in -- these rectangle edges are the
            // actual boundary of a real FILLED shape (the torso rect),
            // rendered as literally these same corner points with nothing
            // extra added around them (drawRockyBodyRect uses identical
            // corner construction to computeRockyRectSegments). That
            // makes a rectangle edge point exactly like a circle's own
            // surface -- real geometry that already needs zero extra
            // robot-side slop -- not like a bare curve that needs padding
            // invented for it. Passing eArcThickness here was treating a
            // real edge as if it were a bare curve, which meant collision
            // stopped the rectangle eArcThickness (~0.0004 world units)
            // above the environment's actual rendered surface -- a small
            // but real, constant, structural gap, not a search-precision
            // residual (see gap= in the [GRAVITY] debug dump: edges were
            // converging to exactly eArcThickness above 0, every time).
            // Passing 0.0f instead means combinedRadius inside
            // pointCollidesWithAnyEnvironmentStroke reduces to just
            // simEnvLineHalfWidthWorld() alone -- the same treatment
            // circles already get -- so the rectangle's real edge now
            // settles flush against the environment's real rendered
            // surface, with nothing invented added on either side.
            if (!app.robotScene.rocky.bodyHidden)
            {
                RockyEdgeSegment rectEdges[NUM_ROCKY_RECT_SEGMENTS];
                computeRockyRectSegments(app.robotScene.rocky, rectEdges);

                // Was a per-edge point-sampling loop (ARC_SAMPLE_COUNT,
                // later grown adaptively -- see git history/backups for
                // that attempt's own comment) -- both versions could,
                // even at their densest, only ever narrow the odds of a
                // fast/long edge slipping through a thinned-out collision
                // band between two samples, never fully rule it out, and
                // the adaptive version's sample counts (needed to even
                // approach "fully ruled out" at high sim-camera zoom)
                // made this the single most expensive check in the whole
                // gravity tick -- visible as real lag once the sample
                // count climbed into the hundreds or thousands per edge,
                // computed up to GRAVITY_CONTACT_SEARCH_ITERATIONS times
                // per tick. A straight robot edge and a straight
                // environment line piece are both plain line segments,
                // so their true minimum distance has an exact closed-form
                // answer (edgeCollidesWithAnyEnvironmentStroke) -- zero
                // sampling, zero discretization gap at ANY zoom, and each
                // environment piece is now tested exactly once instead of
                // up to thousands of times.
                for (int e = 0; e < NUM_ROCKY_RECT_SEGMENTS; e++)
                {
                    float esx, esy, eex, eey;
                    robotPointToEnvWorld(rectEdges[e].start.x, rectEdges[e].start.y, &esx, &esy);
                    robotPointToEnvWorld(rectEdges[e].end.x, rectEdges[e].end.y, &eex, &eey);

                    if (edgeCollidesWithAnyEnvironmentStroke(esx, esy, eex, eey, 0.0f, NULL))
                        return TRUE;
                }
            }

            return FALSE;
        }

        case ROBOT_KIND_STILO:
        {
            CircleSegment bodyCircles[NUM_STILO_BODY_CIRCLES];
            computeStiloBodyCircles(app.robotScene.stilo, bodyCircles);

            for (int c = 0; c < NUM_STILO_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);

                if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius))
                    return TRUE;
            }

            PointF arcPts[NUM_STILO_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
            int arcCounts[NUM_STILO_CIRCLE_SEGMENTS];
            computeStiloArcPoints(app.robotScene.stilo, arcPts, arcCounts);

            for (int a = 0; a < NUM_STILO_CIRCLE_SEGMENTS; a++)
            {
                for (int i = 0; i < arcCounts[a]; i++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);

                    if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eArcThickness))
                        return TRUE;
                }
            }

            return FALSE;
        }

        case ROBOT_KIND_SEMNI:
        default:
        {
            CircleSegment bodyCircles[NUM_ROBOT_BODY_CIRCLES];
            computeSemniBodyCircles(app.robotScene.robot, bodyCircles);

            for (int c = 0; c < NUM_ROBOT_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);

                if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius))
                    return TRUE;
            }

            PointF arcPts[NUM_ROBOT_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
            int arcCounts[NUM_ROBOT_CIRCLE_SEGMENTS];
            computeSemniArcPoints(app.robotScene.robot, arcPts, arcCounts);

            for (int a = 0; a < NUM_ROBOT_CIRCLE_SEGMENTS; a++)
            {
                for (int i = 0; i < arcCounts[a]; i++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);

                    if (pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eArcThickness))
                        return TRUE;
                }
            }

            return FALSE;
        }
    }
}

// ---- Simulation "show contact" debug overlay (plain C, no modifier) ----

// On by default (per explicit request -- this used to default off and
// need an opt-in press of C, but it's been useful enough while chasing
// contact bugs that starting Simulation mode without it was just an extra
// step every time). Plain C still toggles it, now OFF instead of on.
// Read by drawSimulationContactDebug, called from renderCombinedFrame
// right after the robot itself is drawn.
static BOOL showContactDebug = TRUE;

// Sanity-check plumbing for showContactDebug: TRUE once this toggle-ON has
// already printed its one-shot point dump (see drawSimulationContactDebug),
// so the console doesn't get a full point dump every single frame (this
// runs from the main render path). Re-armed to FALSE every time 'C' is
// pressed, in WM_KEYDOWN just below.
static BOOL contactDebugLoggedThisToggle = FALSE;

// Draws a small disc at every single point robotCollidesWithEnvironment
// itself tests against the environment -- every body circle, every
// fillet-arc sample point (this includes Rocky's two shin arcs between
// the knee and foot circles, computeRockyArcPoints -- an earlier version
// of this comment claimed those went untested/undrawn, which stopped
// being true once the loop below was added), and (Rocky only) every
// rectangle-edge sample point -- colored red if THAT exact point is
// colliding right now, dim green otherwise. Deliberately mirrors
// robotCollidesWithEnvironment's own per-kind iteration line-for-line
// instead of calling it, since that function early-returns on the first
// hit and this needs every point's own status to draw anything.
//
// Draws in ROBOT-local coordinates (same convention drawRockyMassCenterTrack/
// drawRockyReferencePoint already use), NOT the robotPointToEnvWorld-
// transformed ones -- that transform only exists to ask "does this land on
// an environment stroke," it isn't the space anything is actually drawn
// in. Must run right after renderRobotScene while ITS projection (set up
// by graphicsOnResize just before it) is still bound -- canvasRenderFrame
// rebinds its own projection afterward, which would misplace these dots
// if this ran after that instead.
static void drawSimulationContactDebug(void)
{
    if (!showContactDebug || appMode != APP_MODE_SIMULATION) return;

    // One-shot text dump the first frame after each toggle-ON: exactly
    // how many points this pass actually tested and how many of them are
    // hits right now. Exists so "I didn't see any dots" can be answered
    // from the console alone -- if this never prints, the overlay isn't
    // even being reached (a toggle/appMode/focus problem upstream of
    // drawing); if it prints hits > 0 but nothing is visible on screen,
    // the problem is specifically in the drawing/projection, not in
    // whether contact is being detected.
    BOOL logThisPass = !contactDebugLoggedThisToggle;
    int dbgPointCount = 0, dbgHitCount = 0;
    if (logThisPass) contactDebugLoggedThisToggle = TRUE;

    // This function calls pointCollidesWithAnyEnvironmentStroke up to ~250
    // times EVERY SINGLE FRAME (once per tested point) purely to decide
    // dot color -- that function has its own [COLLIDE] console print
    // built in (meant for real gravity-step landing decisions, gated by
    // gSuppressGravityDebug), which doesn't know the difference between
    // "the robot just landed, this matters" and "the debug overlay is
    // just asking, for the 200th time this second, whether this one arc
    // sample happens to be red or green." With the overlay now on by
    // default, that flooded the console with dozens of [COLLIDE] lines a
    // frame, burying the actually-meaningful [STUCK]/[CONTACT] output.
    // Suppress it for exactly the duration of this function's own point
    // tests, restoring whatever it was set to on the way out so a REAL
    // applyGravityStep call running around this (or after it, same frame)
    // still logs normally.
    BOOL savedSuppressGravityDebug = gSuppressGravityDebug;
    gSuppressGravityDebug = TRUE;

    const float dotRadius = 0.010f;
    const float smallDotRadius = 0.006f;
    float eArcThickness = robotLengthToEnvWorld(SIMULATION_ARC_COLLISION_THICKNESS);

    switch (app.robotScene.activeKind)
    {
        case ROBOT_KIND_ROCKY:
        {
            // Testing-only toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
            // ID_ROCKY_TOGGLE_BODY_BUTTON) -- a hidden part must not be
            // SAMPLED here either, not just excluded from the real
            // robotCollidesWithEnvironment check above: this overlay
            // runs its own completely separate point pass purely to draw
            // per-point hit/miss dots and print pointsTested= in the
            // [CONTACT] dump, so without this guard a hidden leg/body
            // would still show up in that count (and, worse, still draw
            // its dots) even though it's supposedly gone.
            if (!app.robotScene.rocky.legHidden)
            {
                CircleSegment bodyCircles[NUM_ROCKY_BODY_CIRCLES];
                computeRockyBodyCircles(app.robotScene.rocky, bodyCircles);

                for (int c = 0; c < NUM_ROCKY_BODY_CIRCLES; c++)
                {
                    // Sampled around the circle's own circumference --
                    // same idea as the arc points just below and the
                    // rectangle-edge samples further down -- instead of
                    // one analytic center+radius test. Padded with
                    // eArcThickness (NOT zero) for the same reason arcs
                    // need it: a fixed 41-point ring can leave the true
                    // tangent point sitting in the gap between two
                    // samples, and testing at zero radius made a
                    // genuinely-touching circle show no red dot at all
                    // (every sample individually reading "too far," even
                    // though the circle really is resting on the line).
                    // This is what lets BOTH knee and foot show their own
                    // dots when both are genuinely resting on the ground
                    // at once, and lets a single circle resting in a
                    // notch/corner show two separate lit-up clusters
                    // instead of one synthesized "nearest point" that
                    // could only ever pick one.
                    PointF circlePts[ARC_SAMPLE_COUNT];
                    computeCircleSamplePoints(bodyCircles[c].center, bodyCircles[c].radius, circlePts);

                    for (int i = 0; i < ARC_SAMPLE_COUNT; i++)
                    {
                        float ecx, ecy;
                        robotPointToEnvWorld(circlePts[i].x, circlePts[i].y, &ecx, &ecy);
                        BOOL hit = pointVisuallyContactsEnvironmentStroke(ecx, ecy, eArcThickness);
                        drawMarkerDisc(circlePts[i].x, circlePts[i].y, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                       hit ? 0.95f : 0.25f, hit ? 0.15f : 0.85f, hit ? 0.15f : 0.95f, hit ? 1.0f : 0.7f);
                        if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
                    }
                }

                PointF arcPts[NUM_ROCKY_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
                int arcCounts[NUM_ROCKY_CIRCLE_SEGMENTS];
                computeRockyArcPoints(app.robotScene.rocky, arcPts, arcCounts);

                for (int a = 0; a < NUM_ROCKY_CIRCLE_SEGMENTS; a++)
                {
                    for (int i = 0; i < arcCounts[a]; i++)
                    {
                        float ecx, ecy;
                        robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                        BOOL hit = pointVisuallyContactsEnvironmentStroke(ecx, ecy, eArcThickness);
                        drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                       hit ? 0.95f : 0.25f, hit ? 0.15f : 0.85f, hit ? 0.15f : 0.95f, hit ? 1.0f : 0.7f);
                        if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
                    }
                }
            }

            if (!app.robotScene.rocky.bodyHidden)
            {
                RockyEdgeSegment rectEdges[NUM_ROCKY_RECT_SEGMENTS];
                computeRockyRectSegments(app.robotScene.rocky, rectEdges);

                for (int e = 0; e < NUM_ROCKY_RECT_SEGMENTS; e++)
                {
                    for (int i = 0; i < ARC_SAMPLE_COUNT; i++)
                    {
                        float t = (float)i / (float)(ARC_SAMPLE_COUNT - 1);
                        float lx = rectEdges[e].start.x + (rectEdges[e].end.x - rectEdges[e].start.x) * t;
                        float ly = rectEdges[e].start.y + (rectEdges[e].end.y - rectEdges[e].start.y) * t;

                        float ecx, ecy;
                        robotPointToEnvWorld(lx, ly, &ecx, &ecy);
                        BOOL hit = pointVisuallyContactsEnvironmentStroke(ecx, ecy, 0.0f);
                        drawMarkerDisc(lx, ly, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                       hit ? 0.95f : 0.25f, hit ? 0.15f : 0.85f, hit ? 0.15f : 0.95f, hit ? 1.0f : 0.7f);
                        if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
                    }
                }
            }
            break;
        }

        case ROBOT_KIND_STILO:
        {
            CircleSegment bodyCircles[NUM_STILO_BODY_CIRCLES];
            computeStiloBodyCircles(app.robotScene.stilo, bodyCircles);

            for (int c = 0; c < NUM_STILO_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                BOOL hit = pointVisuallyContactsEnvironmentStroke(ecx, ecy, eRadius);
                // Colliding points draw noticeably BIGGER (2.5x), not just
                // a different color -- a same-size color swap on an
                // already-small marker is easy to miss at a glance,
                // especially sitting right at ground contact where the
                // environment's own line is drawn close by. Size is the
                // primary signal here; color is secondary.
                drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, hit ? dotRadius * 2.5f : dotRadius,
                               hit ? 0.95f : 0.25f, hit ? 0.15f : 0.85f, hit ? 0.15f : 0.95f, hit ? 1.0f : 0.9f);
                if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
            }

            PointF arcPts[NUM_STILO_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
            int arcCounts[NUM_STILO_CIRCLE_SEGMENTS];
            computeStiloArcPoints(app.robotScene.stilo, arcPts, arcCounts);

            for (int a = 0; a < NUM_STILO_CIRCLE_SEGMENTS; a++)
            {
                for (int i = 0; i < arcCounts[a]; i++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                    BOOL hit = pointVisuallyContactsEnvironmentStroke(ecx, ecy, eArcThickness);
                    drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                   hit ? 0.95f : 0.25f, hit ? 0.15f : 0.85f, hit ? 0.15f : 0.95f, hit ? 1.0f : 0.7f);
                    if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
                }
            }
            break;
        }

        case ROBOT_KIND_SEMNI:
        default:
        {
            CircleSegment bodyCircles[NUM_ROBOT_BODY_CIRCLES];
            computeSemniBodyCircles(app.robotScene.robot, bodyCircles);

            for (int c = 0; c < NUM_ROBOT_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                BOOL hit = pointVisuallyContactsEnvironmentStroke(ecx, ecy, eRadius);
                // Colliding points draw noticeably BIGGER (2.5x), not just
                // a different color -- a same-size color swap on an
                // already-small marker is easy to miss at a glance,
                // especially sitting right at ground contact where the
                // environment's own line is drawn close by. Size is the
                // primary signal here; color is secondary.
                drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, hit ? dotRadius * 2.5f : dotRadius,
                               hit ? 0.95f : 0.25f, hit ? 0.15f : 0.85f, hit ? 0.15f : 0.95f, hit ? 1.0f : 0.9f);
                if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
            }

            PointF arcPts[NUM_ROBOT_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
            int arcCounts[NUM_ROBOT_CIRCLE_SEGMENTS];
            computeSemniArcPoints(app.robotScene.robot, arcPts, arcCounts);

            for (int a = 0; a < NUM_ROBOT_CIRCLE_SEGMENTS; a++)
            {
                for (int i = 0; i < arcCounts[a]; i++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                    BOOL hit = pointVisuallyContactsEnvironmentStroke(ecx, ecy, eArcThickness);
                    drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, hit ? smallDotRadius * 3.0f : smallDotRadius,
                                   hit ? 0.95f : 0.25f, hit ? 0.15f : 0.85f, hit ? 0.15f : 0.95f, hit ? 1.0f : 0.7f);
                    if (logThisPass) { dbgPointCount++; if (hit) dbgHitCount++; }
                }
            }
            break;
        }
    }

    gSuppressGravityDebug = savedSuppressGravityDebug;

    if (logThisPass)
    {
        printf("[CONTACT] dump: activeKind=%d pointsTested=%d currentlyColliding=%d\n",
               app.robotScene.activeKind, dbgPointCount, dbgHitCount);
    }
}

// Always-on companion to drawSimulationContactDebug above, by explicit
// request: that function only draws anything while the plain-C toggle
// (showContactDebug) is on, and even then draws EVERY sampled point --
// hits big and red, misses small and green -- as a diagnostic overlay of
// the whole collision test. This instead runs unconditionally any time
// we're in Simulation mode, and draws NOTHING for a point that isn't
// currently colliding -- just one small red dot exactly where contact is
// actually happening right now, so a real touch/land/collision always has
// a visible marker without needing to remember to switch on the fuller
// debug view. Live only, by explicit request -- no accumulated trail: a
// dot disappears the instant that point stops colliding, same as any
// other per-frame overlay.
//
// Same per-kind sampling as robotCollidesWithEnvironment/
// drawSimulationContactDebug (body circles, shin/thigh fillet arcs, and
// for Rocky the rectangle edges), including Rocky's own legHidden/
// bodyHidden testing toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
// ID_ROCKY_TOGGLE_BODY_BUTTON) -- a hidden part isn't sampled here
// either, consistent with it not being sampled for real collision or the
// debug overlay.
static void drawSimulationLiveContactDots(void)
{
    if (appMode != APP_MODE_SIMULATION) return;

    // Same suppression as drawSimulationContactDebug's own -- this calls
    // pointCollidesWithAnyEnvironmentStroke up to ~250 times EVERY FRAME
    // purely to decide dot color, and running unconditionally (not just
    // while a debug toggle is on) makes that even more important to keep
    // quiet than the toggle-gated overlay above.
    BOOL savedSuppressGravityDebug = gSuppressGravityDebug;
    gSuppressGravityDebug = TRUE;

    const float liveContactDotRadius = 0.0015f;
    float eArcThickness = robotLengthToEnvWorld(SIMULATION_ARC_COLLISION_THICKNESS);

    switch (app.robotScene.activeKind)
    {
        case ROBOT_KIND_ROCKY:
        {
            if (!app.robotScene.rocky.legHidden)
            {
                CircleSegment bodyCircles[NUM_ROCKY_BODY_CIRCLES];
                computeRockyBodyCircles(app.robotScene.rocky, bodyCircles);

                for (int c = 0; c < NUM_ROCKY_BODY_CIRCLES; c++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                    float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);

                    // EXACT test (findCircleContactDirections), not the
                    // debug overlay's sampled ring above -- this renderer
                    // is always on, so it can't afford the sampled ring's
                    // discretization gap (a fixed 41-point ring can leave
                    // the true tangent point sitting between two samples,
                    // making a genuinely-touching circle show NO dot at
                    // all). Finds every distinct contact this circle
                    // actually has (handles knee+foot both touching at
                    // once, and a single circle touching in two places)
                    // with zero discretization error at any zoom.
                    float contactDirX[MAX_CIRCLE_CONTACT_POINTS], contactDirY[MAX_CIRCLE_CONTACT_POINTS];
                    int contactCount = findCircleContactDirections(ecx, ecy, eRadius, contactDirX, contactDirY, MAX_CIRCLE_CONTACT_POINTS);
                    for (int k = 0; k < contactCount; k++)
                    {
                        float rx = bodyCircles[c].center.x + contactDirX[k] * bodyCircles[c].radius;
                        float ry = bodyCircles[c].center.y + contactDirY[k] * bodyCircles[c].radius;
                        drawMarkerDisc(rx, ry, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                    }
                }

                PointF arcPts[NUM_ROCKY_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
                int arcCounts[NUM_ROCKY_CIRCLE_SEGMENTS];
                computeRockyArcPoints(app.robotScene.rocky, arcPts, arcCounts);

                for (int a = 0; a < NUM_ROCKY_CIRCLE_SEGMENTS; a++)
                {
                    for (int i = 0; i < arcCounts[a]; i++)
                    {
                        float ecx, ecy;
                        robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                        if (pointVisuallyContactsEnvironmentStroke(ecx, ecy, eArcThickness))
                            drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                    }
                }
            }

            if (!app.robotScene.rocky.bodyHidden)
            {
                RockyEdgeSegment rectEdges[NUM_ROCKY_RECT_SEGMENTS];
                computeRockyRectSegments(app.robotScene.rocky, rectEdges);

                // Was a per-edge ARC_SAMPLE_COUNT point-sampling loop --
                // this runs EVERY FRAME, unconditionally, so it was both
                // the main source of the occasional dropped/missing red
                // dot (a thin collision band at high zoom could fall
                // between two fixed samples, same bug as
                // robotCollidesWithEnvironment's own -- see that
                // function's comment) AND, if grown denser to fix that,
                // a real per-frame cost. edgeVisuallyContactsEnvironmentStroke
                // is exact (no sampling at any zoom) and hands back the
                // one true closest point on this edge, so this now draws
                // at most a single, always-correctly-placed dot per edge
                // instead of looping over many candidate points hoping
                // one of them landed close enough.
                for (int e = 0; e < NUM_ROCKY_RECT_SEGMENTS; e++)
                {
                    float esx, esy, eex, eey;
                    robotPointToEnvWorld(rectEdges[e].start.x, rectEdges[e].start.y, &esx, &esy);
                    robotPointToEnvWorld(rectEdges[e].end.x, rectEdges[e].end.y, &eex, &eey);

                    // t comes back as the parametric position (0=start,
                    // 1=end) of the closest point along THIS edge -- the
                    // robot<->env mapping is affine (see
                    // robotPointToEnvWorld's own comment), so re-applying
                    // the same t to the ORIGINAL robot-space endpoints
                    // lands exactly on the matching robot-space point,
                    // with no inverse transform (and no risk of dividing
                    // by a near-zero axis on an axis-aligned edge) needed.
                    float t;
                    if (edgeVisuallyContactsEnvironmentStroke(esx, esy, eex, eey, 0.0f, &t))
                    {
                        float rx = rectEdges[e].start.x + (rectEdges[e].end.x - rectEdges[e].start.x) * t;
                        float ry = rectEdges[e].start.y + (rectEdges[e].end.y - rectEdges[e].start.y) * t;
                        drawMarkerDisc(rx, ry, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                    }
                }
            }
            break;
        }

        case ROBOT_KIND_STILO:
        {
            CircleSegment bodyCircles[NUM_STILO_BODY_CIRCLES];
            computeStiloBodyCircles(app.robotScene.stilo, bodyCircles);

            for (int c = 0; c < NUM_STILO_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                if (pointVisuallyContactsEnvironmentStroke(ecx, ecy, eRadius))
                    drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
            }

            PointF arcPts[NUM_STILO_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
            int arcCounts[NUM_STILO_CIRCLE_SEGMENTS];
            computeStiloArcPoints(app.robotScene.stilo, arcPts, arcCounts);

            for (int a = 0; a < NUM_STILO_CIRCLE_SEGMENTS; a++)
            {
                for (int i = 0; i < arcCounts[a]; i++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                    if (pointVisuallyContactsEnvironmentStroke(ecx, ecy, eArcThickness))
                        drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                }
            }
            break;
        }

        case ROBOT_KIND_SEMNI:
        default:
        {
            CircleSegment bodyCircles[NUM_ROBOT_BODY_CIRCLES];
            computeSemniBodyCircles(app.robotScene.robot, bodyCircles);

            for (int c = 0; c < NUM_ROBOT_BODY_CIRCLES; c++)
            {
                float ecx, ecy;
                robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
                float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);
                if (pointVisuallyContactsEnvironmentStroke(ecx, ecy, eRadius))
                    drawMarkerDisc(bodyCircles[c].center.x, bodyCircles[c].center.y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
            }

            PointF arcPts[NUM_ROBOT_CIRCLE_SEGMENTS][ARC_SAMPLE_COUNT];
            int arcCounts[NUM_ROBOT_CIRCLE_SEGMENTS];
            computeSemniArcPoints(app.robotScene.robot, arcPts, arcCounts);

            for (int a = 0; a < NUM_ROBOT_CIRCLE_SEGMENTS; a++)
            {
                for (int i = 0; i < arcCounts[a]; i++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(arcPts[a][i].x, arcPts[a][i].y, &ecx, &ecy);
                    if (pointVisuallyContactsEnvironmentStroke(ecx, ecy, eArcThickness))
                        drawMarkerDisc(arcPts[a][i].x, arcPts[a][i].y, liveContactDotRadius, 0.95f, 0.1f, 0.1f, 1.0f);
                }
            }
            break;
        }
    }

    gSuppressGravityDebug = savedSuppressGravityDebug;
}

// Rigidly moves whichever robot is currently active (app.robotScene.
// activeKind) by (dx, dy) -- dispatches to translateRobot/translateRocky/
// translateStilo. Used by Simulation mode's gravity search and
// whole-robot drag below, both of which need to move "the robot on
// screen" without caring which kind that happens to be.
static void translateActiveRobot(float dx, float dy)
{
    switch (app.robotScene.activeKind)
    {
        case ROBOT_KIND_ROCKY:
            translateRocky(&app.robotScene.rocky, dx, dy);
            break;

        case ROBOT_KIND_STILO:
            translateStilo(&app.robotScene.stilo, dx, dy);
            break;

        case ROBOT_KIND_SEMNI:
        default:
            translateRobot(&app.robotScene.robot, dx, dy);
            break;
    }
}

// TRUE if (wx, wy) falls inside whichever robot is currently active --
// generalizes isPointInsideRobotBody (Semni-only, robot.c) to all three
// kinds for Simulation mode's hover/drag-start hit-testing. Stilo is pure
// circles + arcs like Semni, so it gets the identical circle-containment
// test via computeStiloBodyCircles.
//
// Rocky's rectangular torso USED to have no interior-point hit-test
// anywhere else in this app (input.c's own hoverRockyBody is a proximity
// check to the body's center handle, HIP_HANDLE_RADIUS, not a true
// rectangle-interior test) -- Simulation mode's whole-body drag matched
// that same convention instead of inventing a different one, which meant
// clicking most of the visible rectangle (anywhere outside a small
// HIP_HANDLE_RADIUS circle at its exact center) didn't start a drag at
// all, a real report of "only some part of it" grabs the body. A real
// rotated-rectangle test fixes that: undo the rectangle's own rotation
// around its center (rotatePoint by -angle, the exact inverse of however
// drawRockyBodyRect/computeRockyRectSegments built it), then it's a plain
// axis-aligned half-width/half-height box test in that unrotated frame.
//
// Both the rectangle test and the leg circles below respect Rocky's own
// legHidden/bodyHidden testing toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
// ID_ROCKY_TOGGLE_BODY_BUTTON) -- a hidden part shouldn't be draggable by
// clicking where it used to be any more than it's still rendered or
// collision-tested there.
static BOOL isPointInsideActiveRobotBody(float wx, float wy)
{
    switch (app.robotScene.activeKind)
    {
        case ROBOT_KIND_ROCKY:
        {
            PointF center = getRockyCenter(app.robotScene.rocky);

            if (!app.robotScene.rocky.bodyHidden)
            {
                float cdx = wx - center.x, cdy = wy - center.y;
                if (sqrtf(cdx * cdx + cdy * cdy) <= HIP_HANDLE_RADIUS)
                    return TRUE;

                PointF local = rotatePoint((PointF){ wx, wy }, center, -app.robotScene.rocky.angle);
                if (fabsf(local.x - center.x) <= app.robotScene.rocky.bodyHalfWidth
                    && fabsf(local.y - center.y) <= app.robotScene.rocky.bodyHalfHeight)
                    return TRUE;
            }

            if (!app.robotScene.rocky.legHidden)
            {
                CircleSegment bodyCircles[NUM_ROCKY_BODY_CIRCLES];
                computeRockyBodyCircles(app.robotScene.rocky, bodyCircles);
                for (int i = 0; i < NUM_ROCKY_BODY_CIRCLES; i++)
                {
                    float dx = wx - bodyCircles[i].center.x;
                    float dy = wy - bodyCircles[i].center.y;
                    if (sqrtf(dx * dx + dy * dy) <= bodyCircles[i].radius)
                        return TRUE;
                }
            }
            return FALSE;
        }

        case ROBOT_KIND_STILO:
        {
            CircleSegment bodyCircles[NUM_STILO_BODY_CIRCLES];
            computeStiloBodyCircles(app.robotScene.stilo, bodyCircles);
            for (int i = 0; i < NUM_STILO_BODY_CIRCLES; i++)
            {
                float dx = wx - bodyCircles[i].center.x;
                float dy = wy - bodyCircles[i].center.y;
                if (sqrtf(dx * dx + dy * dy) <= bodyCircles[i].radius)
                    return TRUE;
            }
            return FALSE;
        }

        case ROBOT_KIND_SEMNI:
        default:
            return isPointInsideRobotBody(app.robotScene.robot, wx, wy);
    }
}

// Simulation mode only: which of the robot's ROTATABLE joints (hip = body
// circle index 2, knee = index 3 -- see computeSemniBodyCircles' documented
// ordering, renderer.h) the given world-space point falls inside, or -1 if
// neither. Head/butt/foot are deliberately excluded -- they have no
// independent rotation angle of their own to spin (foot's position is
// fully determined by kneeCircle + kneeAngle + a fixed local offset, no
// extra degree of freedom there). Feeds WM_MOUSEWHEEL's joint-rotate
// feature below: hovering the hip or knee's own visible body circle and
// scrolling rotates hipAngle/kneeAngle directly, without needing to leave
// Simulation and go pose it in the separate Design > Robot editor.
static int findHoveredJointSim(Semni robot, float wx, float wy)
{
    CircleSegment bodyCircles[NUM_ROBOT_BODY_CIRCLES];
    computeSemniBodyCircles(robot, bodyCircles);

    for (int i = 2; i <= 3; i++)
    {
        float dx = wx - bodyCircles[i].center.x;
        float dy = wy - bodyCircles[i].center.y;
        if (sqrtf(dx * dx + dy * dy) <= bodyCircles[i].radius)
            return i;
    }

    return -1;
}

// Companion to applyGravityStep's downward binary search below, but
// searching UPWARD instead, within a small fixed budget
// (SIMULATION_SLOPE_CORRECTION_MAX) rather than a whole gravity step --
// used right after nudging the robot's angle toward the slope it's resting
// on, since rotating a rigid body around its torso-level center can push a
// limb a hair into the ground even though the body as a whole was already
// settled. No-ops if the robot isn't actually colliding right now.
static void resolveUpwardIfPenetrating(HWND hWnd, float maxCorrection)
{
    if (!robotCollidesWithEnvironment()) return;

    float safe = maxCorrection;    // largest offset assumed to clear
    float blocked = 0.0f;          // smallest offset confirmed NOT to clear

    for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
    {
        float mid = (safe + blocked) * 0.5f;

        translateActiveRobot(0.0f, mid);
        BOOL hit = robotCollidesWithEnvironment();
        translateActiveRobot(0.0f, -mid);

        if (hit) blocked = mid;
        else     safe = mid;
    }

    translateActiveRobot(0.0f, safe);
    InvalidateRect(hWnd, NULL, FALSE);
}

// Drops the whole active robot (translateActiveRobot -- same rigid,
// all-parts-together move applyGravityStep itself uses) straight down
// from wherever it is RIGHT NOW until first contact, in one call --
// unlike applyGravityStep, which is built around one small fixed-size
// SIMULATION_GRAVITY_STEP nudge per call (right for a per-tick fall), this
// grows the search range from scratch each time, so it finds the TRUE
// resting height in a single shot regardless of how big the actual gap
// turns out to be. Used by advanceRockySettle below to answer "how
// far would the body fall if the knee were bent differently," which
// needs a real answer for a potentially large gap, not one tick's worth.
// Returns how far down it moved (0.0f if it couldn't move at all -- it
// was already resting, or already embedded right where it started).
static float dropActiveRobotToRest(void)
{
    if (robotCollidesWithEnvironment())
        return 0.0f; // already touching/embedded -- nothing to drop

    float safe = 0.0f;      // largest confirmed-clear drop so far
    float push = SIMULATION_LEG_PUSH_SEARCH_START;

    // Tracks whether the doubling search below ever actually FOUND a
    // collision boundary, as opposed to just running out of search range
    // (SIMULATION_LEG_PUSH_SEARCH_MAX) without ever touching anything.
    // That distinction matters a lot to advanceRockySettle's own probes:
    // they call this to answer "would a candidate kneeAngle/bodyAngle let
    // the robot fall further," and pick whichever candidate reports the
    // BIGGEST drop -- without this flag, a candidate that actually swings
    // the tested part out past the edge of a short environment segment
    // (open air, no ground anywhere below within range) used to come back
    // reporting a drop of roughly SIMULATION_LEG_PUSH_SEARCH_MAX, which a
    // real report showed reads to those probes as "by far the best
    // option" -- bigger than any genuine nearby resting-height
    // improvement -- so they'd confidently commit to rotating the body
    // (or knee) straight off the edge of a small platform, and the very
    // next ordinary gravity tick then free-falls with nothing left
    // underneath it at all. "Ran out of search range" has to report back
    // as "no improvement here" (0.0f), not as the deepest drop anyone's
    // found yet.
    BOOL foundGround = FALSE;

    for (;;)
    {
        translateActiveRobot(0.0f, -push);
        BOOL hits = robotCollidesWithEnvironment();
        translateActiveRobot(0.0f, push); // undo probe

        if (hits) { foundGround = TRUE; break; }

        safe = push;
        if (push >= SIMULATION_LEG_PUSH_SEARCH_MAX)
            break; // sanity cap -- nothing below it for a long way (freefall)

        push *= 2.0f;
    }

    if (!foundGround)
        return 0.0f; // open air the whole way out -- not a real resting
                      // spot, so this is not an "improvement" of any size

    float blocked = push;

    for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
    {
        float mid = (safe + blocked) * 0.5f;

        translateActiveRobot(0.0f, -mid);
        BOOL hits = robotCollidesWithEnvironment();
        translateActiveRobot(0.0f, mid); // undo probe

        if (hits) blocked = mid; else safe = mid;
    }

    translateActiveRobot(0.0f, -safe);
    return safe;
}

// Rocky-only: called every single gravity tick (applyGravityStep, below,
// calls this every time any part of Rocky is touching something -- see
// its own comment), tries two independent one-small-step probes -- bend
// the KNEE, and rotate the RECTANGLE itself (Rocky's own `angle` field)
// -- and commits whichever of them (if either) lets the body fall any
// further right now. This is specifically for the knee-detach feature
// (app.h's own comment on draggingRockyKnee): once the knee can sit far
// from the rectangle, gravity's own whole-body drop stops the instant
// its LOWEST part (often the foot) touches something, leaving the
// rectangle exactly as high up (and at whatever ORIENTATION) that pose
// geometrically places it -- which can look like the body is floating,
// when really it's just resting on top of an oddly long/offset leg.
// Real robots settle that kind of thing by both the knee bending AND the
// torso tipping under its own weight until something else (the
// rectangle itself) also finds the ground; being called every tick lets
// this happen gradually, over as many ticks as it takes, rather than
// snapping to the final pose the instant it lands.
//
// Each probe is independent coordinate-descent, one variable at a time
// (first kneeAngle, then r->angle, each against whatever the OTHER just
// ended up as) -- not a single combined search over both together, so a
// resting pose that only opens up once BOTH change together at once
// could be missed. In practice this converges fine over enough ticks
// since each variable's own improvement still keeps nudging the other's
// available range too, and it keeps each individual probe cheap and easy
// to reason about.
//
// Deliberately does only ONE bounded step per probe per call rather than
// hill-climbing to full convergence in one shot -- called continuously
// (every tick, by explicit request) instead of once, it doesn't need to
// finish in a single call; a tiny fixed step per tick is what actually
// produces a smooth multi-frame sink/tip rather than an instant snap.
// Safe to call this often because there's no velocity or damping state
// carried between calls (unlike the removed knee pendulum) -- each probe
// is an independent, deterministic test that only ever commits a change
// it has already confirmed (via dropActiveRobotToRest, the same
// binary-search idiom the rest of gravity uses) genuinely lets the body
// fall further while staying collision-free, so there is nothing here
// that can accumulate into an oscillation or a stuck state.
// Persists across calls: TRUE once BOTH probes below are individually
// exhausted (see rockyKneeSettleStep/rockyBodySettleStep) for the
// CURRENT landing, so this stops actually re-probing every subsequent
// tick instead of continuing to test forever. Reset to FALSE by
// applyGravityStep right when a FRESH landing begins (see its own
// comment) so a robot that's lifted off and lands again later still
// gets a full settle pass of its own.
static BOOL rockySettleConverged = FALSE;

// Each probe's OWN current step size, shrinking independently as it runs
// out of room -- separate statics (not the SIMULATION_LEG_SETTLE_STEP_DEG/
// SIMULATION_BODY_SETTLE_STEP_DEG constants, which are just where these
// start) because a fixed step that never adapts can call things
// "converged" too early: the real remaining improvement at a 1-degree
// step can round down under SIMULATION_LEG_SETTLE_MIN_DROP even though a
// genuinely lower resting pose exists a finer fraction of a degree away
// (a real report of exactly this -- settling stopping with the body
// still visibly floating). Halving on every failed attempt instead of
// giving straight up is the same "a coarse step can overshoot a narrow
// useful window" fix the original one-shot version of this search used,
// just kept as per-probe state now that this runs continuously instead
// of to completion in one call. Both reset to their starting constants
// by applyGravityStep alongside rockySettleConverged.
static float rockyKneeSettleStep = 0.0f;
static float rockyBodySettleStep = 0.0f;

// Set TRUE for the duration of an E/Q-triggered settle pass (see the
// WM_KEYDOWN kickoff below) to stop Probe 1 (knee-bending) from firing
// while the USER is the one actively driving kneeAngle -- without this,
// Probe 1's own "improve the resting fit" search runs every 16ms via the
// postRotateSettleActive timer and directly fights the player's own E/Q
// presses (LEGPUSH lifts the body to clear the leg the user just bent,
// then Probe 1 bends it back and drops the body again a tick later),
// which is what read as the body visibly jittering up and down while
// holding E or Q. Probe 2 (body-rotate/topple) and the plain gravity
// drop are NOT suppressed -- neither one touches kneeAngle, so neither
// one fights the user, and a genuinely unstable rotate still topples.
// Left FALSE (the normal case) for VK_LEFT/RIGHT's own topple kickoff,
// where nothing else is driving kneeAngle and Probe 1 is safe to run.
static BOOL rockyKneeSettleSuppressed = FALSE;

// Which of the leg's two joint circles (knee, foot) is currently closer
// to resting on the environment -- same clearance measurement Probe 1's
// own baseFootClearance already uses (nearestEnvDistance minus the
// combined radius), just checking the knee too -- and returns THAT
// point's current world position (robot-scene space, same as
// bodyX/bodyY/translateActiveRobot, not env-pixel space). Probe 2 below
// pivots its rotation around whichever point this returns, instead of
// around Rocky's own rectangle center: a real object balanced on one
// point doesn't spin in place around its own centroid when gravity tips
// it, it rotates around whatever's actually touching the ground -- see
// rotateRockyBodyAroundPivot's own comment just below for the mechanics.
// Falls back to the rectangle's own center when the leg is hidden
// (nothing else to check, same as Probe 1 skipping entirely in that
// case) -- there is no meaningful "contact point" left to pivot around,
// so this just preserves the old center-pivot behavior for a legless
// Rocky.
static PointF rockyTopplePivotWorld(const Rocky* r)
{
    PointF center = getRockyCenter(*r);

    if (r->legHidden)
        return center;

    PointF kneeWorld = rotatePoint(r->kneeCircle, center, r->angle);
    PointF footWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, center, r->angle);

    float kecx, kecy, fecx, fecy;
    robotPointToEnvWorld(kneeWorld.x, kneeWorld.y, &kecx, &kecy);
    robotPointToEnvWorld(footWorld.x, footWorld.y, &fecx, &fecy);

    float kneeCombinedRadius = robotLengthToEnvWorld(r->kneeRadius) + simEnvLineHalfWidthWorld();
    float footCombinedRadius = robotLengthToEnvWorld(r->footRadius) + simEnvLineHalfWidthWorld();

    float kneeClearance = nearestEnvDistance(kecx, kecy) - kneeCombinedRadius;
    float footClearance = nearestEnvDistance(fecx, fecy) - footCombinedRadius;

    return (kneeClearance < footClearance) ? kneeWorld : footWorld;
}

// Clearance of whichever of {knee, foot} is NOT the current pivot -- the
// point Probe 2's rotation is actually trying to bring down to the
// ground. Exact same knee/foot clearance comparison as
// rockyTopplePivotWorld just above (see its comment), just returning the
// OTHER point's clearance instead of the closer one's position. Used by
// Probe 2's own "dangling point" fallback below, the same idea as Probe
// 1's baseFootClearance/bestClearanceGain a little further up: rotating
// around a point that's ALREADY on the ground holds THAT point's own
// clearance pinned at ~0 for the whole rotation (that's the definition of
// pivoting around it), so dropActiveRobotToRest's whole-body translate
// test can never see any benefit until the dangling point ALSO reaches
// the ground -- tracking its clearance directly is the only way to
// reward the rotation while it's still mid-tip, instead of only ever
// being able to detect the single exact instant both points cross zero
// together (which a fixed-size angle step will almost always step past).
static float rockyDanglingPointClearance(const Rocky* r)
{
    PointF center = getRockyCenter(*r);
    PointF kneeWorld = rotatePoint(r->kneeCircle, center, r->angle);
    PointF footWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, center, r->angle);

    float kecx, kecy, fecx, fecy;
    robotPointToEnvWorld(kneeWorld.x, kneeWorld.y, &kecx, &kecy);
    robotPointToEnvWorld(footWorld.x, footWorld.y, &fecx, &fecy);

    float kneeCombinedRadius = robotLengthToEnvWorld(r->kneeRadius) + simEnvLineHalfWidthWorld();
    float footCombinedRadius = robotLengthToEnvWorld(r->footRadius) + simEnvLineHalfWidthWorld();

    float kneeClearance = nearestEnvDistance(kecx, kecy) - kneeCombinedRadius;
    float footClearance = nearestEnvDistance(fecx, fecy) - footCombinedRadius;

    return (kneeClearance < footClearance) ? footClearance : kneeClearance;
}

// Changes r->angle by deltaAngle the way a real object actually topples:
// pivoted around `pivot` (a world point in robot-scene space, captured
// BEFORE this call -- normally from rockyTopplePivotWorld above, taken
// once at the start of a probe pass so both candidate directions pivot
// around the SAME point) rather than around Rocky's own rectangle
// center. A plain r->angle change alone only ever rotates around
// (bodyX, bodyY) -- see jointToWorld/rotatePoint's own use throughout
// this file. Following that with a compensating translateActiveRobot so
// `pivot` lands back exactly where it started turns that into a
// rotation about `pivot` instead: the standard "rotate about center,
// then translate" trick for getting an arbitrary pivot out of a
// fixed-pivot primitive. Returns the (dx, dy) shift it applied, so the
// caller can undo this exactly (translateActiveRobot by its negation,
// then r->angle -= deltaAngle) without needing to recompute anything --
// safer than re-deriving the pivot's new position a second time, which
// could drift by a hair from floating-point rounding.
static PointF rotateRockyBodyAroundPivot(Rocky* r, PointF pivot, float deltaAngle)
{
    PointF center = getRockyCenter(*r);
    PointF pivotAfterSpin = rotatePoint(pivot, center, deltaAngle);
    PointF shift = { pivot.x - pivotAfterSpin.x, pivot.y - pivotAfterSpin.y };

    r->angle += deltaAngle;
    translateActiveRobot(shift.x, shift.y);
    return shift;
}

// Same idea as resolveUpwardIfPenetrating (further up), but as a plain
// probe that returns how much correction it applied instead of always
// committing/repainting -- called right after rotateRockyBodyAroundPivot
// above, every time Probe 2 below tries a candidate rotation. `pivot` is
// picked as whichever point is ALREADY closest to the ground (gap close
// to, or exactly, 0 -- see rockyTopplePivotWorld), and the whole point of
// pivoting around it is to hold it EXACTLY fixed through the rotation --
// but floating-point rounding in the rotate-then-compensate math
// (rotatePoint, then a translate built from the difference of two
// rotated points) can leave that same point a hair embedded afterward,
// even though it's mathematically meant to land back exactly where it
// started. That's exactly the scenario SIMULATION_SLOPE_CORRECTION_MAX's
// own comment describes: "rotating a rigid body... can push a limb a
// hair into the ground even though the body as a whole was already
// settled." Left uncorrected, dropActiveRobotToRest's own very first
// check (robotCollidesWithEnvironment -> return 0.0f immediately) would
// silently report EVERY candidate direction as "no improvement" no
// matter how much genuine room exists elsewhere -- which is exactly what
// a real report showed: Probe 2 never committing a single rotation,
// scout included, across multiple different poses, once its pivot point
// was sitting at gap ~ 0 (which is the common case -- that's what makes
// it the pivot in the first place). Bounded to the same tiny budget
// resolveUpwardIfPenetrating uses, since this is meant to erase a
// sub-precision rounding residual, not resolve a genuine collision -- a
// real block still comes back as "no improvement" afterward, it's just
// no longer drowned out by this false one first.
static float clearPivotRoundingResidual(void)
{
    if (!robotCollidesWithEnvironment()) return 0.0f;

    float safe = SIMULATION_SLOPE_CORRECTION_MAX;
    float blocked = 0.0f;

    for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
    {
        float mid = (safe + blocked) * 0.5f;

        translateActiveRobot(0.0f, mid);
        BOOL hit = robotCollidesWithEnvironment();
        translateActiveRobot(0.0f, -mid);

        if (hit) blocked = mid; else safe = mid;
    }

    translateActiveRobot(0.0f, safe);
    return safe;
}

static void advanceRockySettle(void)
{
    if (app.robotScene.activeKind != ROBOT_KIND_ROCKY)
        return;

    if (rockySettleConverged)
        return; // both probes already confirmed exhausted for this landing

    // Lazily initialize on first-ever use (also re-primed by
    // applyGravityStep on every fresh landing -- see its own comment) --
    // 0.0f only ever occurs before either has been set, since a real
    // landing always (re)sets both to their starting constants.
    if (rockyKneeSettleStep <= 0.0f)
        rockyKneeSettleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
    if (rockyBodySettleStep <= 0.0f)
        rockyBodySettleStep = SIMULATION_BODY_SETTLE_STEP_DEG;

    Rocky* r = &app.robotScene.rocky;

    // Probe 1: bend the knee. Skipped entirely while the leg is hidden
    // (ID_ROCKY_TOGGLE_LEG_BUTTON, app.h's legHidden comment) -- with no
    // leg being rendered or collision-tested, bending an invisible knee
    // around an invisible foot can never affect anything real (the
    // rectangle's own position/rotation is untouched by kneeAngle -- see
    // getRockyCenter), so this used to just spin kneeAngle through
    // hundreds of degrees for no visible effect, flooding the console
    // with [SETTLE] lines about a foot nobody can see or collide with.
    if (!r->legHidden && !rockyKneeSettleSuppressed && rockyKneeSettleStep >= SIMULATION_LEG_SETTLE_MIN_STEP_DEG)
    {
        float baseKneeAngle = r->kneeAngle;
        float bestDrop = 0.0f;
        float bestKneeAngle = baseKneeAngle;

        // Baseline clearance of the FOOT alone, captured before either
        // candidate angle is tried below. This is the
        // fallback criterion for when dropActiveRobotToRest can't see an
        // improvement at all: see the comment inside the loop for why
        // that happens and why this catches it.
        PointF baseCenter = getRockyCenter(*r);
        PointF baseFootWorld = jointToWorld(r->footCircle, r->kneeCircle, baseKneeAngle, baseCenter, r->angle);
        float baseFecx, baseFecy;
        robotPointToEnvWorld(baseFootWorld.x, baseFootWorld.y, &baseFecx, &baseFecy);
        float footCombinedRadius = robotLengthToEnvWorld(r->footRadius) + simEnvLineHalfWidthWorld();
        float baseFootClearance = nearestEnvDistance(baseFecx, baseFecy) - footCombinedRadius;

        float bestClearanceGain = 0.0f;
        float bestClearanceKneeAngle = baseKneeAngle;

        // Per-direction values, same reason Probe 2 keeps its own
        // dropAtDir[] below: needed after this loop to tell WHICH kind
        // of improvement each direction found (see the straddle check
        // right after this loop).
        float dropAtDir[2] = { 0.0f, 0.0f };
        float gainAtDir[2] = { 0.0f, 0.0f };

        int dirIndex = 0;
        for (int dir = -1; dir <= 1; dir += 2, dirIndex++)
        {
            r->kneeAngle = baseKneeAngle + dir * rockyKneeSettleStep;

            float drop = dropActiveRobotToRest();
            if (drop > 0.0f)
                translateActiveRobot(0.0f, drop); // undo the probe's own move

            dropAtDir[dirIndex] = drop;

            if (drop > bestDrop)
            {
                bestDrop = drop;
                bestKneeAngle = r->kneeAngle;
            }

            // Fallback for exactly the "rectangle rests on a corner,
            // foot dangles in mid-air" pose a real report showed:
            // dropActiveRobotToRest tests a whole-body translate, but
            // bending the knee never moves the rectangle's own corners
            // -- once THOSE are already what's resting on the ground,
            // any candidate kneeAngle re-collides via that same corner
            // the instant the body is pushed down at all, so drop comes
            // back exactly 0.0f no matter how much closer the dangling
            // foot itself would get. Comparing the foot's own clearance
            // directly catches that case: if this candidate angle
            // doesn't embed anything and brings the foot measurably
            // closer to the ground than the baseline, that is a real
            // improvement even though the body itself never moves.
            if (!robotCollidesWithEnvironment())
            {
                PointF center = getRockyCenter(*r);
                PointF footWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, center, r->angle);
                float fecx, fecy;
                robotPointToEnvWorld(footWorld.x, footWorld.y, &fecx, &fecy);
                float clearance = nearestEnvDistance(fecx, fecy) - footCombinedRadius;
                float gain = baseFootClearance - clearance;

                if (clearance >= 0.0f)
                    gainAtDir[dirIndex] = gain;

                if (clearance >= 0.0f && gain > bestClearanceGain)
                {
                    bestClearanceGain = gain;
                    bestClearanceKneeAngle = r->kneeAngle;
                }
            }
        }

        r->kneeAngle = baseKneeAngle;

        // One direction wants to bend the knee to let the body fall
        // further (bestDrop) while the OPPOSITE direction wants to swing
        // the dangling foot closer instead (bestClearanceGain) -- both
        // genuinely real at the SAME time, just from opposite candidate
        // angles. Committing whichever one happens to win this tick, then
        // finding the other one winning again next tick once the first
        // is undone by the resulting pose change, is exactly what a real
        // report showed as the knee (and the whole robot along with it)
        // visibly rocking back and forth instead of settling -- see
        // Probe 2's own crossCriterionStraddle just below for the mirror
        // image of this same fix. Shrinking here instead of picking a
        // side lets a finer step eventually resolve which one the
        // geometry actually favors, same as the existing same-criterion
        // straddle idea.
        BOOL crossCriterionStraddle =
               (dropAtDir[0] > SIMULATION_LEG_SETTLE_MIN_DROP && gainAtDir[1] > SIMULATION_LEG_SETTLE_MIN_CLEARANCE_GAIN)
            || (dropAtDir[1] > SIMULATION_LEG_SETTLE_MIN_DROP && gainAtDir[0] > SIMULATION_LEG_SETTLE_MIN_CLEARANCE_GAIN);

        // Compared against SIMULATION_LEG_SETTLE_MIN_DROP rather than a
        // bare > 0.0f -- see that constant's own comment on why: without
        // a floor, the search binary search's own precision noise right
        // at the true resting pose reads as an endless string of
        // wafer-thin "improvements," which is exactly what showed up as
        // visible vibration instead of a clean stop.
        if (crossCriterionStraddle)
        {
            printf("[SETTLE] kneeAngle=%.2f both directions improve but by different criteria (drop=%.5f/%.5f gain=%.5f/%.5f) -- shrinking instead of picking one, to avoid rocking back and forth\n",
                   r->kneeAngle, dropAtDir[0], dropAtDir[1], gainAtDir[0], gainAtDir[1]);
            rockyKneeSettleStep *= 0.5f;
        }
        else if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            // Cap what's actually applied THIS tick -- see
            // SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK's own comment: a
            // small angle step can still uncover a large resting-depth
            // change, and applying all of it in one tick is what read as
            // a sudden snap after looking "stuck." Any amount left over
            // just shows up again as a fresh bestDrop next tick (from the
            // now-lower position), so nothing is lost, only spread out.
            float appliedDrop = bestDrop;
            if (appliedDrop > SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK)
                appliedDrop = SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK;

            r->kneeAngle = bestKneeAngle;
            translateActiveRobot(0.0f, -appliedDrop);
            printf("[SETTLE] kneeAngle=%.2f bent the knee %.4f deg, body fell %.5f further this tick (of %.5f available)\n",
                   r->kneeAngle, rockyKneeSettleStep, appliedDrop, bestDrop);
            // Keep this probe's step size as-is -- it's still finding
            // real gains at this scale, no need to refine yet.
        }
        else if (bestClearanceGain > SIMULATION_LEG_SETTLE_MIN_CLEARANCE_GAIN)
        {
            // The body itself didn't fall any further, but the dangling
            // foot is now genuinely closer to the ground than it was --
            // commit the knee rotation alone. No translateActiveRobot
            // call here: the body isn't moving, only the foot swings
            // around the still-fixed knee.
            r->kneeAngle = bestClearanceKneeAngle;
            printf("[SETTLE] kneeAngle=%.2f swung the dangling foot %.4f deg closer to the ground (clearance improved %.5f, body stayed put)\n",
                   r->kneeAngle, rockyKneeSettleStep, bestClearanceGain);
        }
        else
        {
            // Before shrinking further, scout a much bigger, FIXED angle
            // in each direction -- see SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG's
            // own comment for why a shrinking step alone can get stuck on
            // a local flat spot. Only ever used to decide direction/
            // worthwhileness; the actual committed move below is still
            // just one ordinary rockyKneeSettleStep-sized nudge, so this
            // can't itself cause a visible jump.
            BOOL scoutedKnee = FALSE;

            // Both directions are measured and undone before either is
            // judged -- this used to just try dir=-1 (clockwise) first and
            // commit to it the instant it cleared the threshold below,
            // never even scouting dir=+1 (counterclockwise) at all once
            // clockwise passed. That made clockwise win by going first,
            // not by being the better option, which is exactly the
            // clockwise-tipping bias a real report noticed. Scouting both
            // first and picking the larger improvement (same drop-before-
            // gain priority the direct branch above already uses) removes
            // that order bias; see the winner-selection loop just below.
            float scoutDropAtDir[2] = { 0.0f, 0.0f };
            float scoutGainAtDir[2] = { -1.0f, -1.0f };
            BOOL scoutClearAtDir[2] = { FALSE, FALSE };

            int scoutDirIndex = 0;
            for (int dir = -1; dir <= 1; dir += 2, scoutDirIndex++)
            {
                r->kneeAngle = baseKneeAngle + dir * SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG;

                float scoutDrop = dropActiveRobotToRest();
                if (scoutDrop > 0.0f)
                    translateActiveRobot(0.0f, scoutDrop); // undo the probe's own move

                BOOL scoutClear = !robotCollidesWithEnvironment();
                float scoutGain = -1.0f;

                if (scoutClear)
                {
                    PointF sCenter = getRockyCenter(*r);
                    PointF sFootWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, sCenter, r->angle);
                    float sfecx, sfecy;
                    robotPointToEnvWorld(sFootWorld.x, sFootWorld.y, &sfecx, &sfecy);
                    float sClearance = nearestEnvDistance(sfecx, sfecy) - footCombinedRadius;
                    scoutGain = baseFootClearance - sClearance;
                }

                scoutDropAtDir[scoutDirIndex] = scoutDrop;
                scoutGainAtDir[scoutDirIndex] = scoutGain;
                scoutClearAtDir[scoutDirIndex] = scoutClear;

                r->kneeAngle = baseKneeAngle; // undo this trial, same as the direct branch's own undo above
            }

            // Same drop-before-gain priority the direct branch above uses
            // (bestDrop checked before bestClearanceGain): a real drop
            // candidate always wins over a mere clearance-gain candidate
            // regardless of magnitude, and within the same criterion the
            // larger value wins. scoutDirIndex 0 is dir=-1 (clockwise), 1
            // is dir=+1 (counterclockwise) -- an exact tie between two
            // equally-good candidates still favors index 0 (clockwise)
            // below, same as the direct branch's own strict '>' compares,
            // but that residual tie-break is now the ONLY place clockwise
            // gets an edge, not an unconditional first-past-the-post win.
            int scoutWinner = -1;
            BOOL scoutWinnerIsDrop = FALSE;

            for (int i = 0; i < 2; i++)
            {
                BOOL isDropCandidate = scoutDropAtDir[i] > SIMULATION_LEG_SETTLE_MIN_DROP;
                BOOL isGainCandidate = !isDropCandidate && scoutClearAtDir[i] && scoutGainAtDir[i] > SIMULATION_LEG_SETTLE_MIN_CLEARANCE_GAIN;

                if (!isDropCandidate && !isGainCandidate)
                    continue;

                if (scoutWinner == -1)
                {
                    scoutWinner = i;
                    scoutWinnerIsDrop = isDropCandidate;
                }
                else if (isDropCandidate && !scoutWinnerIsDrop)
                {
                    scoutWinner = i;
                    scoutWinnerIsDrop = TRUE;
                }
                else if (isDropCandidate == scoutWinnerIsDrop)
                {
                    if (isDropCandidate && scoutDropAtDir[i] > scoutDropAtDir[scoutWinner])
                        scoutWinner = i;
                    else if (!isDropCandidate && scoutGainAtDir[i] > scoutGainAtDir[scoutWinner])
                        scoutWinner = i;
                }
            }

            if (scoutWinner != -1)
            {
                int dir = (scoutWinner == 0) ? -1 : 1;
                float scoutDrop = scoutDropAtDir[scoutWinner];
                float scoutGain = scoutGainAtDir[scoutWinner];

                r->kneeAngle = baseKneeAngle + dir * rockyKneeSettleStep;

                // The scout already measured a real, collision-safe
                // drop (scoutDrop) -- but it measured it OUT AT the
                // full scouted angle, not here at the small nudge. Re-
                // probing dropActiveRobotToRest() at the nudge angle
                // (the previous version of this fix) mostly finds
                // almost nothing, because the ground clearance opens up
                // gradually across the whole scouted range, not right at
                // its very start -- that read as the body creeping down
                // by a few thousandths per tick while still rotating
                // through tens of degrees, i.e. still visibly "floating"
                // for a long time, just no longer perfectly frozen.
                // Instead, spend from the SAME per-tick budget
                // (SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK) the real
                // bestDrop branch above already uses, straight out of
                // scoutDrop, so the body catches up in only a handful of
                // ticks instead of dozens. This is optimistic -- the
                // clearance at the nudge angle isn't actually the same
                // clearance measured at the scouted angle -- so verify
                // afterward and fall back to whatever's genuinely safe
                // right here if it turns out to overshoot into the
                // environment. Skipped when this nudge was only the
                // clearanceGain (dangling-foot) case, matching the real
                // branch above: the body genuinely isn't meant to move
                // there, only the foot swings.
                float nudgeAppliedDrop = 0.0f;
                if (scoutDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
                {
                    nudgeAppliedDrop = scoutDrop;
                    if (nudgeAppliedDrop > SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK)
                        nudgeAppliedDrop = SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK;
                    translateActiveRobot(0.0f, -nudgeAppliedDrop);

                    if (robotCollidesWithEnvironment())
                    {
                        // Optimistic budget didn't actually fit at this
                        // angle -- undo it and fall back to the old,
                        // always-safe approach: whatever's genuinely
                        // available right here, found the same way
                        // dropActiveRobotToRest already searches.
                        translateActiveRobot(0.0f, nudgeAppliedDrop);
                        nudgeAppliedDrop = dropActiveRobotToRest();
                        if (nudgeAppliedDrop > SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK)
                            nudgeAppliedDrop = SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK;
                        if (nudgeAppliedDrop > 0.0f)
                            translateActiveRobot(0.0f, -nudgeAppliedDrop);
                        else
                            nudgeAppliedDrop = 0.0f;
                    }
                }

                printf("[SETTLE] kneeAngle=%.2f scouted real improvement %.1f deg out (drop=%.5f clearanceGain=%.5f), fell %.5f this tick -- nudging that way instead of shrinking\n",
                       r->kneeAngle, dir * SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG, scoutDrop, scoutGain, nudgeAppliedDrop);
                scoutedKnee = TRUE;
            }

            if (!scoutedKnee)
            {
                r->kneeAngle = baseKneeAngle;
                rockyKneeSettleStep *= 0.5f; // nothing here, and nothing further out either -- get finer before giving up
            }
        }
    }

    // Probe 2: rotate the rectangle itself. This used to just change
    // r->angle directly, which always rotates around (bodyX, bodyY) --
    // Rocky's own rectangle center -- no matter what's actually touching
    // the ground. That's why a real report showed the body permanently
    // stuck hovering next to a planted foot (screenshot: only the foot
    // circle on the ground, the knee circle and rectangle floating well
    // above it, [SETTLE] converged and never moving again): rotating
    // around the CENTER swings the foot along its own arc too, and once
    // that arc reaches its lowest point, further rotation in the same
    // direction starts lifting the foot back up, so the confirmed drop
    // shrinks toward zero and the probe "correctly" (by its own narrow
    // measure) concludes there's nothing left to gain -- even though the
    // rectangle is still plainly floating. A real object balanced on one
    // point doesn't spin around its own centroid when gravity tips it;
    // it pivots around whatever's actually touching the ground, and keeps
    // doing so until either its center of mass swings over that point or
    // something else comes down to meet it. rockyTopplePivotWorld picks
    // that contact point (knee or foot, whichever is closer to the
    // ground) once, before either direction is tried, and
    // rotateRockyBodyAroundPivot rotates around THAT instead of the
    // center -- see both their own comments above for the mechanics.
    // dropActiveRobotToRest still only re-tests straight-down clearance
    // afterward, on top of the pivot, so a candidate only ever commits if
    // it genuinely opens up more room to fall, same guarantee as before.
    if (rockyBodySettleStep >= SIMULATION_LEG_SETTLE_MIN_STEP_DEG)
    {
        float baseBodyAngle = r->angle;
        PointF pivot = rockyTopplePivotWorld(r); // same contact point for both directions below
        float bestDrop = 0.0f;
        float bestBodyAngle = baseBodyAngle;
        PointF bestShift = { 0.0f, 0.0f };
        float bestResidual = 0.0f;

        // Fallback for the exact mirror of Probe 1's "dangling foot" case
        // above, just from the other direction: as long as the point
        // rockyTopplePivotWorld picked is ALREADY on the ground, holding
        // it fixed (rotateRockyBodyAroundPivot) means dropActiveRobotToRest
        // can never see any benefit from rotating -- it's blocked by that
        // same pivot at ~0 clearance no matter which way the body turns,
        // right up until the OTHER point (the dangling one) also reaches
        // the ground. Without tracking that point's own clearance
        // directly, Probe 2 is structurally blind to the entire toppling
        // motion in between -- a fixed-size angle step can then jump
        // straight past the one instant both points cross zero together,
        // planting the dangling point while lifting the pivot back off,
        // exactly the overshoot a real report showed (pressing E once
        // swinging past the two-contact pose instead of landing on it).
        // See rockyDanglingPointClearance's own comment for the mechanics.
        // Skipped entirely when the leg is hidden -- same reasoning as
        // Probe 1's own !r->legHidden guard above: with no leg being
        // rendered or collision-tested, its knee/foot circles are just
        // stale numbers left over in the Rocky struct, not anything real
        // that's actually resting on (or dangling above) the ground. Using
        // them here would silently drive body rotation off of clearance
        // values that have nothing to do with what's actually visible or
        // collidable -- exactly the kind of ungrounded nudge a real report
        // showed as a bare rectangle (no leg) endlessly rocking a degree or
        // two around 90 instead of settling flush, once this fallback
        // started firing off invisible-leg geometry it was never meant to
        // look at for a legless Rocky.
        float baseDanglingClearance = r->legHidden ? 0.0f : rockyDanglingPointClearance(r);
        float bestClearanceGain = 0.0f;
        float bestClearanceBodyAngle = baseBodyAngle;
        PointF bestClearanceShift = { 0.0f, 0.0f };
        float bestClearanceResidual = 0.0f;

        // Raw drop found in EACH direction, kept separately from bestDrop
        // above (which only remembers the winner) -- see the "straddling"
        // check right after this loop for why both matter, not just
        // whichever is larger. gainAtDir is the same idea for the
        // dangling-point clearance gain, used by crossCriterionStraddle
        // just after the loop.
        float dropAtDir[2] = { 0.0f, 0.0f };
        float gainAtDir[2] = { 0.0f, 0.0f };

        int dirIndex = 0;
        for (int dir = -1; dir <= 1; dir += 2, dirIndex++)
        {
            float deltaAngle = dir * rockyBodySettleStep;
            PointF shift = rotateRockyBodyAroundPivot(r, pivot, deltaAngle);

            // Clears a possible sub-precision rounding residual left by
            // the pivot rotation itself (see clearPivotRoundingResidual's
            // own comment) BEFORE asking dropActiveRobotToRest whether
            // this candidate helps -- otherwise its very first check
            // would see the pivot point as still (falsely) colliding and
            // report 0.0f no matter what.
            float residual = clearPivotRoundingResidual();

            float drop = dropActiveRobotToRest();
            if (drop > 0.0f)
                translateActiveRobot(0.0f, drop); // undo the probe's own vertical move

            dropAtDir[dirIndex] = drop;

            if (drop > bestDrop)
            {
                bestDrop = drop;
                bestBodyAngle = r->angle;
                bestShift = shift;
                bestResidual = residual;
            }

            // Same "did this get measurably closer, without embedding"
            // check Probe 1's own dangling-foot fallback uses just above
            // -- see the comment before this loop for why Probe 2 needs
            // its own copy of it. Measured right here, after the pivot
            // rotation + residual correction but before undoing them
            // below, so it reflects exactly this candidate's world pose.
            if (!r->legHidden && !robotCollidesWithEnvironment())
            {
                float clearance = rockyDanglingPointClearance(r);
                float gain = baseDanglingClearance - clearance;

                if (clearance >= 0.0f)
                    gainAtDir[dirIndex] = gain;

                if (clearance >= 0.0f && gain > bestClearanceGain)
                {
                    bestClearanceGain = gain;
                    bestClearanceBodyAngle = r->angle;
                    bestClearanceShift = shift;
                    bestClearanceResidual = residual;
                }
            }

            if (residual > 0.0f)
                translateActiveRobot(0.0f, -residual); // undo the residual correction too
            translateActiveRobot(-shift.x, -shift.y); // undo this candidate's pivot rotation
            r->angle = baseBodyAngle;
        }

        // A flat rigid body settling against a flat surface doesn't have a
        // smooth minimum height at the flush angle -- it has a SHARP CORNER
        // there (height grows roughly linearly with tilt on both sides,
        // same slope either way). Hill-climbing a fixed-size step against a
        // kink like that doesn't reliably land ON it: once the step is
        // bigger than the remaining distance to the kink, stepping past it
        // from either side can look comparably good, so the search commits
        // a full step past, then a full step back, forever, at whatever
        // size rockyBodySettleStep happened to be -- it never gets a
        // chance to shrink, because ordinarily shrinking only happens when
        // NEITHER direction improves, and here one direction always does.
        // That's exactly what a real report showed: a bare rectangle (no
        // leg, so this is the only thing driving its rotation) dropped at
        // 88-89 degrees just rocking back and forth instead of settling
        // flush at 90. Both directions showing a real (above-noise)
        // improvement at the SAME time is the telltale sign of straddling
        // that kink -- respond by shrinking the step instead of committing
        // either side, so the next attempt brackets the kink more tightly
        // instead of overshooting it again at the same size.
        BOOL straddlingKink = (dropAtDir[0] > SIMULATION_LEG_SETTLE_MIN_DROP)
                            && (dropAtDir[1] > SIMULATION_LEG_SETTLE_MIN_DROP);

        // Mirror image of the same problem: instead of both directions
        // wanting a real vertical drop, ONE direction wants the drop
        // (relieving whatever's embedded at the pivot -- typically the
        // rectangle's own corner right next to it) while the OPPOSITE
        // direction wants to swing the dangling knee/foot closer instead
        // (bestClearanceGain). Both are genuinely real, just from
        // opposite candidate angles -- committing whichever wins this
        // tick, only to find the other one winning again next tick once
        // this tick's commit changed the pose, is exactly the "keeps
        // vibrating going forth and back" a real report showed for a
        // pose where the knee AND a body corner were both already
        // resting while the foot was still dangling well above. Treated
        // the same way as straddlingKink above: shrink instead of
        // picking a side.
        BOOL crossCriterionStraddle =
               (dropAtDir[0] > SIMULATION_LEG_SETTLE_MIN_DROP && gainAtDir[1] > SIMULATION_LEG_SETTLE_MIN_CLEARANCE_GAIN)
            || (dropAtDir[1] > SIMULATION_LEG_SETTLE_MIN_DROP && gainAtDir[0] > SIMULATION_LEG_SETTLE_MIN_CLEARANCE_GAIN);

        if (straddlingKink || crossCriterionStraddle)
        {
            printf("[SETTLE] bodyAngle=%.2f both directions still improve (drop=%.5f/%.5f gain=%.5f/%.5f) -- shrinking instead of picking one, to avoid oscillating across the exact resting angle\n",
                   r->angle, dropAtDir[0], dropAtDir[1], gainAtDir[0], gainAtDir[1]);
            rockyBodySettleStep *= 0.5f;
        }
        else if (bestDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
        {
            // Same per-tick cap as the knee probe above, same reason --
            // see SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK's comment. The
            // pivot shift AND its residual correction are applied in full
            // (they're what makes this a real pivot instead of a
            // center-spin, and what keeps it there without a false
            // self-collision) -- only the LEFTOVER straight-down settling
            // on top of them is capped/gradual.
            float appliedDrop = bestDrop;
            if (appliedDrop > SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK)
                appliedDrop = SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK;

            r->angle = bestBodyAngle;
            translateActiveRobot(bestShift.x, bestShift.y);
            if (bestResidual > 0.0f)
                translateActiveRobot(0.0f, bestResidual);
            translateActiveRobot(0.0f, -appliedDrop);
            printf("[SETTLE] bodyAngle=%.2f pivoted the rectangle %.4f deg around its ground contact, body fell %.5f further this tick (of %.5f available)\n",
                   r->angle, rockyBodySettleStep, appliedDrop, bestDrop);
        }
        else if (bestClearanceGain > SIMULATION_LEG_SETTLE_MIN_CLEARANCE_GAIN)
        {
            // The whole body isn't falling any further (the pivot's still
            // blocking that test), but the dangling knee/foot is now
            // genuinely closer to the ground than it was -- commit the
            // rotation alone, same idea as Probe 1's own dangling-foot
            // branch just above. Still need to replay the shift + residual
            // (a pivot rotation always moves bodyX/bodyY to keep the pivot
            // itself fixed, unlike Probe 1's pure kneeAngle change), just
            // no extra straight-down translateActiveRobot on top -- there
            // is genuinely nothing further to fall yet at this angle.
            r->angle = bestClearanceBodyAngle;
            translateActiveRobot(bestClearanceShift.x, bestClearanceShift.y);
            if (bestClearanceResidual > 0.0f)
                translateActiveRobot(0.0f, bestClearanceResidual);
            printf("[SETTLE] bodyAngle=%.2f pivoted the rectangle %.4f deg around its ground contact, dangling point %.5f closer to the ground (body stayed put)\n",
                   r->angle, rockyBodySettleStep, bestClearanceGain);
        }
        else
        {
            // Same scouting idea as the knee probe above, same reason --
            // now checking BOTH the drop criterion and the dangling-point
            // clearance criterion at the bigger scouted angle, mirroring
            // the direct branch's own two criteria just above. Same
            // contact-point pivot as the direct branch, not a center-spin.
            BOOL scoutedBody = FALSE;

            // Both directions are measured and undone before either is
            // judged -- see the knee probe's own scout branch above for
            // the full reasoning: trying dir=-1 (clockwise) first and
            // committing to it the instant it cleared the threshold below
            // meant clockwise silently won every time both directions were
            // actually viable, never even scouting dir=+1 (counterclockwise)
            // once clockwise passed. Scouting both first and picking the
            // larger improvement (same drop-before-gain priority the direct
            // branch above uses) removes that order bias.
            float scoutDropAtDir[2] = { 0.0f, 0.0f };
            float scoutGainAtDir[2] = { -1.0f, -1.0f };
            BOOL scoutClearAtDir[2] = { FALSE, FALSE };

            int scoutDirIndex = 0;
            for (int dir = -1; dir <= 1; dir += 2, scoutDirIndex++)
            {
                float scoutDelta = dir * SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG;
                PointF scoutShift = rotateRockyBodyAroundPivot(r, pivot, scoutDelta);

                // Same false-self-collision fix as the direct branch above
                // -- see clearPivotRoundingResidual's own comment -- needed
                // here too, since the scout's OWN pivot rotation has the
                // exact same rounding-residual risk.
                float scoutResidual = clearPivotRoundingResidual();

                float scoutDrop = dropActiveRobotToRest();
                if (scoutDrop > 0.0f)
                    translateActiveRobot(0.0f, scoutDrop); // undo the probe's own vertical move

                // Same dangling-point measurement as the direct branch's
                // own loop above, just out at the full scouted angle --
                // see the comment before that loop for why this is needed
                // at all (dropActiveRobotToRest alone can't see it).
                BOOL scoutClear = !robotCollidesWithEnvironment();
                float scoutGain = -1.0f;
                if (!r->legHidden && scoutClear)
                {
                    float scoutClearance = rockyDanglingPointClearance(r);
                    scoutGain = baseDanglingClearance - scoutClearance;
                }

                scoutDropAtDir[scoutDirIndex] = scoutDrop;
                scoutGainAtDir[scoutDirIndex] = scoutGain;
                scoutClearAtDir[scoutDirIndex] = scoutClear;

                if (scoutResidual > 0.0f)
                    translateActiveRobot(0.0f, -scoutResidual); // undo the scout's residual correction too
                translateActiveRobot(-scoutShift.x, -scoutShift.y); // undo the scout's pivot rotation
                r->angle = baseBodyAngle;
            }

            // Same drop-before-gain priority as the knee probe's own scout
            // branch above -- see its comment for the full reasoning.
            // scoutDirIndex 0 is dir=-1 (clockwise), 1 is dir=+1
            // (counterclockwise).
            int scoutWinner = -1;
            BOOL scoutWinnerIsDrop = FALSE;

            for (int i = 0; i < 2; i++)
            {
                BOOL isDropCandidate = scoutDropAtDir[i] > SIMULATION_LEG_SETTLE_MIN_DROP;
                BOOL isGainCandidate = !isDropCandidate && scoutClearAtDir[i] && scoutGainAtDir[i] > SIMULATION_LEG_SETTLE_MIN_CLEARANCE_GAIN;

                if (!isDropCandidate && !isGainCandidate)
                    continue;

                if (scoutWinner == -1)
                {
                    scoutWinner = i;
                    scoutWinnerIsDrop = isDropCandidate;
                }
                else if (isDropCandidate && !scoutWinnerIsDrop)
                {
                    scoutWinner = i;
                    scoutWinnerIsDrop = TRUE;
                }
                else if (isDropCandidate == scoutWinnerIsDrop)
                {
                    if (isDropCandidate && scoutDropAtDir[i] > scoutDropAtDir[scoutWinner])
                        scoutWinner = i;
                    else if (!isDropCandidate && scoutGainAtDir[i] > scoutGainAtDir[scoutWinner])
                        scoutWinner = i;
                }
            }

            if (scoutWinner != -1)
            {
                int dir = (scoutWinner == 0) ? -1 : 1;
                float scoutDrop = scoutDropAtDir[scoutWinner];
                float scoutGain = scoutGainAtDir[scoutWinner];

                float nudgeDelta = dir * rockyBodySettleStep;
                rotateRockyBodyAroundPivot(r, pivot, nudgeDelta); // committed, kept below
                clearPivotRoundingResidual(); // same fix, also kept -- see above

                // Same follow-up as the knee probe's scout branch, same
                // reasoning -- see its comment for the full explanation:
                // spend straight from scoutDrop (already known safe out
                // at the full scouted angle), capped to the same per-
                // tick budget the real bestDrop branch above uses,
                // rather than re-measuring the (usually tiny) clearance
                // actually available at the small nudge angle. Verified
                // afterward and pulled back to whatever's genuinely safe
                // here if the optimistic amount doesn't actually fit.
                // Skipped when this nudge was only the scoutGain
                // (dangling-point) case, matching the direct branch
                // above and Probe 1's own scout branch: the body
                // genuinely isn't meant to drop any further here, only
                // pivot.
                float nudgeAppliedDrop = 0.0f;
                if (scoutDrop > SIMULATION_LEG_SETTLE_MIN_DROP)
                {
                    nudgeAppliedDrop = scoutDrop;
                    if (nudgeAppliedDrop > SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK)
                        nudgeAppliedDrop = SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK;
                    translateActiveRobot(0.0f, -nudgeAppliedDrop);

                    if (robotCollidesWithEnvironment())
                    {
                        translateActiveRobot(0.0f, nudgeAppliedDrop);
                        nudgeAppliedDrop = dropActiveRobotToRest();
                        if (nudgeAppliedDrop > SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK)
                            nudgeAppliedDrop = SIMULATION_LEG_SETTLE_MAX_DROP_PER_TICK;
                        if (nudgeAppliedDrop > 0.0f)
                            translateActiveRobot(0.0f, -nudgeAppliedDrop);
                        else
                            nudgeAppliedDrop = 0.0f;
                    }
                }

                printf("[SETTLE] bodyAngle=%.2f pivoted around its ground contact, scouted real improvement %.1f deg out (drop=%.5f gain=%.5f), fell %.5f this tick -- nudging that way instead of shrinking\n",
                       r->angle, dir * SIMULATION_LEG_SETTLE_SCOUT_STEP_DEG, scoutDrop, scoutGain, nudgeAppliedDrop);
                scoutedBody = TRUE;
            }

            if (!scoutedBody)
            {
                rockyBodySettleStep *= 0.5f;
            }
        }
    }

    // Probe 1 is skipped entirely while the leg is hidden (its own
    // !r->legHidden guard above), which means rockyKneeSettleStep just
    // sits at whatever it was last reset to and never shrinks -- for a
    // legHidden Rocky, waiting for it to drop below the MIN threshold
    // here was waiting for something that can never happen. That left
    // rockySettleConverged permanently unreachable for a legless Rocky,
    // no matter how thoroughly Probe 2 alone had already converged: the
    // settle timer (postRotateSettleActive/advancePostRotateSettle) never
    // saw "atRest" and so never shut itself back off, leaving it running
    // forever at whatever cadence WM_TIMER (throttled hard by any mouse
    // movement, see WM_MOUSEMOVE's own anti-starvation comment) happened
    // to manage -- which is exactly what a real report showed: a legless
    // Rocky's body visibly creeping a little further every time the mouse
    // moved, long after it had actually finished settling. Treat Probe 1
    // as trivially "already at its floor" when there's no leg for it to
    // ever run against, same as Probe 1's own skip condition, so
    // convergence only ever waits on whichever probe(s) can actually run.
    //
    // The exact same unreachability hits during an E/Q press: Probe 1's
    // OWN top-level guard is "!r->legHidden && !rockyKneeSettleSuppressed
    // && rockyKneeSettleStep >= MIN" -- rockyKneeSettleSuppressed is set
    // TRUE for the whole duration of the keypress (so the user's own
    // manual knee-bend isn't fought by the settle probe), which means
    // Probe 1 doesn't run then either, and rockyKneeSettleStep is just as
    // stuck as the legHidden case. Without bypassing it here too,
    // rockySettleConverged can never go TRUE while E/Q is held, so
    // advancePostRotateSettle keeps calling applyGravityStep -- a REAL
    // fall step -- every tick for as long as the key (or mouse movement,
    // which also unconditionally pumps the same driver) keeps going,
    // which is exactly "press e or q once and the whole robot starts
    // falling, faster when the cursor moves."
    if ((r->legHidden || rockyKneeSettleSuppressed || rockyKneeSettleStep < SIMULATION_LEG_SETTLE_MIN_STEP_DEG)
        && rockyBodySettleStep < SIMULATION_LEG_SETTLE_MIN_STEP_DEG)
    {
        // Both probes have been refined all the way down to the floor
        // and still found nothing worth committing -- THIS is the
        // resting pose. Stop actually testing from here on (until the
        // next fresh landing) rather than continuing to probe every tick
        // for the same nothing.
        rockySettleConverged = TRUE;
        printf("[SETTLE] converged -- kneeAngle=%.2f bodyAngle=%.2f, no further improvement found\n",
               r->kneeAngle, r->angle);
    }
}

// Applies one gravity step to the robot -- tentatively translates it down
// by `step` world units, then checks robotCollidesWithEnvironment. If that
// collides, binary-searches within the step (GRAVITY_CONTACT_SEARCH_ITERATIONS,
// config.h) for how far it can actually descend before touching, instead of
// just undoing the whole step -- otherwise the robot always stops up to one
// full step short of the ground, a gap that's invisible in world units at
// normal zoom but turns into an obvious floating gap once you zoom in close
// enough (see config.h's comment on GRAVITY_CONTACT_SEARCH_ITERATIONS).
// Shared by the plain G keypress (one SIMULATION_GRAVITY_STEP per press, or
// per Windows auto-repeat tick while held) and the Shift+G auto-gravity
// timer (an accelerating step, see autoGravityVelocity below) -- see
// WM_KEYDOWN/WM_TIMER below. Returns TRUE if the step was blocked (i.e. it
// landed on something this tick), so auto-gravity's timer handler knows
// when to reset its velocity back to 0.
// Debug-print-only helper: whichever robot is active, some single "how
// high up is it" number for the [GRAVITY] log lines below. Rocky's torso
// position is bodyY rather than y (see app.h) -- Stilo happens to share
// Semni's exact "y" field name/convention already, needing no branch.
static float activeRobotDebugY(void)
{
    if (app.robotScene.activeKind == ROBOT_KIND_ROCKY)
        return app.robotScene.rocky.bodyY;
    if (app.robotScene.activeKind == ROBOT_KIND_STILO)
        return app.robotScene.stilo.y;
    return app.robotScene.robot.y;
}

// Same idea as activeRobotDebugY just above, but for the whole-body
// rotation angle instead -- used by the VK_LEFT/RIGHT rotation HUD
// (canvasRenderFrame) to show whichever robot is actually active right
// now. All three kinds name this field "angle" (app.h), just on
// different structs, so this is the one place that has to know that.
static float activeRobotAngleDegrees(void)
{
    if (app.robotScene.activeKind == ROBOT_KIND_ROCKY)
        return app.robotScene.rocky.angle;
    if (app.robotScene.activeKind == ROBOT_KIND_STILO)
        return app.robotScene.stilo.angle;
    return app.robotScene.robot.angle;
}

// How much heavier/lighter than the 1.0f baseline the active robot's real
// mass is -- multiplies straight into both gravity paths below (the plain
// G nudge's step size, and auto-gravity's acceleration + terminal-velocity
// cap) so a heavier robot actually falls faster, not just exports a bigger
// number. All three kinds now have their own real total-mass field
// (actualWeight -- Rocky's own SemniExport/StiloExport-flavored siblings,
// see save.c's saveSemniAsRobLeg/saveStiloAsRobLeg for the other place
// each one is read), each defaulted to 1.0f (app_init.c), so a fresh
// robot of any kind still sitting at its default weight is a no-op here,
// same as before this was extended past Rocky. Clamped away from 0/
// negative so a blank or garbage Weight box (wcstod returns 0.0f for
// that) can't freeze or reverse the fall instead of just falling slowly.
static float activeRobotWeightFactor(void)
{
    float w = 1.0f;
    switch (app.robotScene.activeKind)
    {
        case ROBOT_KIND_ROCKY:
            w = app.robotScene.rocky.actualWeight;
            break;
        case ROBOT_KIND_STILO:
            w = app.robotScene.stilo.actualWeight;
            break;
        case ROBOT_KIND_SEMNI:
        default:
            w = app.robotScene.robot.actualWeight;
            break;
    }

    if (w < 0.05f)
        w = 0.05f;

    return w;
}

static BOOL applyGravityStep(HWND hWnd, float step)
{
    // wasLanded persists across calls (it's a local static): TRUE means the
    // robot was already resting as of the last call, so this tick's prints
    // (and any nested [COLLIDE] prints) are suppressed unless something
    // actually changes -- see gSuppressGravityDebug's own comment above.
    static BOOL wasLanded = FALSE;
    BOOL suppressThisCall = wasLanded;
    gSuppressGravityDebug = suppressThisCall;

    float yBefore = activeRobotDebugY();
    translateActiveRobot(0.0f, -step);

    BOOL landed = robotCollidesWithEnvironment();
    if (!suppressThisCall)
        printf("[GRAVITY] step=%.6f y_before=%.6f y_after_step=%.6f landed=%d\n",
               step, yBefore, activeRobotDebugY(), landed);
    if (landed)
    {
        // Back out to the last known-safe position (before this step), then
        // binary-search the largest downward offset within [0, step] that
        // doesn't collide, converging on the true contact point instead of
        // leaving a whole-step-sized gap above it.
        translateActiveRobot(0.0f, step);

        float safe = 0.0f;      // largest offset confirmed NOT to collide
        float blocked = step;   // smallest offset confirmed TO collide

        for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
        {
            float mid = (safe + blocked) * 0.5f;

            translateActiveRobot(0.0f, -mid);
            BOOL hit = robotCollidesWithEnvironment();
            translateActiveRobot(0.0f, mid); // undo probe

            if (hit) blocked = mid;
            else     safe = mid;
        }

        translateActiveRobot(0.0f, -safe);

        if (!suppressThisCall)
        {
            printf("[GRAVITY]   search: safe=%.6f blocked=%.6f y_final=%.6f\n",
                   safe, blocked, activeRobotDebugY());

            // envHalfWidth: the SAME value simEnvLineHalfWidthWorld() gives
            // collision (pointCollidesWithAnyEnvironmentStroke) and the
            // renderer (the Comparison Mode ribbon) right now -- printed
            // once here so it's visible per-run without having to
            // reverse-engineer it from eRadius/combined below.
            //
            // Per-body-part breakdown -- generalized to all three kinds so
            // a gap can be diagnosed no matter which robot is active.
            // Reports dist/combined/gap instead of a bottom_y estimate:
            // dist is the TRUE geometric distance to the nearest traced
            // segment (nearestEnvDistance, no padding applied), combined
            // is exactly the combinedRadius the real collision check used
            // for that same part, and gap = dist - combined is the signed
            // result -- positive means still floating by that many world
            // units, negative means overlapping by that many, ~0.00000
            // means flush. This replaces a bottom_y print that assumed a
            // flat horizontal line and that went stale the moment
            // simEnvLineHalfWidthWorld's env-side padding was added to
            // collision without a matching update here -- gap is computed
            // from the exact same inputs the real check uses, so it can't
            // drift out of sync with the actual physics again the same
            // way, and it works for a sloped or curved line just as well
            // as a flat one.
            float envHalfWidth = simEnvLineHalfWidthWorld();
            printf("[GRAVITY]   envHalfWidth=%.5f\n", envHalfWidth);

            if (app.robotScene.activeKind == ROBOT_KIND_SEMNI)
            {
                CircleSegment dbgCircles[NUM_ROBOT_BODY_CIRCLES];
                computeSemniBodyCircles(app.robotScene.robot, dbgCircles);
                static const char* dbgNames[NUM_ROBOT_BODY_CIRCLES] = { "head", "butt", "hip", "knee", "foot" };
                for (int c = 0; c < NUM_ROBOT_BODY_CIRCLES; c++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(dbgCircles[c].center.x, dbgCircles[c].center.y, &ecx, &ecy);
                    float eRadius = robotLengthToEnvWorld(dbgCircles[c].radius);
                    float combined = eRadius + envHalfWidth;
                    float dist = nearestEnvDistance(ecx, ecy);
                    printf("[GRAVITY]   circle=%-4s world=(%.5f,%.5f) radius=%.5f dist=%.5f combined=%.5f gap=%.5f\n",
                           dbgNames[c], ecx, ecy, eRadius, dist, combined, dist - combined);
                }
            }
            else if (app.robotScene.activeKind == ROBOT_KIND_ROCKY)
            {
                CircleSegment dbgCircles[NUM_ROCKY_BODY_CIRCLES];
                computeRockyBodyCircles(app.robotScene.rocky, dbgCircles);
                static const char* dbgNames[NUM_ROCKY_BODY_CIRCLES] = { "knee", "foot" };
                for (int c = 0; c < NUM_ROCKY_BODY_CIRCLES; c++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(dbgCircles[c].center.x, dbgCircles[c].center.y, &ecx, &ecy);
                    float eRadius = robotLengthToEnvWorld(dbgCircles[c].radius);
                    float combined = eRadius + envHalfWidth;
                    float dist = nearestEnvDistance(ecx, ecy);
                    printf("[GRAVITY]   circle=%-4s world=(%.5f,%.5f) radius=%.5f dist=%.5f combined=%.5f gap=%.5f\n",
                           dbgNames[c], ecx, ecy, eRadius, dist, combined, dist - combined);
                }

                // No eArcThickness-style stand-in here anymore -- see
                // the matching comment on the real check in
                // robotCollidesWithEnvironment (Rocky branch) for why: a
                // rectangle edge is a real filled boundary, not a bare
                // curve, so it needs zero extra robot-side padding, same
                // as a circle's own real radius.
                float edgeCombined = envHalfWidth;
                RockyEdgeSegment dbgEdges[NUM_ROCKY_RECT_SEGMENTS];
                computeRockyRectSegments(app.robotScene.rocky, dbgEdges);
                static const char* edgeNames[NUM_ROCKY_RECT_SEGMENTS] = { "top", "right", "bottom", "left" };
                for (int e = 0; e < NUM_ROCKY_RECT_SEGMENTS; e++)
                {
                    float esx, esy, eex, eey;
                    robotPointToEnvWorld(dbgEdges[e].start.x, dbgEdges[e].start.y, &esx, &esy);
                    robotPointToEnvWorld(dbgEdges[e].end.x, dbgEdges[e].end.y, &eex, &eey);

                    // Same per-sample walk robotCollidesWithEnvironment
                    // actually does for this edge (ARC_SAMPLE_COUNT points
                    // along it, not just the two endpoints) -- reports
                    // whichever sample is closest, so a rotated/tilted
                    // edge's true nearest point is what gets measured,
                    // not just wherever its corners happen to land.
                    float minDist = -1.0f;
                    for (int i = 0; i < ARC_SAMPLE_COUNT; i++)
                    {
                        float t = (float)i / (float)(ARC_SAMPLE_COUNT - 1);
                        float lx = dbgEdges[e].start.x + (dbgEdges[e].end.x - dbgEdges[e].start.x) * t;
                        float ly = dbgEdges[e].start.y + (dbgEdges[e].end.y - dbgEdges[e].start.y) * t;
                        float ecx, ecy;
                        robotPointToEnvWorld(lx, ly, &ecx, &ecy);
                        float d = nearestEnvDistance(ecx, ecy);
                        if (minDist < 0.0f || d < minDist) minDist = d;
                    }

                    printf("[GRAVITY]   edge=%-6s world=(%.5f,%.5f)-(%.5f,%.5f) dist=%.5f combined=%.5f gap=%.5f\n",
                           edgeNames[e], esx, esy, eex, eey, minDist, edgeCombined, minDist - edgeCombined);
                }
            }
            else if (app.robotScene.activeKind == ROBOT_KIND_STILO)
            {
                CircleSegment dbgCircles[NUM_STILO_BODY_CIRCLES];
                computeStiloBodyCircles(app.robotScene.stilo, dbgCircles);
                static const char* dbgNames[NUM_STILO_BODY_CIRCLES] = { "butt", "head", "hip1", "feet1", "hip2", "feet2" };
                for (int c = 0; c < NUM_STILO_BODY_CIRCLES; c++)
                {
                    float ecx, ecy;
                    robotPointToEnvWorld(dbgCircles[c].center.x, dbgCircles[c].center.y, &ecx, &ecy);
                    float eRadius = robotLengthToEnvWorld(dbgCircles[c].radius);
                    float combined = eRadius + envHalfWidth;
                    float dist = nearestEnvDistance(ecx, ecy);
                    printf("[GRAVITY]   circle=%-5s world=(%.5f,%.5f) radius=%.5f dist=%.5f combined=%.5f gap=%.5f\n",
                           dbgNames[c], ecx, ecy, eRadius, dist, combined, dist - combined);
                }
            }
        }

        // Plain gravity + contact only: once the fall is stopped by the
        // binary search above, the robot stays exactly where it landed.
        // (Previously this also incrementally rotated the whole body to
        // align with the ground slope every landed tick -- removed because
        // that per-tick lean + re-settle read as a visible bounce/wobble
        // instead of a clean stop.)
        //
        // Rocky specifically gets one more thing here, called EVERY tick
        // this block runs (by explicit request this is continuous, unlike
        // the removed slope-alignment idea referenced just above): if the
        // knee was dragged far from the rectangle (the knee-detach
        // feature, app.h's own comment on draggingRockyKnee), the LEG can
        // land on something while the body itself is still floating well
        // above the ground -- see advanceRockySettle's own comment for
        // why running it every tick is safe here (no velocity/damping
        // state to oscillate, unlike the old knee pendulum), and how it
        // bends the knee AND rotates the rectangle a little further each
        // call (by explicit request, so the body can tip instead of only
        // ever sinking straight down) until there's nowhere left to go --
        // advanceRockySettle stops re-probing on its own once that
        // happens (rockySettleConverged), which is what actually keeps
        // this from reading as endless vibration.
        //
        // !suppressThisCall here means a FRESH landing (wasLanded was
        // FALSE going into this call) -- reset the convergence flag AND
        // both probes' step sizes right at that moment so a robot that's
        // lifted off (E/Q, a drag, or just walking off a ledge) and lands
        // again later still gets its own full settle pass at full
        // resolution, instead of staying permanently "already converged"
        // (or stuck at whatever fine step size it had refined down to)
        // from whatever the PREVIOUS landing settled into.
        if (!suppressThisCall)
        {
            rockySettleConverged = FALSE;
            rockyKneeSettleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
            rockyBodySettleStep = SIMULATION_BODY_SETTLE_STEP_DEG;
        }

        advanceRockySettle();
    }

    wasLanded = landed;
    InvalidateRect(hWnd, NULL, FALSE);
    return landed;
}

// Finds which segment's drawn arc strip is closest to a world-space point,
// within a small pick tolerance. Returns -1 if nothing is close enough.
static int findHoveredSegment(float wx, float wy)
{
    float tolerance = 0.05f * canvas.zoom; // pick radius, world units
    int best = -1;
    float bestDist = tolerance;

    for (int s = 0; s < canvas.segmentResultCount; s++)
    {
        int start = segmentStarts[s];
        int count = segmentCounts[s];
        if (count < 2) continue;

        for (int i = 0; i < count - 1; i++)
        {
            float ax = segmentPointsWorld[(start + i) * 2];
            float ay = segmentPointsWorld[(start + i) * 2 + 1];
            float bx = segmentPointsWorld[(start + i + 1) * 2];
            float by = segmentPointsWorld[(start + i + 1) * 2 + 1];

            float d = distPointToSegment(wx, wy, ax, ay, bx, by);
            if (d < bestDist) {
                bestDist = d;
                best = s;
            }
        }
    }

    return best;
}

// Finds the nearest stroke START or END point to a world-space point,
// within a small pick tolerance - used to let a new stroke snap onto
// where a previous one left off (chaining straight lines end-to-end).
// Only strokes belonging to the currently active design layer are
// considered, so this stays consistent with what's actually editable.
// Returns TRUE and fills outX/outY if something is close enough.
static BOOL findNearestStrokeEndpoint(float wx, float wy, float* outX, float* outY)
{
    float tolerance = 0.05f * canvas.zoom; // pick radius, world units - matches segment picking
    BOOL found = FALSE;
    float bestDist = tolerance;

    for (int s = 0; s < canvas.strokeCount; s++)
    {
        if (strokeLayer[s] != designLayer) continue;

        int start = strokeStarts[s];
        int end = (s == canvas.strokeCount - 1) ? canvas.pointCount : strokeStarts[s + 1];
        int count = (end - start) / 2;
        if (count < 1) continue;

        float candidates[2][2] = {
            { points[start],     points[start + 1] },     // stroke start
            { points[end - 2],   points[end - 1] },        // stroke end
        };

        for (int c = 0; c < 2; c++)
        {
            float dx = wx - candidates[c][0];
            float dy = wy - candidates[c][1];
            float d = sqrtf(dx * dx + dy * dy);
            if (d < bestDist)
            {
                bestDist = d;
                found = TRUE;
                *outX = candidates[c][0];
                *outY = candidates[c][1];
            }
        }
    }

    return found;
}

// In Simulation mode, the ArcSpline environment is driven by sim_camera's
// own independent zoom instead of canvas.zoom -- see sim_camera.h for why
// (keeps the environment and the robot zooming together as one scene,
// without disturbing canvas.zoom, which Design > Environment mode still
// owns and returns to exactly as it left it).
//
// NOTE on the conversion below: canvas.zoom follows a "bigger = zoomed
// OUT" convention (it's used directly as the ortho half-extent multiplier
// -- see the non-simulation branch), while sim_camera's zoom follows the
// opposite "bigger = zoomed IN" convention (same as graphics.c's g_zoom,
// so the robot's own applyProjection can share it via a plain
// ROBOT_VIEW_HALF_EXTENT/zoom -- see sim_camera.h). Inverting it here
// (1.0f / simCameraGetZoom(), 1.0
// being canvas's own base half-extent at zoom=1) is what lets a single
// simCameraZoom() call still zoom the environment and the robot in the
// same direction together, despite the two subsystems' projections having
// opposite native conventions.
void UpdateProjection(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)glWindowWidth / (float)glWindowHeight;
    float zoom = (appMode == APP_MODE_SIMULATION) ? (1.0f / simCameraGetZoom()) : canvas.zoom;
    if (aspect >= 1.0f) {
        glOrtho(-aspect * zoom, aspect * zoom, -1.0f * zoom, 1.0f * zoom, -1.0f, 1.0f);
    } else {
        glOrtho(-1.0f * zoom, 1.0f * zoom, -(1.0f / aspect) * zoom, (1.0f / aspect) * zoom, -1.0f, 1.0f);
    }
    glMatrixMode(GL_MODELVIEW);
}

// NEW: periodic check for the "hover top-right corner to reveal UI panel" behavior
#define UI_HOTZONE_TIMER_ID 1001
#define UI_HOTZONE_INTERVAL_MS 16    // ~60Hz - also drives the fade animation smoothness
#define UI_HOTZONE_WIDTH  48
#define UI_HOTZONE_HEIGHT 48
#define UI_FADE_STEP 18              // alpha change per tick (~14 ticks, ~230ms, to fully fade)

// Simulation mode's "auto gravity" (Shift+G toggle -- see WM_KEYDOWN) --
// its own dedicated timer, separate from UI_HOTZONE_TIMER_ID above (that
// one's tied to the corner-reveal panel, an unrelated feature; sharing it
// here would couple the two for no reason). TRUE while a G press should
// keep repeating on its own, without the user having to hold the key down.
#define AUTO_GRAVITY_TIMER_ID 1002
static BOOL autoGravityActive = FALSE;

// TRUE for as long as a rotate-release topple (WM_KEYUP's VK_LEFT/RIGHT
// case below) still has real work left to do. Completely independent of
// autoGravityActive/Shift+G -- see advancePostRotateSettle's own comment
// for why this needed its own flag instead of just flipping that one on.
static BOOL postRotateSettleActive = FALSE;

// simTimeScale/hSlowMotionBtn ("Slow Motion") are declared up near hRC/hDC
// instead of here -- applyGravityStep, defined earlier in this file, needs
// to read simTimeScale already. See that declaration's comment.

// Auto gravity's current fall speed, in world units PER MILLISECOND (real
// time, not "per tick" -- see advanceAutoGravity below for why). Ramps up
// from 0 by SIMULATION_AUTO_GRAVITY_ACCEL_PER_MS2 * elapsed-ms each time
// it's advanced, capped at SIMULATION_AUTO_GRAVITY_MAX_VELOCITY_PER_MS
// (config.h), so the fall actually accelerates instead of moving at one
// flat speed (see config.h's comment on why a flat speed kept reading as
// slow motion). Reset to 0 both when auto gravity is freshly toggled on
// (WM_KEYDOWN) and the instant applyGravityStep reports it landed
// (advanceAutoGravity) -- either way, the next fall should start from
// rest, not carry over speed from before.
static float autoGravityVelocity = 0.0f;


// Real-time (GetTickCount) timestamp of the last advanceAutoGravity call --
// lets it compute how much real time actually elapsed since last time,
// rather than assuming a fixed tick length. See advanceAutoGravity's own
// comment for why this matters.
static DWORD autoGravityLastTickTime = 0;

// Advances auto gravity by however much real time has actually passed
// since the last call, instead of assuming a fixed SIMULATION_AUTO_GRAVITY_
// INTERVAL_MS worth of time. This is what it is because of a real bug:
// WM_TIMER is a LOW-PRIORITY Windows message -- the OS only synthesizes one
// once the thread's message queue would otherwise be empty -- so a steady
// stream of WM_MOUSEMOVE messages while the user is actively moving the
// cursor can starve AUTO_GRAVITY_TIMER_ID for as long as the mouse keeps
// moving, visibly pausing the fall. Fixing that means this can't assume
// "one call == one fixed-size step" anymore, since calls may now arrive
// far apart (WM_TIMER alone, mouse held still) or very close together
// (WM_MOUSEMOVE flooding in) -- so both the acceleration and the resulting
// step are scaled by the real elapsed milliseconds instead. Called from
// WM_TIMER's AUTO_GRAVITY_TIMER_ID case (the fallback, steady baseline) AND
// directly from WM_MOUSEMOVE (so mouse movement itself keeps driving the
// fall forward instead of blocking it) -- both funnel through here so the
// physics can never disagree no matter which message actually triggered it.
static void advanceAutoGravity(HWND hWnd)
{
    if (!autoGravityActive || appMode != APP_MODE_SIMULATION) return;

    // While the user is manually dragging the robot, let THEM be the one
    // moving it -- silently skip stepping instead of fighting the drag
    // (or, worse, applying a big catch-up step once the drag ends). This
    // deliberately leaves autoGravityActive/the toast untouched -- it's a
    // temporary pause, not a toggle-off, so nothing shows on screen and
    // Shift+G's own state doesn't change. WM_LBUTTONUP resets
    // autoGravityVelocity/autoGravityLastTickTime once the drag ends, so
    // the fall resumes fresh from rest rather than picking up wherever it
    // left off (the robot may have just been moved somewhere completely
    // different) or jumping by however long the drag itself lasted.
    if (app.draggingRobotSim) return;

    DWORD now = GetTickCount();
    DWORD elapsed = now - autoGravityLastTickTime;
    if (elapsed == 0) return; // already advanced this exact millisecond

    if (elapsed > SIMULATION_AUTO_GRAVITY_MAX_DT_MS)
        elapsed = SIMULATION_AUTO_GRAVITY_MAX_DT_MS; // avoid a big catch-up jump after a long stall

    autoGravityLastTickTime = now;

    // Cap against real elapsed time FIRST, then scale down for Slow Motion
    // -- capping the already-scaled value would let a real stall (elapsed
    // > MAX_DT_MS) sneak back under the cap purely because it got divided
    // down, defeating the whole point of the cap.
    float simElapsed = (float)elapsed * simTimeScale;

    // Both the acceleration and the terminal-velocity cap it climbs
    // toward are scaled by the same weight factor (see
    // activeRobotWeightFactor's own comment) -- heavier ramps up faster
    // AND settles into a higher top speed, lighter does the opposite,
    // exactly mirroring how the plain G nudge above scales its step.
    float weightFactor = activeRobotWeightFactor();

    autoGravityVelocity += SIMULATION_AUTO_GRAVITY_ACCEL_PER_MS2 * weightFactor * simElapsed;

    float maxVelocity = SIMULATION_AUTO_GRAVITY_MAX_VELOCITY_PER_MS * weightFactor;
    if (autoGravityVelocity > maxVelocity)
        autoGravityVelocity = maxVelocity;

    float step = autoGravityVelocity * simElapsed;

    // A single velocity-scaled step here can be MUCH bigger than the
    // plain G press's fixed SIMULATION_GRAVITY_STEP -- up to 100x, at the
    // velocity/elapsed-time caps just above (SIMULATION_AUTO_GRAVITY_MAX_
    // VELOCITY_PER_MS * SIMULATION_AUTO_GRAVITY_MAX_DT_MS) -- which is
    // exactly what a real stall produces (a burst of WM_MOUSEWHEEL zoom
    // events is a common real trigger: each one forces a synchronous
    // UpdateWindow that can starve AUTO_GRAVITY_TIMER_ID for a while, so
    // the next tick's catch-up elapsed time balloons toward its cap).
    // applyGravityStep's own GRAVITY_CONTACT_SEARCH_ITERATIONS binary
    // search only narrows the gap AFTER it already detected a collision
    // at the far end of a step -- it does nothing for a step that's
    // simply bigger than the ground line's own thickness, which can
    // carry the robot from cleanly above a thin line to cleanly below it
    // in one jump, with the end-of-step check reporting "not touching"
    // because it's already through to the other side. That's tunneling,
    // not a resting-precision problem, and no amount of searching WITHIN
    // one oversized step can catch it. Splitting into a sequence of
    // SIMULATION_GRAVITY_STEP-sized sub-steps -- the same size a plain G
    // press already uses without this problem -- guarantees the
    // environment gets tested at least once per that increment on the
    // way down, so a thin line can never be skipped over unnoticed no
    // matter how large the originally requested step was.
    float remaining = step;
    BOOL landedThisTick = FALSE;

    while (remaining > 0.0f)
    {
        float subStep = (remaining > SIMULATION_GRAVITY_STEP) ? SIMULATION_GRAVITY_STEP : remaining;
        remaining -= subStep;

        if (applyGravityStep(hWnd, subStep))
        {
            landedThisTick = TRUE;
            break;
        }
    }

    if (landedThisTick)
        autoGravityVelocity = 0.0f; // landed -- next fall starts from rest

    // Rocky's knee-to-foot pendulum used to run here (an automatic damped
    // swing while the foot wasn't resting) -- removed by explicit request
    // after it kept producing hard-to-diagnose "stuck"/embedded states on
    // top of the manual E/Q rotation. E/Q (WM_KEYDOWN below) is now the
    // only thing that moves Rocky's kneeAngle in Simulation mode; this
    // function only ever drives the whole-body drop above.
}

// After a manual Left/Right whole-body rotate (WM_KEYUP below) is
// released, the robot might now be resting on a corner/edge that's no
// longer actually stable (rotated past its own tipping point), or even
// lifted clean off the ground by the rotation itself. Auto Gravity
// (Shift+G) already knows how to resolve either case -- keep falling
// until landed, then let advanceRockySettle nudge the rest of the way --
// but the user shouldn't have to remember to turn that toggle on just to
// see a rotate-triggered topple finish playing out. This is a separate,
// self-terminating driver for exactly that: it never touches
// autoGravityActive or its toast, so the persistent Auto Gravity toggle
// stays exactly whatever the user last set it to, and it shuts its own
// timer back off once there's nothing left to do (rather than leaving a
// tiny "is anything still moving" check running forever).
static void advancePostRotateSettle(HWND hWnd)
{
    if (!postRotateSettleActive || appMode != APP_MODE_SIMULATION) return;

    // Same reasoning as advanceAutoGravity's own check just above -- don't
    // fight a manual drag in progress.
    if (app.draggingRobotSim) return;

    // Fixed SIMULATION_GRAVITY_STEP per tick (same size a plain G press
    // uses) rather than auto-gravity's own accelerating velocity ramp --
    // this is meant to read as "settling into place" after a rotate, not
    // "falling from height", and a topple is normally a short trip
    // either way.
    BOOL landed = applyGravityStep(hWnd, SIMULATION_GRAVITY_STEP * activeRobotWeightFactor());

    // Done once there's nothing left for gravity OR (for Rocky) the
    // settle probes to improve -- the same "fully at rest" condition the
    // rest of this file already tracks (rockySettleConverged), so this
    // can't spin forever on a shape that's genuinely settled. Semni/Stilo
    // have no equivalent settle system, so plain `landed` is already the
    // right stop condition for them.
    BOOL atRest = landed && (app.robotScene.activeKind != ROBOT_KIND_ROCKY || rockySettleConverged);

    if (atRest)
    {
        postRotateSettleActive = FALSE;
        rockyKneeSettleSuppressed = FALSE; // this session (E/Q or rotate) is over either way

        // Don't rip the timer out from under a separately-active Auto
        // Gravity session -- only stop it if this was the only reason it
        // was still running.
        if (!autoGravityActive)
            KillTimer(hWnd, AUTO_GRAVITY_TIMER_ID);
    }
}

// ---- Scripted gait ("Walk" toggle, Shift+W -- see config.h's
// SIMULATION_GAIT_* comment block for the overall rationale) ----

#define AUTO_GAIT_TIMER_ID 1003
static BOOL gaitActive = FALSE;

// "WALK ON"/"WALK OFF" toast (Shift+W -- see ToggleGait below) -- same
// stateless-fade pattern as gravityToastOn/gravityToastStartTick further
// down this file, kept as its own separate pair (not reusing the gravity
// toast's variables) so auto-gravity and Walk can each show/fade their own
// toast independently without one stomping the other if both get toggled
// close together. Drawn one line above the gravity toast (canvasRenderFrame)
// so both can be visible at once without overlapping. Declared up here,
// ahead of gravityToastOn/gravityToastStartTick's own declaration, because
// ToggleGait -- defined right below -- already needs to write these.
static BOOL gaitToastOn = FALSE;
static DWORD gaitToastStartTick = 0;

// "ROTATION: NN" HUD, shown live while VK_LEFT/RIGHT is held in
// Simulation mode (see WM_KEYDOWN/WM_KEYUP's own VK_LEFT/RIGHT branches
// below), reading whichever robot is active via activeRobotAngleDegrees.
// Not quite the same shape as the two stateless-fade toasts above: while
// the key is actually held, this shows continuously at full opacity (no
// hold/fade timing at all) so it tracks the angle live, one substep at a
// time, for as long as the user keeps rotating; the SAME
// hold-then-fade pattern only kicks in once the key is released
// (rotateKeyHeld goes FALSE and rotateToastStartTick is stamped), so the
// last angle shown doesn't just vanish the instant the key comes up.
// Drawn one line above the Walk toast (canvasRenderFrame) so all three
// can be visible together without overlapping.
static BOOL rotateKeyHeld = FALSE;
static DWORD rotateToastStartTick = 0;

// The robot's exact pose the instant Walk was toggled on -- every gait
// tick restores this in full, then reapplies that tick's interpolated
// deltas on top (see advanceGait), rather than nudging the live pose
// incrementally tick after tick. Recomputing from this fixed source of
// truth every time means the cycle can never drift or accumulate
// floating-point error over a long walk, the same reasoning
// updateDrawingPoint's shift-line preview truncate-and-rebuild uses.
static Semni gaitBaseline;

// Continuously increasing (never wrapped/reset) -- one full unit is one
// complete crouch-push-swing-land cycle. advanceGait derives two different
// things from it: fmodf(gaitPhaseUnwrapped, 1.0f) for looking up where in
// the cycle the joints/hop-bob currently are (that part DOES need to
// repeat every cycle), and gaitPhaseUnwrapped directly (unwrapped) for how
// far forward the robot has walked (that part must NOT reset every cycle,
// or it would hop in place instead of advancing) -- see advanceGait's own
// comment for the derivation.
static float gaitPhaseUnwrapped = 0.0f;

// Real-time (GetTickCount) timestamp of the last advanceGait call -- same
// "measure actual elapsed time, don't assume a fixed tick length" reason
// autoGravityLastTickTime exists (see advanceAutoGravity's comment).
static DWORD gaitLastTickTime = 0;

// One keyframe in the gait cycle -- see config.h's SIMULATION_GAIT_*
// comment block for the overall design. Every field except `phase` is a
// DELTA from gaitBaseline, not an absolute value, so the cycle always
// swings around whatever pose the robot actually had when Walk was
// switched on instead of assuming a fixed geometry.
typedef struct {
    float phase;            // 0..1, strictly ascending; first entry MUST be 0.0, last MUST be 1.0
    float hipAngleDeltaDeg;
    float kneeAngleDeltaDeg;
    float bodyAngleDeltaDeg;
    float hopY;              // world units, relative to gaitBaseline.y
} GaitKeyframe;

// A single-leg hop/pivot cycle: crouch (load up), push off (extend and
// rise), swing the leg forward through the air (peak height), land back at
// neutral to close the loop. First-pass numbers (SIMULATION_GAIT_* in
// config.h) -- not visually tuned yet, see that block's comment. If a
// swing reads as going the wrong way once this is actually visible, the
// fix is almost certainly flipping one of these signs, not the structure.
static const GaitKeyframe gaitCycle[] = {
    // phase   hipDelta                          kneeDelta                          bodyDelta                          hopY
    { 0.00f,   0.0f,                              0.0f,                              0.0f,                              0.0f },
    { 0.20f,  -SIMULATION_GAIT_HIP_SWING_DEG*0.3f, SIMULATION_GAIT_KNEE_BEND_DEG,    -SIMULATION_GAIT_BODY_LEAN_DEG*0.5f, -SIMULATION_GAIT_HOP_HEIGHT*0.25f }, // crouch/load
    { 0.45f,   SIMULATION_GAIT_HIP_SWING_DEG*0.5f,-SIMULATION_GAIT_KNEE_BEND_DEG*0.7f, SIMULATION_GAIT_BODY_LEAN_DEG,      SIMULATION_GAIT_HOP_HEIGHT*0.6f  }, // push off
    { 0.70f,  -SIMULATION_GAIT_HIP_SWING_DEG,     SIMULATION_GAIT_KNEE_BEND_DEG*0.8f,  SIMULATION_GAIT_BODY_LEAN_DEG*0.3f, SIMULATION_GAIT_HOP_HEIGHT       }, // swing forward, peak height
    { 1.00f,   0.0f,                              0.0f,                              0.0f,                              0.0f },  // land -- same as phase 0.00, closes the loop
};
#define GAIT_KEYFRAME_COUNT (sizeof(gaitCycle) / sizeof(gaitCycle[0]))

// Linearly interpolates the gait cycle at the given phase (0..1, already
// wrapped -- see advanceGait). Walks the table for the bracketing pair of
// keyframes rather than assuming a fixed count/spacing, so gaitCycle's
// entries above can be freely added to/re-timed later without touching
// this function.
static void sampleGaitCycle(float phase, float* hipDeg, float* kneeDeg, float* bodyDeg, float* hopY)
{
    for (unsigned int i = 0; i + 1 < GAIT_KEYFRAME_COUNT; i++)
    {
        const GaitKeyframe* a = &gaitCycle[i];
        const GaitKeyframe* b = &gaitCycle[i + 1];

        if (phase >= a->phase && phase <= b->phase)
        {
            float span = b->phase - a->phase;
            float t = (span > 1e-6f) ? (phase - a->phase) / span : 0.0f;

            *hipDeg  = a->hipAngleDeltaDeg  + (b->hipAngleDeltaDeg  - a->hipAngleDeltaDeg)  * t;
            *kneeDeg = a->kneeAngleDeltaDeg + (b->kneeAngleDeltaDeg - a->kneeAngleDeltaDeg) * t;
            *bodyDeg = a->bodyAngleDeltaDeg + (b->bodyAngleDeltaDeg - a->bodyAngleDeltaDeg) * t;
            *hopY    = a->hopY              + (b->hopY              - a->hopY)              * t;
            return;
        }
    }

    // Shouldn't happen (phase is always in [0,1] and the table spans
    // [0,1]) -- fall back to the neutral pose rather than leaving these
    // uninitialized if it ever does.
    *hipDeg = 0.0f; *kneeDeg = 0.0f; *bodyDeg = 0.0f; *hopY = 0.0f;
}

// Advances the gait cycle by however much real time has actually passed
// since the last call -- same real-time-delta approach advanceAutoGravity
// uses, and for the same reason (WM_TIMER's low priority, see that
// function's comment); called from the same two places (WM_TIMER's
// AUTO_GAIT_TIMER_ID case and directly from WM_MOUSEMOVE) so mouse
// movement can't stall the walk either.
static void advanceGait(HWND hWnd)
{
    if (!gaitActive || appMode != APP_MODE_SIMULATION) return;

    // Same reasoning as advanceAutoGravity's own drag-pause: let the user's
    // drag win outright rather than fighting it or catching up afterward.
    if (app.draggingRobotSim) return;

    DWORD now = GetTickCount();
    DWORD elapsed = now - gaitLastTickTime;
    if (elapsed == 0) return;

    if (elapsed > SIMULATION_AUTO_GRAVITY_MAX_DT_MS)
        elapsed = SIMULATION_AUTO_GRAVITY_MAX_DT_MS; // same stall-guard auto-gravity uses

    gaitLastTickTime = now;

    float simElapsed = (float)elapsed * simTimeScale; // Slow Motion, same as advanceAutoGravity

    gaitPhaseUnwrapped += simElapsed / SIMULATION_GAIT_CYCLE_MS;

    float phase = fmodf(gaitPhaseUnwrapped, 1.0f);
    float hipDeg, kneeDeg, bodyDeg, hopY;
    sampleGaitCycle(phase, &hipDeg, &kneeDeg, &bodyDeg, &hopY);

    // Forward progress is the UNWRAPPED phase times the per-cycle step
    // length -- e.g. after 2.5 cycles, gaitPhaseUnwrapped is 2.5, so the
    // robot has moved 2.5 step-lengths forward, continuously and without
    // any special-casing at each loop boundary (unlike hipDeg/kneeDeg/
    // bodyDeg/hopY above, which DO reset every cycle via fmodf's wrap).
    float forwardX = gaitPhaseUnwrapped * SIMULATION_GAIT_STEP_LENGTH;

    app.robotScene.robot = gaitBaseline;
    app.robotScene.robot.hipAngle  += hipDeg;
    app.robotScene.robot.kneeAngle += kneeDeg;
    app.robotScene.robot.angle     += bodyDeg;
    translateRobot(&app.robotScene.robot, forwardX, hopY);

    InvalidateRect(hWnd, NULL, FALSE);
}

// Toggles the gait cycle on/off -- shared by Shift+W (WM_KEYDOWN) and a
// direct click on the "Walk" button (WM_COMMAND's ID_WALK_TOGGLE), so the
// two input paths can never drift apart. Captures gaitBaseline (the pose
// the cycle swings around) fresh every time it's switched ON, so
// re-toggling Walk after posing the robot differently restarts the cycle
// around the NEW pose rather than snapping back to whatever it was the
// first time Walk was turned on this session.
static void ToggleGait(HWND hWnd)
{
    gaitActive = !gaitActive;

    if (gaitActive)
    {
        gaitBaseline = app.robotScene.robot;
        gaitPhaseUnwrapped = 0.0f;
        gaitLastTickTime = GetTickCount();
        SetTimer(hWnd, AUTO_GAIT_TIMER_ID, SIMULATION_AUTO_GRAVITY_INTERVAL_MS, NULL);
    }
    else
    {
        KillTimer(hWnd, AUTO_GAIT_TIMER_ID);
    }

    if (hWalkBtn)
        SendMessage(hWalkBtn, BM_SETCHECK, gaitActive ? BST_CHECKED : BST_UNCHECKED, 0);

    gaitToastOn = gaitActive;
    gaitToastStartTick = GetTickCount();
    InvalidateRect(hWnd, NULL, FALSE);
}

// Bottom-left "AUTO GRAVITY ON"/"AUTO GRAVITY OFF" toast (canvasRenderFrame
// draws it, WM_KEYDOWN's Shift+G branch below sets these). Stateless fade:
// gravityToastStartTick is just a GetTickCount() snapshot from the moment
// of the toggle, and the render code recomputes the alpha fresh every
// frame from how long ago that was (SIMULATION_GRAVITY_TOAST_HOLD_MS/
// FADE_MS, config.h) rather than stepping some alpha variable on a timer
// tick -- simpler, and immune to any particular timer's cadence.
static BOOL gravityToastOn = FALSE;       // which message to show: ON vs OFF
static DWORD gravityToastStartTick = 0;   // 0 == no toast pending/visible

// Everything WM_PAINT used to do directly (clear excluded -- that's now
// renderCombinedFrame's job, done once per combined frame rather than once
// per subsystem) -- sets the ArcSpline canvas's own projection and draws
// the whole scene into whatever's currently in the color buffer. dimAmount
// scales every draw call's own alpha (opacity, below) when the ArcSpline
// canvas isn't the currently active editor mode, so it still reads as
// background context instead of vanishing while Semni is active. This is
// deliberately NOT a full-screen overlay drawn on top afterward -- that
// would also darken the shared white canvas background underneath it,
// making an empty canvas look grayed out everywhere instead of just this
// subsystem's own strokes/lines fading. Same "scale this alpha down"
// pattern the file already uses for dimming the non-edited Robot/
// Environment layer against each other (see strokeAlpha/ghostAlpha below).
void canvasRenderFrame(float dimAmount)
{
    float opacity = 1.0f - dimAmount;

    UpdateProjection();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // In Simulation mode, pan through sim_camera instead of canvas.panX/
    // panY -- converted into THIS subsystem's own current half-extent (the
    // exact same aspect/zoom math UpdateProjection above just used) via
    // simCameraGetWorldPan, so it stays in exact pixel-lockstep with
    // however far the robot itself pans (see graphics.c's graphicsGetPan),
    // despite the two subsystems' projections not sharing a base unit or
    // formula.
    if (appMode == APP_MODE_SIMULATION)
    {
        float aspect = (float)glWindowWidth / (float)glWindowHeight;
        float zoom = 1.0f / simCameraGetZoom();
        float halfX, halfY;
        if (aspect >= 1.0f) { halfX = aspect * zoom; halfY = zoom; }
        else                { halfX = zoom; halfY = zoom / aspect; }

        float simPanX, simPanY;
        simCameraGetWorldPan(halfX, halfY, &simPanX, &simPanY);
        glTranslatef(-simPanX, -simPanY, 0.0f);
    }
    else
    {
        glTranslatef(-canvas.panX, -canvas.panY, 0.0f);   // NEW: apply camera pan to everything below
    }

    // Only show background image if NOT in active comparison mode.
    // Deliberately NOT gated on canvas.showSegments - Comparison Mode
    // has to work on its own whether or not "View Segments" is also
    // checked, as long as something has been traced.
    BOOL isComparisonActive = canvas.comparisonMode && canvas.segmentResultCount > 0;

    // Robot layer has no drawable content of its own yet (it's reserved
    // for a separate project to be embedded here later), so no new
    // strokes can be added while it's active (see WM_LBUTTONDOWN).
    // But the Environment layer still renders underneath as a dimmed
    // reference, same as when Environment is dimmed while Robot is
    // hypothetically active in the other direction — only the currently
    // edited layer is shown at full opacity.
    BOOL isRobotLayerActive = (appMode == APP_MODE_DESIGN && designLayer == LAYER_ROBOT);

    if (canvas.hasBackgroundImage && !isComparisonActive)
    {
        // FIXED bounds (computed once at upload time) — canvas.zoom now
        // actually affects this via the ortho projection, same as strokes
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, canvasTexture);
        glColor4f(1.0f, 1.0f, 1.0f, (isRobotLayerActive ? 0.25f : 1.0f) * opacity);

        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(bgLeft,  bgBottom);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(bgRight, bgBottom);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(bgRight, bgTop);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(bgLeft,  bgTop);
        glEnd();

        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Only apply comparison mode (hide/fade strokes) if segments are actually being shown

    if (!isComparisonActive)
    {
        for (int s = 0; s < canvas.strokeCount; s++)
        {
            int start = strokeStarts[s];
            int end = (s == canvas.strokeCount - 1) ? canvas.pointCount : strokeStarts[s + 1];
            int count = (end - start) / 2;
            if (count < 2) continue;

            COLORREF c = strokeColor[s];

            // In Design mode, dim strokes belonging to the layer that
            // isn't currently being edited (Robot vs Environment) so
            // it stays visible as reference without competing with the
            // active layer. Simulation mode shows everything at full
            // opacity.
            float strokeAlpha = 1.0f;
            if (appMode == APP_MODE_DESIGN && strokeLayer[s] != designLayer)
                strokeAlpha = 0.25f;

            glColor4f(GetRValue(c)/255.0f, GetGValue(c)/255.0f, GetBValue(c)/255.0f, strokeAlpha * opacity);

            // Same sim_camera-vs-canvas.zoom fix as the Comparison Mode
            // ribbon below (see its own comment) -- this loop is normally
            // skipped during Simulation (isComparisonActive forces the
            // reconstructed ribbon instead), but Comparison Mode can be
            // unchecked manually while simulating, and canvas.zoom sits
            // frozen the whole time Simulation is active either way, so
            // this stayed wrong for the same reason if that ever happened.
            // Same reasoning as the Comparison Mode ribbon's baseHalfW
            // fix below: in Simulation mode, draw at the SAME half-width
            // collision actually uses (simEnvLineHalfWidthWorld, shared
            // with pointCollidesWithAnyEnvironmentStroke) instead of the
            // stroke's real measured thickness, so a manually-unchecked
            // Comparison Mode still shows contact as flush rather than
            // sinking/floating relative to what collision treats as the
            // surface.
            float effectiveZoomForStroke = (appMode == APP_MODE_SIMULATION) ? (1.0f / simCameraGetZoom()) : canvas.zoom;
            float halfW;
            if (appMode == APP_MODE_SIMULATION)
                halfW = simEnvLineHalfWidthWorld();
            else
                halfW = (strokeThickness[s] * effectiveZoomForStroke) / (float)glWindowWidth;

            glBegin(GL_TRIANGLE_STRIP);
            for (int i = 0; i < count; i++)
            {
                float x = points[start + i * 2];
                float y = points[start + i * 2 + 1];
                float dx = 0.0f, dy = 0.0f;

                if (i == 0) {
                    dx = points[start + (i + 1) * 2] - x;
                    dy = points[start + (i + 1) * 2 + 1] - y;
                } else if (i == count - 1) {
                    dx = x - points[start + (i - 1) * 2];
                    dy = y - points[start + (i - 1) * 2 + 1];
                } else {
                    float dx1 = x - points[start + (i - 1) * 2];
                    float dy1 = y - points[start + (i - 1) * 2 + 1];
                    float dx2 = points[start + (i + 1) * 2] - x;
                    float dy2 = points[start + (i + 1) * 2 + 1] - y;
                    dx = dx1 + dx2;
                    dy = dy1 + dy2;
                }

                float len = sqrtf(dx * dx + dy * dy);
                if (len == 0.0f) len = 1.0f;
                float nx = -dy / len;
                float ny = dx / len;

                glVertex2f(x + nx * halfW, y + ny * halfW);
                glVertex2f(x - nx * halfW, y - ny * halfW);
            }
            glEnd();
        }
    }
    glDisable(GL_BLEND);

    // NEW: endpoint-snap highlight - a bright ring around the stroke
    // endpoint the cursor is currently hovering near, showing the user
    // that starting a new stroke here will snap onto it.
    if (snapEndpointAvailable)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const int ringSteps = 24;
        float ringRadius = 0.02f * canvas.zoom;

        glColor4f(1.0f, 0.55f, 0.0f, 0.9f * opacity);   // orange, matches nothing else on canvas
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < ringSteps; i++)
        {
            float theta = (2.0f * 3.14159265f * i) / ringSteps;
            glVertex2f(snapEndpointX + ringRadius * cosf(theta),
                       snapEndpointY + ringRadius * sinf(theta));
        }
        glEnd();

        // Small filled center dot so the exact snap point is unambiguous
        glColor4f(1.0f, 0.55f, 0.0f, 0.9f * opacity);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(snapEndpointX, snapEndpointY);
        for (int i = 0; i <= ringSteps; i++)
        {
            float theta = (2.0f * 3.14159265f * i) / ringSteps;
            glVertex2f(snapEndpointX + (ringRadius * 0.3f) * cosf(theta),
                       snapEndpointY + (ringRadius * 0.3f) * sinf(theta));
        }
        glEnd();

        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }


	// Rendered when EITHER "View Segments" is checked OR Comparison Mode
	// is active - the two controls are independent, so Comparison Mode
	// must be able to show the traced arcs on its own without also
	// requiring View Segments to be checked.
	if ((canvas.showSegments || isComparisonActive) && canvas.segmentResultCount > 0)
	{
	    glEnable(GL_BLEND);
	    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	    // Comparison Mode HIDES the original strokes entirely while it's on
	    // (see the "!isComparisonActive" guard around the real-stroke render
	    // loop above) -- so the user is judging width by memory/toggling,
	    // not a simultaneous overlay. Full opacity here (was 0.95) matches
	    // the original stroke's own strokeAlpha exactly, so a lower-contrast
	    // ghost line isn't ALSO reading as visually thinner/fainter on top
	    // of whatever the true geometric width difference is -- the actual
	    // width itself comes entirely from segmentAvgRadiusPx below, this
	    // is just making sure alpha doesn't quietly stack another
	    // "apples-to-apples" mismatch on top of it.
	    float ghostAlpha = isComparisonActive ? 1.0f : 0.35f;
	    if (isRobotLayerActive) ghostAlpha *= 0.3f;  // extra-dim: Environment reference while on Robot layer

	    for (int s = 0; s < canvas.segmentResultCount; s++)
	    {
	        int start = segmentStarts[s];
	        int count = segmentCounts[s];
	        if (count < 2) continue;

	        BOOL isHovered = (s == hoveredSegment) && !isRobotLayerActive;   // NEW

	        float r, g, b;
	        if (isComparisonActive) {
	            // Dark grey for comparison mode
	            r = 0.3f;
	            g = 0.3f;
	            b = 0.3f;
	        } else {
	            segmentGhostColor(s, &r, &g, &b);
	        }
	        glColor4f(r, g, b, (isHovered ? 1.0f : ghostAlpha) * opacity);

	        // Per-segment now (used to be one flat ghostHalfW for every
	        // segment) -- same *canvas.zoom/glWindowWidth constant-screen-
	        // pixel-width formula real strokes use just above, applied to
	        // THIS segment's own recovered original radius (segmentAvgRadiusPx,
	        // canvas_bridge.c), so the reconstruction respects each stroke's
	        // actual original thickness instead of rendering every segment
	        // at the same made-up width regardless of how thick it really was.
	        //
	        // The 2x bump is View-Segments-only: that ribbon (not the dashed
	        // ghost CIRCLE outline further below) is the actual colorful
	        // reconstructed line the user sees, and at its original 1x width
	        // the hovered segment's highlight color was hard to make out
	        // against its neighbors. Comparison Mode's whole point is an
	        // apples-to-apples look against the real original stroke, so it
	        // keeps the true 1x width (segmentAvgRadiusPx is already
	        // corrected to match strokeThickness's own units, same as the
	        // untouched original-stroke render above) -- doubling it there
	        // too made the reconstruction visibly thicker than the drawing
	        // it's supposed to be compared against.
	        // In Simulation mode this ribbon is driven by sim_camera's own
	        // independent zoom, not canvas.zoom (same reasoning as
	        // UpdateProjection's own "bigger = zoomed OUT" vs "bigger =
	        // zoomed IN" comment just above in this file) -- canvas.zoom is
	        // deliberately left untouched while simulating (WM_MOUSEWHEEL),
	        // so it just sits frozen at whatever Design > Environment last
	        // left it at. Using it here unconditionally meant this ribbon's
	        // drawn half-width never actually tracked sim_camera zooming in
	        // Simulation mode: scrolling to zoom moved the projection (and
	        // so the robot and the line's own CENTERLINE) correctly, but
	        // this ribbon's thickness stayed anchored to the stale
	        // canvas.zoom, making the line visibly thinner than its real
	        // (correct, zoom-independent) collision half-width the more you
	        // zoomed in -- exactly the "still a gap once I zoom in" symptom,
	        // even after the environment segment data itself was already
	        // correct and unaffected by zoom.
	        // In Simulation mode the ribbon's thickness must match what
	        // collision actually treats as the environment's half-width, or
	        // contact looks wrong even when it's mathematically exact:
	        // robotCollidesWithEnvironment() now pads every environment
	        // surface with a single fixed epsilon (robotLengthToEnvWorld(
	        // SIMULATION_ARC_COLLISION_THICKNESS), see eArcThickness/
	        // eEdgeThickness) instead of the stroke's real measured
	        // thickness (segmentAvgRadiusPx). If this ribbon kept drawing at
	        // the real (usually much larger) measured thickness, a robot
	        // settled flush at the collision centerline would visually sit
	        // inside/under the top of the still-thick-looking line -- the
	        // "went through the line" symptom. Drawing it at the same fixed
	        // epsilon collision uses makes contact look flush instead of
	        // floating or sinking, for any robot kind/size. Design mode is
	        // unaffected: it keeps the original real-thickness look, since
	        // nothing about the drawing there depends on this simulated
	        // collision constant.
	        float effectiveZoomForRender = (appMode == APP_MODE_SIMULATION) ? (1.0f / simCameraGetZoom()) : canvas.zoom;
	        float baseHalfW;
	        if (appMode == APP_MODE_SIMULATION)
	            baseHalfW = simEnvLineHalfWidthWorld();
	        else
	            baseHalfW = (segmentAvgRadiusPx[s] * effectiveZoomForRender) / (float)glWindowWidth;
	        float ghostHalfW = isComparisonActive ? baseHalfW : baseHalfW * 2.0f;
	        float halfW = isHovered ? ghostHalfW * 1.5f : ghostHalfW;

	        glBegin(GL_TRIANGLE_STRIP);
	        for (int i = 0; i < count; i++)
	        {
	            float x = segmentPointsWorld[(start + i) * 2];
	            float y = segmentPointsWorld[(start + i) * 2 + 1];
	            float dx = 0.0f, dy = 0.0f;

	            if (i == 0) {
	                dx = segmentPointsWorld[(start + i + 1) * 2] - x;
	                dy = segmentPointsWorld[(start + i + 1) * 2 + 1] - y;
	            } else if (i == count - 1) {
	                dx = x - segmentPointsWorld[(start + i - 1) * 2];
	                dy = y - segmentPointsWorld[(start + i - 1) * 2 + 1];
	            } else {
	                dx = segmentPointsWorld[(start + i + 1) * 2] - segmentPointsWorld[(start + i - 1) * 2];
	                dy = segmentPointsWorld[(start + i + 1) * 2 + 1] - segmentPointsWorld[(start + i - 1) * 2 + 1];
	            }

	            float len = sqrtf(dx * dx + dy * dy);
	            if (len == 0.0f) len = 1.0f;
	            float nx = -dy / len;
	            float ny = dx / len;

	            glVertex2f(x + nx * halfW, y + ny * halfW);
	            glVertex2f(x - nx * halfW, y - ny * halfW);
	        }
	        glEnd();
	    }

	    glDisable(GL_BLEND);
	}

	// NEW: ghost circles - the FULL circle each arc segment was cut from
	if (canvas.showSegments && canvas.segmentResultCount > 0)
	{
	    glEnable(GL_BLEND);
	    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	    const int circleSteps = 64;

	    for (int s = 0; s < canvas.segmentResultCount; s++)
	    {
	        float rx = segmentCircleRadiusWorld[s];
	        float ry = segmentCircleRadiusWorldY[s];
	        if (rx <= 0.0f || ry <= 0.0f) continue; // straight/degenerate segment - no circle to show

	        float cx = segmentCircleCenterWorld[s * 2];
	        float cy = segmentCircleCenterWorld[s * 2 + 1];

	        BOOL isHovered = (s == hoveredSegment) && !isRobotLayerActive;   // NEW

	        float gr, gg, gb;
	        segmentGhostColor(s, &gr, &gg, &gb);

	        if (isHovered) {
	            glDisable(GL_LINE_STIPPLE);          // solid outline when hovered
	            glLineWidth(2.5f);
	            glColor4f(gr, gg, gb, 1.0f * opacity);
	        } else {
	            glEnable(GL_LINE_STIPPLE);
	            glLineStipple(1, 0x00FF);            // dotted outline otherwise
	            glLineWidth(1.0f);
	            glColor4f(gr, gg, gb, (isRobotLayerActive ? 0.18f : 0.6f) * opacity);
	        }

	        glBegin(GL_LINE_LOOP);
	        for (int i = 0; i < circleSteps; i++) {
	            float theta = (2.0f * 3.14159265f * i) / circleSteps;
	            glVertex2f(cx + rx * cosf(theta), cy + ry * sinf(theta));
	        }
	        glEnd();
	    }

	    glLineWidth(1.0f);
	    glDisable(GL_LINE_STIPPLE);
	    glDisable(GL_BLEND);
	}

    // Branch/junction points (a Y/T/X-shaped stroke splits into multiple
    // edges here) - green.
    if (branchMarkerCount > 0)
    {
        float branchMarkerRadius = 0.02f * canvas.zoom;
        for (int m = 0; m < branchMarkerCount; m++)
        {
            drawMarkerDisc(branchMarkersWorld[m * 2], branchMarkersWorld[m * 2 + 1],
                           branchMarkerRadius, 0.0f, 1.0f, 0.0f, opacity);
        }
    }

    // --- BLINK-FREE UI TEXT DRAWING ---
    // ArcSpline, Semni, and Simulation each have their own independent zoom
    // (canvas.zoom / graphicsGetZoom() / simCameraGetZoom()), but only ONE
    // readout should be on screen at a time -- whichever mode the user is
    // actually in -- rather than stacking all three, since the other two
    // are irrelevant to what you're doing.
    BOOL semniModeActive = (editorModeState.currentMode == EDITOR_MODE_SEMNI);

    char zoomStr[32];
    if (appMode == APP_MODE_SIMULATION)
    {
        // Same "bigger = zoomed in" convention as graphicsGetZoom below --
        // sim_camera's zoom was deliberately built to match it (see
        // sim_camera.h), so this readout uses the same "100 * zoom" formula
        // rather than canvas.zoom's inverted one.
        int simZoomPercent = (int)(100.0f * simCameraGetZoom());
        wsprintfA(zoomStr, "Zoom (Simulation): %d%%", simZoomPercent);
    }
    else if (semniModeActive)
    {
        // NOTE: opposite convention from canvas.zoom below -- graphics.c's
        // g_zoom is a direct multiplier on top of the base projection (see
        // graphicsZoom/applyProjection), so a BIGGER g_zoom means zoomed
        // IN (smaller frustum), unlike canvas.zoom where bigger means
        // zoomed OUT. Using the same "100 / zoom" formula here made this
        // readout count down while actually zooming in.
        int robotZoomPercent = (int)(100.0f * graphicsGetZoom());
        wsprintfA(zoomStr, "Zoom (Robot): %d%%", robotZoomPercent);
    }
    else
    {
        int zoomPercent = (int)(100.0f / canvas.zoom);
        wsprintfA(zoomStr, "Zoom (Sketch): %d%%", zoomPercent);
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix(); glLoadIdentity();
    glOrtho(0, glWindowWidth, 0, glWindowHeight, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); glLoadIdentity();

    // NEW: corner hover indicator - shows exactly where to hover to
    // reveal the UI panel, and brightens while you're hovering it.
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float tabLeft   = (float)(glWindowWidth - UI_HOTZONE_WIDTH);
        float tabRight  = (float)glWindowWidth;
        float tabBottom = (float)(glWindowHeight - UI_HOTZONE_HEIGHT);
        float tabTop    = (float)glWindowHeight;

        float fillA = (hotZoneHighlighted ? 0.30f : 0.10f) * opacity;
        if (hotZoneHighlighted)
            glColor4f(0.25f, 0.55f, 0.95f, fillA);
        else
            glColor4f(0.4f, 0.4f, 0.4f, fillA);

        glBegin(GL_QUADS);
            glVertex2f(tabLeft,  tabBottom);
            glVertex2f(tabRight, tabBottom);
            glVertex2f(tabRight, tabTop);
            glVertex2f(tabLeft,  tabTop);
        glEnd();

        // Small drawer-handle icon: three short horizontal bars
        float shade = hotZoneHighlighted ? 0.95f : 0.55f;
        glColor4f(shade, shade, shade, 0.9f * opacity);
        float cx = (tabLeft + tabRight) * 0.5f;
        float cy = (tabBottom + tabTop) * 0.5f;
        float barHalfW = (float)UI_HOTZONE_WIDTH * 0.22f;
        for (int i = -1; i <= 1; i++)
        {
            float by = cy + i * 7.0f;
            glBegin(GL_QUADS);
                glVertex2f(cx - barHalfW, by - 1.5f);
                glVertex2f(cx + barHalfW, by - 1.5f);
                glVertex2f(cx + barHalfW, by + 1.5f);
                glVertex2f(cx - barHalfW, by + 1.5f);
            glEnd();
        }

        glDisable(GL_BLEND);
    }

    // Text/swatch drawing below relies on its glColor4f alpha actually
    // being respected (needed for opacity < 1 when this canvas is the
    // dimmed/inactive subsystem), which requires GL_BLEND enabled --
    // unlike the corner indicator above, this block doesn't toggle it
    // itself, so enable it here and leave it on through the mode indicator.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // This zoom readout is global HUD, not ArcSpline-specific content, so
    // -- same reasoning as the mode indicator below -- it always draws at
    // full opacity instead of fading with `opacity` when this canvas
    // happens to be the dimmed/inactive subsystem.
    glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
    glRasterPos2i(glWindowWidth - 160, 20);
    glPushAttrib(GL_LIST_BIT);
    glListBase(fontBase - 32);
    glCallLists((GLsizei)strlen(zoomStr), GL_UNSIGNED_BYTE, zoomStr);
    glPopAttrib();

    // Always-on exact client-area pixel position of the cursor, same
    // bottom-right column as the zoom readout just above, stacked directly
    // above it (y = 40 vs the zoom line's y = 20, one text row's worth of
    // clearance) so the two never overlap. Read fresh via GetCursorPos/
    // ScreenToClient every frame instead of threading it through
    // WM_MOUSEMOVE, since WM_MOUSEMOVE only ever reaches whichever
    // subsystem (WndProcGL here, or handleInput) is currently active (see
    // WndProcShared's mode-gated dispatch) -- reading the cursor position
    // directly here means one implementation works identically in both
    // Design > Robot and Design > Environment, with no mouse-move hook
    // needed in either one. Used to be gated behind a 'P' toggle -- removed
    // in favor of just always showing it. Same "always full opacity"
    // reasoning as the zoom readout above -- global HUD, not
    // ArcSpline-specific content.
    {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWndGL, &pt);

        char pixelStr[32];
        wsprintfA(pixelStr, "Pixel: (%d, %d)", (int)pt.x, (int)pt.y);

        glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
        glRasterPos2i(glWindowWidth - 160, 40);
        glPushAttrib(GL_LIST_BIT);
        glListBase(fontBase - 32);
        glCallLists((GLsizei)strlen(pixelStr), GL_UNSIGNED_BYTE, pixelStr);
        glPopAttrib();
    }

    // NEW: View Segments hover readout -- tells you WHICH segment (by
    // position) the cursor is currently over, not just that it's
    // highlighted. Follows the cursor itself (drawn just up and to the
    // right of it, using the same client-pixel `pt` the Pixel readout just
    // above already fetched) rather than sitting in a fixed HUD corner, so
    // it always reads right next to whatever you're actually pointing at.
    // Colored to match the hovered circle/segment's own highlight color
    // (circleSegmentColor/bodyCircleColor/segmentGhostColor) instead of the
    // HUD's usual gray, so the label and the thing it's naming visually tie
    // together at a glance. Same "Segment N/M" format in both modes
    // (Design > Robot and Design > Environment). Only one of the two
    // branches can ever be true at once (Design > Robot forces
    // EDITOR_MODE_SEMNI, Design > Environment/Simulation don't), so there's
    // no need to pick between them beyond the existing semniModeActive
    // check both other HUD blocks in this function already use.
    //
    // Robot mode: input.c's WM_MOUSEMOVE hit-test (gated on
    // app.showCircleSegments the same way this is) already resolves the
    // hover down to at most ONE of a fillet (app.hoveredCircleSegment) or a
    // body circle (app.hoveredBodyCircle) -- see its own comment for why
    // they share one winner -- so at most one of these two branches fires.
    // The two are numbered as one combined N/M range (fillets 1..6, body
    // circles 7..11) rather than each restarting at 1/6 or 1/5, so this
    // reads as one consistent "N/M" position the same way Environment
    // mode's flat segment list already does, instead of silently meaning
    // two different scales depending on what's hovered.
    //
    // Environment mode: canvas.c's own hoveredSegment (WM_MOUSEMOVE above)
    // is an index into canvas.segmentResults, forced back to -1 outright
    // during Simulation (see that assignment's comment), so this can never
    // show a stale/wrong-mode readout there even though semniModeActive
    // alone wouldn't otherwise rule Simulation out.
    {
        char hoverSegStr[64];
        BOOL showHoverSeg = FALSE;
        float segR = 0.3f, segG = 0.3f, segB = 0.3f;

        if (semniModeActive)
        {
            // Rocky/Stilo have their own View Segments overlays now too
            // (renderer.c/h's computeRocky*/computeStilo*), each with a
            // different fillet/body-circle count than Semni's own 6/5 --
            // pick the matching pair for whichever kind is actually active
            // so the "N/M" total (and the fillet-vs-body-circle numbering
            // split just below) means the right thing for all three.
            //
            // The DISPLAY number is no longer just "fillets 1..N, then body
            // circles N+1..N+M" (the raw index order the underlying arrays
            // use for color/hit-test, which stays untouched) -- it now
            // follows the same Head/Butt/Seam/Hip/Knee/Feet/Thigh/Shin
            // priority order established for hover labels and click
            // priority (input.c), so a user reading "Segment 3/11" sees the
            // same position in that established order regardless of which
            // internal array (fillet vs body circle) actually backs it.
            // filletDisplayNum[i]/bodyDisplayNum[i] map a raw array index to
            // that display number; see each kind's compute* function
            // (renderer.c) for the raw index order these were derived from.
            int numCircleSegments, numBodyCircles, numRectSegments;
            const int* filletDisplayNum;
            const int* bodyDisplayNum;
            const int* rectDisplayNum;
            switch (app.robotScene.activeKind)
            {
                case ROBOT_KIND_ROCKY:
                {
                    // raw rect-edge order: top, right, bottom, left (see
                    // renderer.c's computeRockyRectSegments) -- raw fillet
                    // order: shin1, shin2 -- raw body order: knee, foot.
                    // The 4 rectangle edges still come first (1-4), then
                    // knee(5), foot(6), shin1(7), shin2(8) -- but the
                    // mapping among the 4 edges was cyclically shifted by
                    // one position per explicit user request (whatever
                    // showed "4" should show "1", "1"->"2", "2"->"3",
                    // "3"->"4"): raw index 0 (top, was 1) is now 2, raw 1
                    // (right, was 2) is now 3, raw 2 (bottom, was 3) is now
                    // 4, raw 3 (left, was 4) is now 1.
                    static const int rockyRectDisplayNum[NUM_ROCKY_RECT_SEGMENTS] = { 2, 3, 4, 1 };
                    static const int rockyFilletDisplayNum[NUM_ROCKY_CIRCLE_SEGMENTS] = { 7, 8 };
                    static const int rockyBodyDisplayNum[NUM_ROCKY_BODY_CIRCLES] = { 5, 6 };
                    numCircleSegments = NUM_ROCKY_CIRCLE_SEGMENTS;
                    numBodyCircles = NUM_ROCKY_BODY_CIRCLES;
                    numRectSegments = NUM_ROCKY_RECT_SEGMENTS;
                    filletDisplayNum = rockyFilletDisplayNum;
                    bodyDisplayNum = rockyBodyDisplayNum;
                    rectDisplayNum = rockyRectDisplayNum;
                    break;
                }
                case ROBOT_KIND_STILO:
                {
                    // raw fillet order: seam1, seam2, thigh1Arc1,
                    // thigh1Arc2, thigh2Arc1, thigh2Arc2 -- raw body order:
                    // out[0] is the physical headX-based circle, out[1] the
                    // physical buttX-based circle. Same display-name swap as
                    // input.c's hoverStiloHead/hoverStiloButt (see that
                    // comment): the physical headX circle (out[0]) is what
                    // the UI calls "Butt", and the physical buttX circle
                    // (out[1]) is what it calls "Head" -- so out[0] gets the
                    // "Butt" display number and out[1] gets "Head"'s, not
                    // the other way around. Desired: head=out[1](1),
                    // butt=out[0](2), seam1(3), seam2(4), hip1(5), feet1(6),
                    // thigh1Arc1(7), thigh1Arc2(8), hip2(9), feet2(10),
                    // thigh2Arc1(11), thigh2Arc2(12).
                    static const int stiloFilletDisplayNum[NUM_STILO_CIRCLE_SEGMENTS] = { 3, 4, 7, 8, 11, 12 };
                    static const int stiloBodyDisplayNum[NUM_STILO_BODY_CIRCLES] = { 2, 1, 5, 6, 9, 10 };
                    numCircleSegments = NUM_STILO_CIRCLE_SEGMENTS;
                    numBodyCircles = NUM_STILO_BODY_CIRCLES;
                    // Stilo has no rectangular body (see app.h's own
                    // comment on hoveredRectSegment) -- 0/NULL, never
                    // dereferenced since app.hoveredRectSegment is reset to
                    // -1 whenever Rocky isn't the active kind (input.c).
                    numRectSegments = 0;
                    filletDisplayNum = stiloFilletDisplayNum;
                    bodyDisplayNum = stiloBodyDisplayNum;
                    rectDisplayNum = NULL;
                    break;
                }
                case ROBOT_KIND_SEMNI:
                default:
                {
                    // raw fillet order: seam1, seam2, thigh1, thigh2, shin1,
                    // shin2 -- raw body order: out[0] is the physical
                    // headX-based circle, out[1] the physical buttX-based
                    // circle. Same display-name swap as input.c's
                    // hoverHead/hoverButt (see that comment): the physical
                    // headX circle (out[0]) is what the UI calls "Butt", and
                    // the physical buttX circle (out[1]) is what it calls
                    // "Head" -- so out[0] gets the "Butt" display number and
                    // out[1] gets "Head"'s, not the other way around.
                    // Desired: head=out[1](1), butt=out[0](2), seam1(3),
                    // seam2(4), hip(5), knee(6), foot(7), thigh1(8),
                    // thigh2(9), shin1(10), shin2(11).
                    static const int semniFilletDisplayNum[NUM_ROBOT_CIRCLE_SEGMENTS] = { 3, 4, 8, 9, 10, 11 };
                    static const int semniBodyDisplayNum[NUM_ROBOT_BODY_CIRCLES] = { 2, 1, 5, 6, 7 };
                    numCircleSegments = NUM_ROBOT_CIRCLE_SEGMENTS;
                    numBodyCircles = NUM_ROBOT_BODY_CIRCLES;
                    // Semni has no rectangular body either -- same 0/NULL,
                    // never-dereferenced reasoning as Stilo's own case above.
                    numRectSegments = 0;
                    filletDisplayNum = semniFilletDisplayNum;
                    bodyDisplayNum = semniBodyDisplayNum;
                    rectDisplayNum = NULL;
                    break;
                }
            }
            int totalRobotSegments = numCircleSegments + numBodyCircles + numRectSegments;

            if (app.showCircleSegments && app.hoveredCircleSegment != -1)
            {
                wsprintfA(hoverSegStr, "Segment %d/%d", filletDisplayNum[app.hoveredCircleSegment], totalRobotSegments);
                circleSegmentColor(app.hoveredCircleSegment, &segR, &segG, &segB);
                showHoverSeg = TRUE;
            }
            else if (app.showCircleSegments && app.hoveredBodyCircle != -1)
            {
                wsprintfA(hoverSegStr, "Segment %d/%d", bodyDisplayNum[app.hoveredBodyCircle], totalRobotSegments);
                bodyCircleColor(app.hoveredBodyCircle, &segR, &segG, &segB);
                showHoverSeg = TRUE;
            }
            else if (app.showCircleSegments && app.hoveredRectSegment != -1)
            {
                // Rocky only (see app.h's own hoveredRectSegment comment) --
                // rectDisplayNum is only ever non-NULL for
                // ROBOT_KIND_ROCKY, and app.hoveredRectSegment is only ever
                // set to something other than -1 while Rocky is active
                // (input.c), so this branch can't fire with a NULL
                // rectDisplayNum.
                wsprintfA(hoverSegStr, "Segment %d/%d", rectDisplayNum[app.hoveredRectSegment], totalRobotSegments);
                rectSegmentColor(app.hoveredRectSegment, &segR, &segG, &segB);
                showHoverSeg = TRUE;
            }
        }
        else if (canvas.showSegments && hoveredSegment != -1)
        {
            wsprintfA(hoverSegStr, "Segment %d/%d", hoveredSegment + 1, canvas.segmentResultCount);
            segmentGhostColor(hoveredSegment, &segR, &segG, &segB);
            showHoverSeg = TRUE;
        }

        if (showHoverSeg)
        {
            // Same GetCursorPos/ScreenToClient technique the Pixel readout
            // above uses (its own `pt` is scoped to that block, so this
            // fetches its own rather than reaching outside it) -- client
            // pixels, origin top-left, Y growing DOWNWARD. This block's
            // ortho (glOrtho(0, glWindowWidth, 0, glWindowHeight, ...), set
            // up before the Zoom readout earlier in this function) has
            // origin bottom-left, Y growing UPWARD instead, so Y has to
            // flip (glWindowHeight - pt.y) before adding the "up and to the
            // right" offset.
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWndGL, &pt);

            int tooltipX = (int)pt.x + 16;
            int tooltipY = (glWindowHeight - (int)pt.y) + 16;

            glColor4f(segR, segG, segB, 1.0f);
            glRasterPos2i(tooltipX, tooltipY);
            glPushAttrib(GL_LIST_BIT);
            glListBase(fontBase - 32);
            glCallLists((GLsizei)strlen(hoverSegStr), GL_UNSIGNED_BYTE, hoverSegStr);
            glPopAttrib();
        }
    }

    // NEW: persistent top-left mode/layer indicator - otherwise the
    // only way to tell Design/Robot/Environment apart is to open the
    // Mode menu and see which item is checked.
    {
        char modeStr[64];
        float mr, mg, mb;

        if (appMode == APP_MODE_SIMULATION) {
            wsprintfA(modeStr, "Mode: Simulation");
            mr = 0.5f; mg = 0.3f; mb = 0.7f;   // purple
        } else if (designLayer == LAYER_ROBOT) {
            wsprintfA(modeStr, "Mode: Design - Robot");
            mr = 0.85f; mg = 0.45f; mb = 0.0f; // orange, matches the endpoint-snap highlight
        } else {
            wsprintfA(modeStr, "Mode: Design - Environment");
            mr = 0.1f; mg = 0.55f; mb = 0.15f; // green
        }

        // Small color swatch ahead of the text so the mode reads at a
        // glance without needing to read the label itself.
        //
        // This indicator reflects global app/mode state, not ArcSpline-
        // specific content, so it always draws at full opacity -- even
        // when this canvas is the dimmed/inactive subsystem (e.g. while
        // the Semni robot editor is on top). Using `opacity` here made
        // "Mode: Design - Robot" fade out precisely while you were IN
        // robot design mode, which read as a bug/dead control.
        glColor4f(mr, mg, mb, 1.0f);
        glBegin(GL_QUADS);
            glVertex2f(10.0f, (float)glWindowHeight - 24.0f);
            glVertex2f(20.0f, (float)glWindowHeight - 24.0f);
            glVertex2f(20.0f, (float)glWindowHeight - 14.0f);
            glVertex2f(10.0f, (float)glWindowHeight - 14.0f);
        glEnd();

        glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
        glRasterPos2i(26, glWindowHeight - 22);
        glPushAttrib(GL_LIST_BIT);
        glListBase(fontBase - 32);
        glCallLists((GLsizei)strlen(modeStr), GL_UNSIGNED_BYTE, modeStr);
        glPopAttrib();
    }

    // Shift+G's "AUTO GRAVITY ON"/"AUTO GRAVITY OFF" toast -- bottom-left,
    // mirroring the top-left mode indicator above. Stateless fade: alpha is
    // recomputed fresh every frame from how long ago gravityToastStartTick
    // was stamped (WM_KEYDOWN's Shift+G branch), not stepped by a timer, so
    // it doesn't matter which timer happens to be driving repaints right
    // now (see WM_TIMER's UI_HOTZONE_TIMER_ID case, which nudges repaints
    // along during the fade once auto-gravity's own ticking has stopped).
    // gravityToastStartTick == 0 means no toast is pending/visible.
    if (gravityToastStartTick != 0)
    {
        DWORD elapsed = GetTickCount() - gravityToastStartTick;
        float toastAlpha = 0.0f;

        if (elapsed < SIMULATION_GRAVITY_TOAST_HOLD_MS)
        {
            toastAlpha = 1.0f;
        }
        else if (elapsed < (DWORD)(SIMULATION_GRAVITY_TOAST_HOLD_MS + SIMULATION_GRAVITY_TOAST_FADE_MS))
        {
            float fadeElapsed = (float)(elapsed - SIMULATION_GRAVITY_TOAST_HOLD_MS);
            toastAlpha = 1.0f - (fadeElapsed / (float)SIMULATION_GRAVITY_TOAST_FADE_MS);
        }

        if (toastAlpha > 0.0f)
        {
            const char* toastStr = gravityToastOn ? "AUTO GRAVITY ON" : "AUTO GRAVITY OFF";
            float tr = gravityToastOn ? 0.15f : 0.4f;
            float tg = gravityToastOn ? 0.55f : 0.4f;
            float tb = gravityToastOn ? 0.15f : 0.4f;

            glColor4f(tr, tg, tb, toastAlpha);
            glRasterPos2i(10, 20);
            glPushAttrib(GL_LIST_BIT);
            glListBase(fontBase - 32);
            glCallLists((GLsizei)strlen(toastStr), GL_UNSIGNED_BYTE, toastStr);
            glPopAttrib();
        }
    }

    // Shift+W's "WALK ON"/"WALK OFF" toast -- same stateless-fade pattern
    // as the gravity toast just above, one line higher (y=36 vs y=20) so
    // both can show at once without overlapping if auto-gravity and Walk
    // are toggled close together.
    if (gaitToastStartTick != 0)
    {
        DWORD elapsed = GetTickCount() - gaitToastStartTick;
        float toastAlpha = 0.0f;

        if (elapsed < SIMULATION_GRAVITY_TOAST_HOLD_MS)
        {
            toastAlpha = 1.0f;
        }
        else if (elapsed < (DWORD)(SIMULATION_GRAVITY_TOAST_HOLD_MS + SIMULATION_GRAVITY_TOAST_FADE_MS))
        {
            float fadeElapsed = (float)(elapsed - SIMULATION_GRAVITY_TOAST_HOLD_MS);
            toastAlpha = 1.0f - (fadeElapsed / (float)SIMULATION_GRAVITY_TOAST_FADE_MS);
        }

        if (toastAlpha > 0.0f)
        {
            const char* toastStr = gaitToastOn ? "WALK ON" : "WALK OFF";
            float tr = gaitToastOn ? 0.15f : 0.4f;
            float tg = gaitToastOn ? 0.55f : 0.4f;
            float tb = gaitToastOn ? 0.15f : 0.4f;

            glColor4f(tr, tg, tb, toastAlpha);
            glRasterPos2i(10, 36);
            glPushAttrib(GL_LIST_BIT);
            glListBase(fontBase - 32);
            glCallLists((GLsizei)strlen(toastStr), GL_UNSIGNED_BYTE, toastStr);
            glPopAttrib();
        }
    }

    // VK_LEFT/RIGHT's rotation HUD -- one line above the Walk toast (y=52
    // vs y=36), same 16px spacing. Not the same shape as the two
    // stateless-fade toasts just above: while rotateKeyHeld is TRUE the
    // key is actively down right now, so this shows continuously at full
    // opacity, live, every frame -- no hold/fade timing at all, otherwise
    // the readout would start fading out from under the user's own
    // fingers mid-hold. Only once the key comes up (WM_KEYUP clears
    // rotateToastStartTick) does the SAME
    // hold-then-fade pattern take over, so the last angle shown lingers
    // briefly instead of vanishing the instant the key is released. Shows
    // the real degree sign (0xB0) now that fontBase's own glyph range
    // covers it (see its own comment, WM_CREATE above) -- no longer
    // spelled out as "DEG".
    if (rotateKeyHeld || rotateToastStartTick != 0)
    {
        float toastAlpha = 1.0f;

        if (!rotateKeyHeld)
        {
            DWORD elapsed = GetTickCount() - rotateToastStartTick;
            toastAlpha = 0.0f;

            if (elapsed < SIMULATION_GRAVITY_TOAST_HOLD_MS)
            {
                toastAlpha = 1.0f;
            }
            else if (elapsed < (DWORD)(SIMULATION_GRAVITY_TOAST_HOLD_MS + SIMULATION_GRAVITY_TOAST_FADE_MS))
            {
                float fadeElapsed = (float)(elapsed - SIMULATION_GRAVITY_TOAST_HOLD_MS);
                toastAlpha = 1.0f - (fadeElapsed / (float)SIMULATION_GRAVITY_TOAST_FADE_MS);
            }
        }

        if (toastAlpha > 0.0f)
        {
            // wsprintfA (the convention every other on-screen label in this
            // file uses -- zoomStr/hoverSegStr/modeStr above) doesn't
            // support floating-point format specifiers at all, so round to
            // the nearest whole degree by hand first rather than handing
            // it a raw float.
            //
            // Negated here, display-only: rotatePoint (geometry.c) turns a
            // positive angle into a COUNTERclockwise turn on screen (the
            // standard math convention for this coordinate space), which
            // made VK_RIGHT (a clockwise turn -- it applies a NEGATIVE
            // step, see WM_KEYDOWN above) read as the number counting
            // DOWN, and VK_LEFT counting up -- backwards from how a
            // clockwise-increasing readout (like a compass heading) is
            // expected to behave. Flipping the sign right here, before
            // rounding, makes the DISPLAYED count increase 0->359 for a
            // clockwise turn (VK_RIGHT) and decrease for a counterclockwise
            // one (VK_LEFT), without touching the real angle field itself
            // or which key does which -- only which direction this readout
            // counts up in.
            float angle = -activeRobotAngleDegrees();
            int angleDeg = (int)(angle >= 0.0f ? (angle + 0.5f) : (angle - 0.5f));

            // activeRobotAngleDegrees() is the raw, unwrapped rotation --
            // it keeps climbing (or dropping negative) with every further
            // VK_LEFT/RIGHT press rather than resetting at a full turn, so
            // by itself it would show 360, 361, 720... instead of wrapping
            // back to 0. Wrap the ROUNDED int (not the float before
            // rounding) into [0, 359] here, display-only -- this never
            // touches the real angle field driving the robot's actual
            // pose, only what this readout prints. The C '%' operator can
            // return a negative result for a negative left operand (e.g.
            // -1 % 360 == -1, not 359), so the sign is corrected by hand
            // right after.
            angleDeg %= 360;
            if (angleDeg < 0) angleDeg += 360;

            char toastStr[32];
            wsprintfA(toastStr, "ROTATION: %d\xB0", angleDeg);

            glColor4f(0.2f, 0.4f, 0.85f, toastAlpha);
            glRasterPos2i(10, 52);
            glPushAttrib(GL_LIST_BIT);
            glListBase(fontBase - 32);
            glCallLists((GLsizei)strlen(toastStr), GL_UNSIGNED_BYTE, toastStr);
            glPopAttrib();
        }
    }

    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

// Draws ONE combined frame: clears the color buffer once, then draws both
// editor subsystems, whichever is currently active (editorModeState.
// currentMode) at full opacity on top, the other dimmed underneath. See
// canvas.h for the full rationale.
//
// EXCEPT in Simulation mode: switchEditorMode always forces ArcSpline to
// be the "active" editor mode when Simulation is picked (see WM_COMMAND's
// ID_MODE_SIMULATION handling), which would otherwise leave the robot
// dimmed the whole time you're simulating -- backwards for a mode whose
// whole point is seeing the robot posed clearly within its environment,
// and dragging it into a starting position (WM_LBUTTONDOWN/WM_MOUSEMOVE
// above). So both subsystems draw at full opacity together here instead.
void renderCombinedFrame(void)
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    BOOL semniActive = (editorModeState.currentMode == EDITOR_MODE_SEMNI);
    BOOL simulationActive = (appMode == APP_MODE_SIMULATION);

    // hideInactiveLayer (View > Hide Inactive Layer, see its own comment)
    // swaps the inactive layer's dim amount from the normal partial fade
    // (INACTIVE_MODE_DIM_ALPHA) up to 1.0 -- renderRobotScene/
    // canvasRenderFrame both compute opacity = 1.0 - dimAmount, so 1.0
    // means fully invisible rather than just faded.
    float inactiveDimAmount = hideInactiveLayer ? 1.0f : INACTIVE_MODE_DIM_ALPHA;
    float robotDimAmount  = (semniActive || simulationActive) ? 0.0f : inactiveDimAmount;
    float canvasDimAmount = (!semniActive || simulationActive) ? 0.0f : inactiveDimAmount;

    // Simulation mode's mass-center drop line needs to know where a
    // straight drop from the mass center actually reaches the
    // environment -- computed once here (canvas.c owns the environment/
    // collision code this needs) and threaded into renderRobotScene,
    // which draws it (see drawSimulationMassCenterDropLine's own
    // comment). Only worth computing while actually in Simulation --
    // it's not drawn otherwise, and the search below does real per-frame
    // environment queries.
    float massCenterDropStopY = 0.0f;
    if (simulationActive)
    {
        PointF massCenterTip = computeSimulationMassCenterWorld(&app);
        massCenterDropStopY = simFindGroundBelowRobotPoint(massCenterTip);
    }

    // Semni's own projection/blend state has to be (re)asserted right
    // before it draws, and the ArcSpline canvas's projection right before
    // IT draws (canvasRenderFrame does this itself via UpdateProjection) --
    // same reasoning as the old per-mode render loop in main.c: the two
    // subsystems share one GL context, so whichever drew last left its own
    // projection matrix active.
    if (semniActive)
    {
        // ArcSpline underneath...
        canvasRenderFrame(canvasDimAmount);

        // ...Semni on top
        graphicsOnResize(glWindowWidth, glWindowHeight);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        renderRobotScene(&app, robotDimAmount, massCenterDropStopY);
    }
    else
    {
        // Semni underneath...
        graphicsOnResize(glWindowWidth, glWindowHeight);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        renderRobotScene(&app, robotDimAmount, massCenterDropStopY);

        // ...ArcSpline on top
        canvasRenderFrame(canvasDimAmount);

        // Contact debug dots (plain C toggle) -- drawn LAST, after BOTH
        // the robot and the environment, so nothing painted on top of
        // Simulation mode's own environment ribbon/strokes can cover them
        // (this used to run right after renderRobotScene, BEFORE
        // canvasRenderFrame -- if the environment's own drawing happened
        // to paint over a marker sitting right at ground contact, exactly
        // where most of these markers actually are, it would never be
        // visible despite genuinely being drawn). canvasRenderFrame just
        // rebound its OWN projection for its own draw, so the robot's has
        // to be reasserted here -- these points are in robot-local
        // coordinates, same convention drawRockyMassCenterTrack/
        // drawRockyReferencePoint already use, not whatever ArcSpline just
        // left bound. No-ops instantly unless both showContactDebug is on
        // and we're actually in Simulation mode.
        graphicsOnResize(glWindowWidth, glWindowHeight);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        {
            // graphicsOnResize above only re-asserts the PROJECTION matrix
            // -- canvasRenderFrame's own MODELVIEW translate (the
            // ENVIRONMENT's pan, simPanX/simPanY) is still bound at this
            // point and has to be replaced with the ROBOT's own pan here,
            // the exact same way renderRobotScene/renderApp do it each
            // frame, or these robot-local points drift away from the
            // robot the moment either subsystem's pan differs from the
            // other's (which is most of the time -- they're unrelated
            // pan values that only coincide at (0,0)).
            float panX, panY;
            graphicsGetPan(&panX, &panY);
            glTranslatef(-panX, -panY, 0.0f);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawSimulationContactDebug();
        drawSimulationLiveContactDots();
    }

    SwapBuffers(hDC);
}

// Appends (or, while Shift is held, re-derives) the in-progress stroke's
// next point from a CLIENT-space pixel position -- the exact logic
// WM_MOUSEMOVE's drawing branch used to have inline. Factored out so
// WM_KEYDOWN's VK_SHIFT handling below can run the same "constrain to a
// straight line" computation using GetCursorPos, not just WM_MOUSEMOVE
// using lParam -- see that handler's comment for why that matters. mx/my
// are CLIENT window pixel coordinates in both cases (WM_MOUSEMOVE's
// LOWORD/HIWORD(lParam) already are; the caller is responsible for
// ScreenToClient-ing a GetCursorPos result first).
static void updateDrawingPoint(HWND hWnd, int mx, int my, BOOL shiftHeld, BOOL ctrlHeld)
{
    float x = (float)mx;
    float y = (float)my;
    float aspect = (float)glWindowWidth / (float)glWindowHeight;
    float nx, ny;
    if (aspect >= 1.0f) {
        nx = ((2.0f * x / glWindowWidth) - 1.0f) * aspect * canvas.zoom;
        ny = (1.0f - (2.0f * y / glWindowHeight)) * canvas.zoom;
    } else {
        nx = ((2.0f * x / glWindowWidth) - 1.0f) * canvas.zoom;
        ny = (1.0f - (2.0f * y / glWindowHeight)) * (1.0f / aspect) * canvas.zoom;
    }
    nx += canvas.panX;
    ny += canvas.panY;

    // Holding Shift constrains the stroke to a straight line from its
    // start point to the current cursor. Re-derived every call (truncate +
    // re-append) rather than appended to, so it tracks the cursor like a
    // rubber-band preview instead of accumulating a freehand trail
    // underneath it. Releasing Shift mid-drag restores the freehand path
    // exactly as it was before Shift was first pressed (see the backup
    // snapshot/restore below) -- Shift is a temporary preview, not a commit.
    //
    // Adding Ctrl on top of Shift instantly snaps that line's angle to the
    // nearest 45 degrees (horizontal / vertical / diagonal), same idea as
    // the axis-lock in most drawing tools.
    //
    // Holding the cursor still (no Ctrl needed) for SHIFT_HOLD_SNAP_MS near
    // one of those same angles snaps it too -- see the dwell check in
    // WM_TIMER, since a stationary cursor generates no WM_MOUSEMOVE
    // messages for that check to run inside. This function only tracks raw
    // cursor state and applies the lock once WM_TIMER has set
    // shiftHoldSnapped; it never decides to snap itself.
    if (shiftHeld)
    {
        int curStrokeStart = strokeStarts[canvas.strokeCount - 1];

        if (!shiftHoldActive)
        {
            // Fresh activation for this drag (first Shift-held call since
            // the last release) -- snapshot the freehand points drawn so
            // far, BEFORE they get truncated away below, so releasing
            // Shift can restore them exactly instead of leaving whatever
            // the straight-line preview ended up at.
            shiftHoldFreehandBackupCount = canvas.pointCount;
            memcpy(shiftHoldFreehandBackup, points, sizeof(float) * (size_t)canvas.pointCount);
        }

        canvas.pointCount = curStrokeStart + 2; // keep only the stroke's first point

        int dxPix = mx - shiftHoldPixelX;
        int dyPix = my - shiftHoldPixelY;
        BOOL movedSignificantly = !shiftHoldActive ||
            (dxPix * dxPix + dyPix * dyPix) > (SHIFT_HOLD_JITTER_PX * SHIFT_HOLD_JITTER_PX);

        if (movedSignificantly)
        {
            shiftHoldActive       = TRUE;
            shiftHoldStrokeStart  = curStrokeStart;
            shiftHoldPixelX       = mx;
            shiftHoldPixelY       = my;
            shiftHoldLastMoveTick = GetTickCount();
            shiftHoldSnapped      = FALSE;   // moving again releases any dwell-lock
        }
        // Always track the latest raw position (even tiny sub-jitter
        // moves), so the WM_TIMER dwell check judges the angle against
        // where the cursor actually is right now.
        shiftHoldWorldX = nx;
        shiftHoldWorldY = ny;

        if (ctrlHeld)
        {
            // points[curStrokeStart]/[+1] is the stroke's untouched first
            // point - only pointCount was rewound above, the underlying
            // data is still there, so this is a safe anchor to snap from.
            float startX = points[curStrokeStart];
            float startY = points[curStrokeStart + 1];

            float dx = nx - startX;
            float dy = ny - startY;
            float len = sqrtf(dx * dx + dy * dy);

            if (len > 1e-6f)
            {
                const float step = 3.14159265f / 4.0f; // 45 degrees
                float angle = atan2f(dy, dx);
                float snapped = roundf(angle / step) * step;

                nx = startX + len * cosf(snapped);
                ny = startY + len * sinf(snapped);
            }
        }
        else if (shiftHoldSnapped)
        {
            // Dwell-locked (set in WM_TIMER): the ANGLE is fixed, but the
            // LENGTH stays live - project the raw cursor onto the locked
            // ray so the user can still drag the endpoint back and forth.
            float startX = points[curStrokeStart];
            float startY = points[curStrokeStart + 1];
            float dirX = cosf(shiftHoldSnapAngleRad);
            float dirY = sinf(shiftHoldSnapAngleRad);
            float relX = nx - startX;
            float relY = ny - startY;
            float proj = relX * dirX + relY * dirY;

            nx = startX + dirX * proj;
            ny = startY + dirY * proj;
        }
    }
    else
    {
        if (shiftHoldActive)
        {
            // Shift just released mid-drag -- restore the freehand points
            // from the snapshot taken when Shift was first pressed this
            // drag, discarding the straight-line preview entirely instead
            // of keeping it or continuing freehand from its endpoint.
            // Returns immediately after, so the restored path is exactly
            // what it was before Shift touched it -- no extra point tacked
            // on at wherever the cursor happens to be right now (ordinary
            // freehand drawing will naturally continue from here on the
            // next real mouse move, same as any other gap between moves).
            canvas.pointCount = shiftHoldFreehandBackupCount;
            memcpy(points, shiftHoldFreehandBackup, sizeof(float) * (size_t)shiftHoldFreehandBackupCount);

            shiftHoldActive  = FALSE;
            shiftHoldSnapped = FALSE;

            InvalidateRect(hWnd, NULL, FALSE);
            return;
        }

        shiftHoldActive  = FALSE;   // Shift released - drop any dwell tracking/lock
        shiftHoldSnapped = FALSE;
    }

    if (canvas.pointCount < MAX_POINTS - 1) {
        points[canvas.pointCount++] = nx;
        points[canvas.pointCount++] = ny;
        InvalidateRect(hWnd, NULL, FALSE);
    }
}
LRESULT CALLBACK WndProcGL(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_CREATE:
    {
        hDC = GetDC(hWnd);
        PIXELFORMATDESCRIPTOR pfd = {0};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 16;

        int pf = ChoosePixelFormat(hDC, &pfd);
        SetPixelFormat(hDC, pf, &pfd);
        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);

        HFONT hFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);

        // Range widened from the original 32..127 (96 glyphs, plain ASCII
        // only) up through 176 (0xB0) -- the rotation HUD (canvasRenderFrame,
        // further down) wants the actual degree sign, which lives at 0xB0
        // in the Windows ANSI/Latin-1 codepage this DEFAULT_CHARSET font
        // resolves to, not anywhere in 32..127. glListBase(fontBase - 32)
        // (every call site already uses this same offset) still lines up
        // correctly for any byte in this new range since it's one
        // contiguous block starting at 32, same as before -- nothing else
        // needed to change. The handful of extra Latin-1 glyphs in between
        // (144 of them now instead of 96) go unused by anything else in
        // this file, which costs a bit more video memory for their display
        // lists but nothing else.
        fontBase = glGenLists(145);
        wglUseFontBitmaps(hDC, 32, 145, fontBase);
        SelectObject(hDC, hOldFont);
        DeleteObject(hFont);

        UpdateProjection();
        glLoadIdentity();

        SetTimer(hWnd, UI_HOTZONE_TIMER_ID, UI_HOTZONE_INTERVAL_MS, NULL); // NEW

        // "Reset" -- top of the panel. A plain BS_PUSHBUTTON, not
        // BS_AUTOCHECKBOX -- this is a one-shot action, not a persistent
        // on/off state, so it has nothing to stay "checked" as. Created
        // without WS_VISIBLE -- appMode starts in Design, not Simulation,
        // so it should start hidden; WM_COMMAND's ID_MODE_SIMULATION
        // handling shows/hides it from then on.
        hResetBtn = CreateWindowEx(0, L"BUTTON", L"Reset",
                            WS_CHILD | BS_PUSHBUTTON,
                            10, 10, 120, 28, hWnd, (HMENU)ID_RESET_ROBOT,
                            GetModuleHandle(NULL), NULL);

        // Slow Motion toggle -- directly below Reset (28px tall + 6px
        // gap). Same BS_AUTOCHECKBOX | BS_PUSHLIKE "checkbox that
        // looks/behaves like a toggle button" style ui.c's Trace/
        // Comparison Mode buttons use, same hidden-until-Simulation
        // treatment.
        hSlowMotionBtn = CreateWindowEx(0, L"BUTTON", L"Slow Motion",
                            WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                            10, 44, 120, 28, hWnd, (HMENU)ID_SLOW_MOTION,
                            GetModuleHandle(NULL), NULL);

        // "Walk" toggle (Shift+W) button removed for now -- hWalkBtn is
        // left NULL (its window is simply never created). All other
        // usages of hWalkBtn elsewhere in this file are already guarded
        // by "if (hWalkBtn)", so this is safe without touching them; the
        // underlying gaitActive/Shift+W machinery is untouched, only the
        // visible button is gone.

        // Environment autosave: restore whatever was last drawn before the
        // app was closed, so a fresh launch shows the last environment
        // instead of always starting blank. See loadEnvironmentAutosave's
        // own comment -- a no-op if nothing was ever autosaved yet.
        loadEnvironmentAutosave();
        return 0;
    }
    case WM_TIMER:
    {
        // Shift+G's "auto gravity" -- see WM_KEYDOWN's toggle above. This is
        // just the steady fallback driver (see SIMULATION_AUTO_GRAVITY_
        // INTERVAL_MS's comment, config.h, for why WM_MOUSEMOVE also calls
        // advanceAutoGravity directly) -- the real step/acceleration math
        // lives in advanceAutoGravity so both call sites can never disagree.
        // Also bails and turns itself off if Simulation mode was left some
        // other way (e.g. switching Design Mode from the menu) without going
        // through the mode-switch cleanup in WM_COMMAND -- defensive, since
        // it wouldn't make sense to keep running outside Simulation.
        if (wParam == AUTO_GRAVITY_TIMER_ID)
        {
            if (appMode == APP_MODE_SIMULATION)
            {
                advanceAutoGravity(hWnd);
                advancePostRotateSettle(hWnd);
            }
            else
            {
                autoGravityActive = FALSE;
                postRotateSettleActive = FALSE;
                KillTimer(hWnd, AUTO_GRAVITY_TIMER_ID);
            }
            return 0;
        }

        // Shift+W's "Walk" -- same fallback-driver/defensive-shutoff
        // structure as AUTO_GRAVITY_TIMER_ID just above, own timer ID so
        // gravity and the gait cycle can each be toggled independently.
        if (wParam == AUTO_GAIT_TIMER_ID)
        {
            if (appMode == APP_MODE_SIMULATION)
            {
                advanceGait(hWnd);
            }
            else
            {
                gaitActive = FALSE;
                KillTimer(hWnd, AUTO_GAIT_TIMER_ID);
            }
            return 0;
        }

        // Keeps the gravity toast's fade animating even after auto-gravity's
        // own timer has stopped ticking (e.g. the "AUTO GRAVITY OFF" toast,
        // or "ON" if the robot lands and applyGravityStep's per-tick
        // InvalidateRect calls happen to line up exactly with a paint
        // that's already up to date). Piggybacks on the always-running
        // hot-zone timer rather than starting a third one just for this.
        // Stops invalidating on its own once the fade duration has fully
        // elapsed (gravityToastStartTick reset to 0), so this doesn't
        // repaint forever.
        if (wParam == UI_HOTZONE_TIMER_ID && gravityToastStartTick != 0)
        {
            DWORD elapsed = GetTickCount() - gravityToastStartTick;
            if (elapsed >= (DWORD)(SIMULATION_GRAVITY_TOAST_HOLD_MS + SIMULATION_GRAVITY_TOAST_FADE_MS))
                gravityToastStartTick = 0;

            InvalidateRect(hWnd, NULL, FALSE);
        }

        // Same fade-keeping piggyback as the gravity toast just above, for
        // the "WALK ON"/"WALK OFF" toast.
        if (wParam == UI_HOTZONE_TIMER_ID && gaitToastStartTick != 0)
        {
            DWORD elapsed = GetTickCount() - gaitToastStartTick;
            if (elapsed >= (DWORD)(SIMULATION_GRAVITY_TOAST_HOLD_MS + SIMULATION_GRAVITY_TOAST_FADE_MS))
                gaitToastStartTick = 0;

            InvalidateRect(hWnd, NULL, FALSE);
        }

        // Same fade-keeping piggyback again, for the rotation HUD -- only
        // needed once the key's already up (rotateToastStartTick != 0);
        // while rotateKeyHeld is still TRUE, WM_KEYDOWN's own auto-repeat
        // and the settle timer's per-tick InvalidateRect already keep it
        // current every frame on their own.
        if (wParam == UI_HOTZONE_TIMER_ID && rotateToastStartTick != 0)
        {
            DWORD elapsed = GetTickCount() - rotateToastStartTick;
            if (elapsed >= (DWORD)(SIMULATION_GRAVITY_TOAST_HOLD_MS + SIMULATION_GRAVITY_TOAST_FADE_MS))
                rotateToastStartTick = 0;

            InvalidateRect(hWnd, NULL, FALSE);
        }

        // NEW: shift-line dwell-to-snap. If the cursor has been basically
        // stationary for SHIFT_HOLD_SNAP_MS while drawing a Shift-line, and
        // its raw angle from the line's start point is close to horizontal,
        // vertical, or a 45/135-degree diagonal, lock the line to that exact
        // angle. Has to live here (not WM_MOUSEMOVE) because a genuinely
        // still cursor produces no WM_MOUSEMOVE messages at all - this timer
        // tick, which already runs every 16ms for the UI hot-zone check
        // below, is what actually notices the elapsed time.
        //
        // NEW: once snapped, this block keeps RE-ASSERTING the locked
        // endpoint every tick (not just once) for as long as the hold is
        // still active - rather than writing it a single time and trusting
        // it to survive untouched. This makes it self-healing: if anything
        // else (a stray/no-op mouse message, etc.) leaves canvas.pointCount
        // truncated to just the start point, the very next 16ms tick puts
        // the endpoint straight back rather than leaving the line missing.
        if (wParam == UI_HOTZONE_TIMER_ID && drawing && shiftHoldActive &&
            (GetAsyncKeyState(VK_SHIFT) & 0x8000))
        {
            if (!shiftHoldSnapped &&
                (GetTickCount() - shiftHoldLastMoveTick) >= SHIFT_HOLD_SNAP_MS)
            {
                float startX = points[shiftHoldStrokeStart];
                float startY = points[shiftHoldStrokeStart + 1];
                float relX = shiftHoldWorldX - startX;
                float relY = shiftHoldWorldY - startY;
                float len = sqrtf(relX * relX + relY * relY);

                if (len > 1e-4f)
                {
                    float deg = atan2f(relY, relX) * (180.0f / 3.14159265f);
                    float nearest45 = roundf(deg / 45.0f) * 45.0f;
                    float diff = fabsf(deg - nearest45);

                    if (diff <= SHIFT_HOLD_TOLERANCE_DEG)
                    {
                        shiftHoldSnapped      = TRUE;
                        shiftHoldSnapAngleRad = nearest45 * (3.14159265f / 180.0f);
                    }
                }
            }

            if (shiftHoldSnapped)
            {
                float startX = points[shiftHoldStrokeStart];
                float startY = points[shiftHoldStrokeStart + 1];
                float dirX = cosf(shiftHoldSnapAngleRad);
                float dirY = sinf(shiftHoldSnapAngleRad);
                float relX = shiftHoldWorldX - startX;
                float relY = shiftHoldWorldY - startY;
                float proj = relX * dirX + relY * dirY;

                canvas.pointCount = shiftHoldStrokeStart + 4;
                points[shiftHoldStrokeStart + 2] = startX + dirX * proj;
                points[shiftHoldStrokeStart + 3] = startY + dirY * proj;

                if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
            }
        }

        // NEW: reveal the UI panel when hovering the GL window's top-right
        // corner, or when the panel is already up and the cursor is over it
        // (so you don't lose it mid-click); hide it otherwise. Fades smoothly
        // instead of popping instantly.
        if (wParam == UI_HOTZONE_TIMER_ID && hWndUI)
        {
            POINT pt;
            GetCursorPos(&pt);

            // NEW: GetCursorPos/PtInRect only ever compare raw SCREEN
            // coordinates - they have no idea whether our window is
            // actually the one visible at that point, or buried behind
            // some other app (Discord, a browser, ...) that just happens
            // to overlap the same screen region as our hot zone. Without
            // this, moving the mouse over that other app's window while
            // ours sits behind it still read as "cursor is in the hot
            // zone" and popped the panel up on top of everything.
            // WindowFromPoint respects real Z-order/occlusion - it
            // returns whichever window would actually receive a click at
            // that exact screen point - so this only counts hovers that
            // are genuinely over one of OUR windows (the main GL window,
            // the panel itself, or one of the panel's own child controls).
            HWND wndAtPt = WindowFromPoint(pt);
            BOOL cursorOverOurWindows = wndAtPt != NULL &&
                                        ((wndAtPt == hWnd) ||
                                         (wndAtPt == hWndUI) ||
                                         IsChild(hWndUI, wndAtPt));

            // NEW: use the CLIENT area's top-right corner, not the full
            // window rect - GetWindowRect() includes the title bar, which
            // would (a) make the hot zone drift up into the title bar
            // instead of matching where the indicator is actually drawn,
            // and (b) place the panel over the title bar, covering the
            // close button.
            RECT glClientRect;
            GetClientRect(hWnd, &glClientRect);
            POINT clientTopRight = { glClientRect.right, glClientRect.top };
            ClientToScreen(hWnd, &clientTopRight);

            RECT hotZone;
            hotZone.right  = clientTopRight.x;
            hotZone.left   = clientTopRight.x - UI_HOTZONE_WIDTH;
            hotZone.top    = clientTopRight.y;
            hotZone.bottom = clientTopRight.y + UI_HOTZONE_HEIGHT;

            BOOL inHotZone = cursorOverOurWindows && PtInRect(&hotZone, pt);

            if (inHotZone != hotZoneHighlighted)
            {
                hotZoneHighlighted = inHotZone;   // NEW: drives the corner indicator's look
                InvalidateRect(hWnd, NULL, FALSE);
            }

            BOOL inUIWindow = FALSE;
            if (uiShown && cursorOverOurWindows)
            {
                RECT uiRect;
                GetWindowRect(hWndUI, &uiRect);
                inUIWindow = PtInRect(&uiRect, pt);
            }

            BOOL wantVisible = inHotZone || inUIWindow;

            // This panel is Design > Environment-only tooling (Clear,
            // thickness, color, Trace, View Segments, Comparison Mode) --
            // none of it applies to Robot design mode or Simulation, so
            // only ever let the hot corner reveal it while Environment is
            // actually the active design layer, even if the cursor is
            // sitting in its hot corner in some other mode.
            BOOL envDesignActive = (appMode == APP_MODE_DESIGN && designLayer == LAYER_ENVIRONMENT);
            if (!envDesignActive)
                wantVisible = FALSE;

            if (wantVisible && !uiShown)
            {
                // Just entered the hot zone: position it flush against the
                // client area's top-right corner (below the title bar) and
                // start fully transparent, then fade in below.
                RECT uiRectCur;
                GetWindowRect(hWndUI, &uiRectCur);
                int uiW = uiRectCur.right - uiRectCur.left;

                uiAlpha = 0;
                SetLayeredWindowAttributes(hWndUI, 0, (BYTE)uiAlpha, LWA_ALPHA);
                SetWindowPos(hWndUI, HWND_TOPMOST,
                             clientTopRight.x - uiW, clientTopRight.y,
                             0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
                uiShown = TRUE;
            }

            if (uiShown)
            {
                int target = wantVisible ? 255 : 0;

                if (uiAlpha != target)
                {
                    if (uiAlpha < target) {
                        uiAlpha += UI_FADE_STEP;
                        if (uiAlpha > target) uiAlpha = target;
                    } else {
                        uiAlpha -= UI_FADE_STEP;
                        if (uiAlpha < target) uiAlpha = target;
                    }
                    SetLayeredWindowAttributes(hWndUI, 0, (BYTE)uiAlpha, LWA_ALPHA);
                }

                if (!wantVisible && uiAlpha <= 0)
                {
                    ShowWindow(hWndUI, SW_HIDE);
                    uiShown = FALSE;
                }
            }
        }
        return 0;
    }
    case WM_SIZE:
    {
        glWindowWidth = LOWORD(lParam);
        glWindowHeight = HIWORD(lParam);
        if (glWindowWidth == 0) glWindowWidth = 1;
        if (glWindowHeight == 0) glWindowHeight = 1;
        glViewport(0, 0, glWindowWidth, glWindowHeight);
        UpdateProjection();

        // Simulation mode's ground-collision data (segmentPointsWorld/
        // segmentThicknessWorld) is traced from a raster sized to
        // glWindowWidth/glWindowHeight AT THE MOMENT Simulation was
        // entered (see canvasToImage/canvas_bridge.c) -- resizing the
        // window afterward (maximizing, going fullscreen, etc.) changes
        // that same pixel<->world mapping for everything rendered live
        // (including the environment stroke's own on-screen thickness),
        // but does nothing to the already-traced data, which just sits
        // there representing the OLD window size. The environment line
        // then visibly drifts out of sync with what collision is still
        // testing against -- exactly the "looks flush windowed, floats
        // once maximized/zoomed" symptom this fixes. Re-tracing here is
        // the other moment (besides ID_MODE_SIMULATION's own entry-point
        // trace, see its comment) where the trace goes stale through no
        // drawing action of the user's -- only through the window itself
        // changing size while Simulation is already active.
        //
        // wParam == SIZE_MINIMIZED is excluded deliberately: Windows
        // reports a minimized window's client area as 0x0, which the
        // glWindowWidth/Height == 0 guards just above clamp to 1x1 purely
        // so glViewport/UpdateProjection don't divide by zero -- tracing
        // against that degenerate 1x1 raster would collapse the whole
        // environment down to nothing, silently deleting every segment
        // collision tests against (robot then free-falls forever, since
        // nothing is left to land on). Minimizing never actually changes
        // the size the user will resume looking at, so there's nothing
        // real to re-trace here anyway -- the next genuine resize (e.g.
        // restoring the window) retraces correctly on its own.
        if (appMode == APP_MODE_SIMULATION && canvas.segmentResultCount > 0
            && wParam != SIZE_MINIMIZED)
            RunTracePipeline();

        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

        // Simulation mode, hovering the hip or knee body circle WHILE
        // HOLDING SHIFT: rotate that joint instead of zooming -- see
        // simHoveredJoint/findHoveredJointSim's comments. Gated behind
        // Shift for the same reason Design > Robot mode's own hip/knee
        // scroll-rotate is (input.c's WM_MOUSEWHEEL): a plain scroll while
        // the cursor happens to be over a joint should still zoom, same as
        // scrolling anywhere else, rather than surprise-rotating a limb.
        // WM_MOUSEWHEEL packs the modifier keys held during the scroll
        // into the low word of wParam (MK_SHIFT), same as WM_MOUSEMOVE --
        // no GetAsyncKeyState polling needed (same trick input.c's own
        // version already uses).
        if (appMode == APP_MODE_SIMULATION && simHoveredJoint != -1 && (LOWORD(wParam) & MK_SHIFT))
        {
            float step = (zDelta > 0) ? SIMULATION_JOINT_ROTATE_STEP_DEG : -SIMULATION_JOINT_ROTATE_STEP_DEG;

            if (simHoveredJoint == 2)      app.robotScene.robot.hipAngle  += step;
            else if (simHoveredJoint == 3) app.robotScene.robot.kneeAngle += step;

            // Rotating a joint can push the limb hanging off it into the
            // ground if the robot's already resting on something -- same
            // upward-only correction slope response uses after its own
            // rotation, so a joint tweak mid-rest can never visibly sink
            // through the terrain.
            resolveUpwardIfPenetrating(hWnd, SIMULATION_SLOPE_CORRECTION_MAX);

            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        // Simulation mode: zoom through sim_camera (shared with the robot
        // -- see UpdateProjection/sim_camera.h) instead of canvas.zoom, so
        // Design > Environment's own zoom is left untouched by anything
        // that happens while simulating.
        //
        // Design > Robot's own camera zoom (graphics.c's g_zoom) is
        // deliberately advanced by the SAME factor right alongside
        // sim_camera's, even though applyProjection ignores g_zoom entirely
        // while Simulation is active (see its own comment) -- so it has no
        // effect on anything drawn THIS frame. The point is purely for
        // later: g_zoom and sim_camera's zoom start at the same default
        // (1.0) and share the exact same MIN_ZOOM/MAX_ZOOM clamp (config.h),
        // so stepping both by the same ZOOM_STEP factor here keeps them in
        // exact lockstep -- meaning leaving Simulation and going back into
        // Design > Robot lands on the same zoom level you were just looking
        // at, instead of snapping back to whatever g_zoom was last left at
        // before you ever entered Simulation.
        if (appMode == APP_MODE_SIMULATION)
        {
            float simZoomFactor = (zDelta > 0) ? ZOOM_STEP : (1.0f / ZOOM_STEP);
            simCameraZoom(simZoomFactor);
            graphicsZoom(simZoomFactor);
        }
        else
        {
            if (zDelta > 0) canvas.zoom *= 0.9f;
            else            canvas.zoom *= 1.1f;
            if (canvas.zoom < 0.1f)  canvas.zoom = 0.1f;
            if (canvas.zoom > 10.0f) canvas.zoom = 10.0f;
        }

        UpdateProjection();

        // Defensive safety net, Simulation mode only: zoom itself never
        // legitimately moves the robot relative to the environment (the
        // shared sim_camera conversion robotPointToEnvWorld uses is exact
        // and zoom-independent by construction), but a real report of the
        // robot slipping through the ground specifically while scrolling
        // to zoom -- worst on an unstable, narrow-footprint resting pose
        // (e.g. standing on a short edge) -- kept showing up regardless of
        // the tunneling fixes already made elsewhere (advanceAutoGravity's
        // sub-stepping, dropActiveRobotToRest's open-air check). Rapid
        // WM_MOUSEWHEEL scrolling forces a synchronous UpdateWindow on
        // every single notch, which is exactly the kind of message-queue
        // pressure that can cause an ordinary settle/gravity tick landing
        // in between those notches to end up very slightly embedded
        // instead of cleanly resting, same root cause as the auto-gravity
        // stall case. Rather than keep chasing the exact interleaving,
        // this re-validates right after every zoom step and pushes back
        // out (same bounded, collision-confirmed correction the joint-
        // rotate and Left/Right handlers already use elsewhere) if the
        // robot is currently colliding at all -- a complete no-op whenever
        // it genuinely isn't, which is the overwhelming majority of zoom
        // events.
        if (appMode == APP_MODE_SIMULATION)
            resolveUpwardIfPenetrating(hWnd, SIMULATION_SLOPE_CORRECTION_MAX);

        InvalidateRect(hWnd, NULL, FALSE);
        UpdateWindow(hWnd);
        return 0;
    }
    case WM_KEYDOWN:
    {
        // Ctrl+Numpad0: reset the view (zoom to 100%, pan back to center --
        // i.e. canvas.panX/panY or sim_camera's pan back to exactly 0, not
        // just whatever this session's camera happened to start at) -- this
        // WndProc handles both Design > Environment and Simulation (see
        // editorModeState.currentMode's routing in main.c's
        // WndProcShared -- Simulation forces EDITOR_MODE_ARCSPLINE so it
        // ends up here too), so branch the same way WM_MOUSEWHEEL/panning
        // above do: Simulation resets sim_camera, Environment resets
        // canvas.zoom/panX/panY. Same shortcut as Design > Robot's own
        // Ctrl+Numpad0 (see input.c's WM_KEYDOWN). Deliberately VK_NUMPAD0,
        // not the top-row '0' -- top-row digits are VK_0..VK_9 (0x30-0x39,
        // same as their ASCII codes) while the numpad digits are their own
        // separate VK_NUMPAD0..VK_NUMPAD9 (0x60-0x69) range, so this only
        // fires for the numpad key specifically.
        if (wParam == VK_NUMPAD0 && (GetAsyncKeyState(VK_CONTROL) & 0x8000))
        {
            if (appMode == APP_MODE_SIMULATION)
            {
                simCameraReset();
            }
            else
            {
                canvas.zoom = 1.0f;
                canvas.panX = 0.0f;
                canvas.panY = 0.0f;
            }

            UpdateProjection();
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        // Shift (pressed while actively drawing, i.e. mid-drag): kicks off
        // the straight-line constraint immediately, using WHEREVER the
        // cursor already is, instead of waiting for it to also move.
        // updateDrawingPoint (used here and by WM_MOUSEMOVE) is only ever
        // driven by WM_MOUSEMOVE otherwise -- and Windows does NOT send
        // WM_MOUSEMOVE just because a modifier key's state changed, only
        // on actual cursor movement -- so pressing Shift mid-stroke used to
        // visibly do nothing at all until the user nudged the mouse
        // afterward, even though the key registered fine. Explicitly
        // pulling the current cursor position here (GetCursorPos, then
        // ScreenToClient to match lParam's coordinate space) and running
        // the exact same logic closes that gap: the line preview appears
        // the instant Shift goes down, full stop.
        //
        // Guarded by the lParam bit 30 auto-repeat check (same reasoning
        // as Shift+G below) so holding Shift down doesn't re-run this on
        // every OS key-repeat tick -- harmless if it did (idempotent), just
        // wasted work.
        if (wParam == VK_SHIFT && drawing && appMode != APP_MODE_SIMULATION)
        {
            BOOL isAutoRepeat = (lParam & 0x40000000) != 0;
            if (!isAutoRepeat)
            {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hWnd, &pt);
                updateDrawingPoint(hWnd, pt.x, pt.y, TRUE, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            }
            return 0;
        }

        // Shift+G: toggles "auto gravity" on/off (Simulation mode only) --
        // starts/stops AUTO_GRAVITY_TIMER_ID, which calls applyGravityStep
        // on its own every SIMULATION_AUTO_GRAVITY_INTERVAL_MS (config.h)
        // without needing G held down. Checked BEFORE the plain-G branch
        // below, since Shift+G would otherwise also satisfy `wParam == 'G'`
        // (Shift is a separate modifier key, not part of wParam here).
        //
        // Unlike plain G (which WANTS Windows' auto-repeat -- see below),
        // a toggle has to ignore it: lParam bit 30 is set on every repeated
        // WM_KEYDOWN while a key is held, clear only on the very first one,
        // so holding Shift+G down would otherwise flip autoGravityActive
        // rapidly on/off/on/... instead of toggling once per actual press.
        if (wParam == 'G' && appMode == APP_MODE_SIMULATION && (GetAsyncKeyState(VK_SHIFT) & 0x8000))
        {
            BOOL isAutoRepeat = (lParam & 0x40000000) != 0;

            if (!isAutoRepeat)
            {
                autoGravityActive = !autoGravityActive;
                autoGravityVelocity = 0.0f; // (re)start every toggle from rest
                autoGravityLastTickTime = GetTickCount(); // avoid a huge first elapsed-time jump

                if (autoGravityActive)
                    SetTimer(hWnd, AUTO_GRAVITY_TIMER_ID, SIMULATION_AUTO_GRAVITY_INTERVAL_MS, NULL);
                else
                    KillTimer(hWnd, AUTO_GRAVITY_TIMER_ID);

                // Kick off the bottom-left "AUTO GRAVITY ON/OFF" toast --
                // canvasRenderFrame reads these two each frame to compute
                // its fade. gravityToastOn mirrors the state we just
                // switched TO, not the one we switched from.
                gravityToastOn = autoGravityActive;
                gravityToastStartTick = GetTickCount();
                InvalidateRect(hWnd, NULL, FALSE);
            }

            return 0;
        }

        // G ("gravity"): Simulation mode only -- nudges the whole robot
        // straight down by SIMULATION_GRAVITY_STEP (config.h) via
        // applyGravityStep, same translateRobot the whole-robot drag uses,
        // so it's a pure rigid move that doesn't touch the pose. "Holding G
        // repeats it" is just Windows' own WM_KEYDOWN auto-repeat firing
        // this same branch again and again for as long as the key stays
        // down -- no separate timer or held-key tracking needed here
        // (unlike Shift+G's auto-gravity above, which has to keep going
        // even after the key is released).
        if (wParam == 'G' && appMode == APP_MODE_SIMULATION)
        {
            // Scaled by the active robot's own weight (see
            // activeRobotWeightFactor's comment) so a heavier robot drops
            // a bigger notch per press/repeat, same idea as auto-gravity's
            // acceleration scaling just below.
            applyGravityStep(hWnd, SIMULATION_GRAVITY_STEP * activeRobotWeightFactor());
            return 0;
        }

        // Shift+W: toggles the scripted gait cycle ("Walk") on/off --
        // same structure as Shift+G's auto-gravity toggle just above
        // (own timer, own auto-repeat guard, own toast), independent of
        // it -- gravity and Walk can each be on or off regardless of the
        // other. Captures gaitBaseline (the pose the cycle swings around)
        // fresh every time it's switched ON, so re-toggling Walk after
        // posing the robot differently restarts the cycle around the NEW
        // pose rather than snapping back to whatever it was the first
        // time Walk was turned on this session.
        if (wParam == 'W' && appMode == APP_MODE_SIMULATION && (GetAsyncKeyState(VK_SHIFT) & 0x8000))
        {
            BOOL isAutoRepeat = (lParam & 0x40000000) != 0;

            if (!isAutoRepeat)
                ToggleGait(hWnd);

            return 0;
        }

        // E / Q (no modifier): rotate Rocky's own knee joint (kneeAngle) in
        // Simulation mode, without needing to hover its body circle first
        // (unlike WM_MOUSEWHEEL's Shift+scroll-over-the-joint gesture
        // above -- simHoveredJoint/findHoveredJointSim only ever track
        // Semni's hip/knee circles, and Rocky has no separate hip stage to
        // hover anyway, just this one knee->foot joint hanging off the
        // rectangle). Deliberately plain (no Shift, unlike Shift+G/Shift+W
        // above) per explicit user request. Only acts while Rocky is the
        // active robot kind, same "harmlessly does nothing for the other
        // kinds" convention as Semni-only features elsewhere in this file.
        //
        // rotatePoint (geometry.c) is the standard CCW-for-positive-angle
        // matrix in this app's Y-up world space, so clockwise on screen is
        // the NEGATIVE direction: E (clockwise) subtracts the step, Q
        // (counter-clockwise) adds it -- same step size
        // (SIMULATION_JOINT_ROTATE_STEP_DEG) as the scroll-wheel joint
        // rotate, and same "let Windows' own auto-repeat handle holding
        // the key down" treatment as plain G above (a continuous rotate
        // needs no repeat/timer bookkeeping here).
        if ((wParam == 'E' || wParam == 'Q') && appMode == APP_MODE_SIMULATION && app.robotScene.activeKind == ROBOT_KIND_ROCKY)
        {
            // E/Q's entire job is bending the knee -- with the leg removed
            // (ID_ROCKY_TOGGLE_LEG_BUTTON) there's no visible/collidable
            // knee left to bend, so do nothing at all rather than letting
            // everything below still run on an invisible leg. It used to:
            // rotate kneeAngle (harmlessly, since nothing draws or collides
            // with it -- see advanceRockySettle's own !r->legHidden comment
            // on Probe 1), then unconditionally call dropActiveRobotToRest()
            // and kick off the postRotateSettleActive timer regardless --
            // dropActiveRobotToRest applies its FULL correction in one
            // synchronous jump (that's deliberate for a real knee rotation,
            // to avoid a two-frame "jump then land" -- see its own call
            // site's comment), so on a legless Rocky a single E/Q press
            // read as the whole rectangle instantly teleporting to the
            // ground, and the settle timer it started afterward then kept
            // running on top of that. Bailing out before any of it -- no
            // kneeAngle change, no drop, no settle kickoff -- makes E/Q a
            // true no-op for a legless Rocky, exactly like the button being
            // gone in the first place.
            if (app.robotScene.rocky.legHidden)
                return 0;

            // By explicit request: E/Q ONLY rotates the knee joint, full
            // stop -- no LEGPUSH lift, no dropActiveRobotToRest snap, no
            // settle-timer kickoff, nothing else touches the robot's
            // position. All of that used to run automatically after every
            // knee rotation specifically to keep the leg from visibly
            // passing through the ground -- but each layer of that also
            // ended up moving the BODY itself (a push up, a snap down, a
            // gradual fall), which is exactly what kept reading as the
            // robot falling/teleporting from a plain knee press instead of
            // just the knee moving. If the new angle leaves the leg
            // embedded in the ground, that is left exactly as-is here --
            // nothing in this handler corrects it or starts anything that
            // would. Auto Gravity (Shift+G) or a manual drag are still
            // there if the user wants that resolved; this key no longer
            // does it on their behalf.
            Rocky* r = &app.robotScene.rocky;
            float step = (wParam == 'E') ? -SIMULATION_JOINT_ROTATE_STEP_DEG : SIMULATION_JOINT_ROTATE_STEP_DEG;
            r->kneeAngle += step;

            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        // Plain C (no modifier): toggles the "show contact" debug overlay
        // -- see drawSimulationContactDebug's own comment for exactly what
        // it draws and why. Not gated to any one robot kind (unlike E/Q)
        // since it's just as useful for Semni/Stilo.
        if (wParam == 'C' && appMode == APP_MODE_SIMULATION)
        {
            showContactDebug = !showContactDebug;
            contactDebugLoggedThisToggle = FALSE; // re-arm the one-shot dump below
            printf("[CONTACT] overlay toggled %s (activeKind=%d)\n",
                   showContactDebug ? "ON" : "off", app.robotScene.activeKind);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        // Plain Left/Right (no Shift): rotates the WHOLE robot, not any
        // one joint -- joints are a Shift+scroll-over-the-hovered-circle
        // action instead (WM_MOUSEWHEEL above), gated behind Shift
        // specifically so a plain scroll still zooms (see that branch's
        // own comment) -- unrelated to this. Same angle field and same
        // per-press step Design > Robot mode's own Left/Right already
        // nudges (input.c's WM_KEYDOWN, `angle += 2.0f` / `-= 2.0f`),
        // same Left = positive / Right = negative sign convention too,
        // so a press feels identical between the two editors. Windows'
        // own key-repeat re-fires this while held, same as plain G
        // above, so no repeat bookkeeping needed.
        //
        // Dispatches on activeKind now -- this used to unconditionally
        // write app.robotScene.robot.angle (Semni's own field) no matter
        // which kind was actually active, so a real report showed Left/
        // Right silently doing nothing visible while Rocky or Stilo was
        // selected: it WAS rotating something, just the invisible,
        // inactive Semni sitting behind it, not the robot actually on
        // screen. Every other kind-specific action in this file (E/Q,
        // robotCollidesWithEnvironment, applyGravityStep's settle, etc.)
        // already reads app.robotScene.activeKind for exactly this
        // reason -- this was the one plain-rotate branch that never got
        // the same treatment.
        if ((wParam == VK_LEFT || wParam == VK_RIGHT) && appMode == APP_MODE_SIMULATION)
        {
            // Rotation HUD: show it (at full opacity, no fade) for as long
            // as the key stays down -- Windows auto-repeat re-fires
            // WM_KEYDOWN every press-and-hold, so this just re-stamps TRUE
            // each time rather than needing any "first press" detection.
            // See rotateKeyHeld's own comment for the rest of the mechanics.
            rotateKeyHeld = TRUE;

            float step = (wParam == VK_LEFT) ? SIMULATION_WHOLE_BODY_ROTATE_STEP_DEG : -SIMULATION_WHOLE_BODY_ROTATE_STEP_DEG;

            // Sub-stepped (see SIMULATION_WHOLE_BODY_ROTATE_SUBSTEP_DEG's
            // own comment for why a single big jump can tunnel a far
            // corner clean through the ground line when resting on a
            // narrow edge): walk the full press's rotation in several
            // smaller pieces, re-checking/correcting after each one
            // instead of only once at the very end.
            float remaining = step;
            while (remaining != 0.0f)
            {
                float sub = SIMULATION_WHOLE_BODY_ROTATE_SUBSTEP_DEG;
                if (sub > fabsf(remaining)) sub = fabsf(remaining);
                if (remaining < 0.0f) sub = -sub;

                switch (app.robotScene.activeKind)
                {
                    case ROBOT_KIND_ROCKY:
                        app.robotScene.rocky.angle += sub;
                        break;
                    case ROBOT_KIND_STILO:
                        app.robotScene.stilo.angle += sub;
                        break;
                    case ROBOT_KIND_SEMNI:
                    default:
                        app.robotScene.robot.angle += sub;
                        break;
                }

                // Same re-settle WM_MOUSEWHEEL's joint-rotate branch and
                // slope response both use -- keeps a rotate from visibly
                // sinking the robot into ground it's already resting on.
                // Already reads activeKind internally
                // (robotCollidesWithEnvironment's own switch), so no
                // change needed here to act on the right kind. Called
                // after EVERY sub-step (not just once at the end) so a
                // corner sweeping toward the ground is caught and pushed
                // back up as soon as it first touches, rather than only
                // after the whole press has already been applied.
                //
                // Skipped for a legless Rocky specifically: with no leg
                // holding the rectangle up off the ground, its own edge is
                // what's normally resting flush against it, so this
                // center-spin rotation routinely embeds one corner every
                // single press -- pushing that corner back up here, only
                // for WM_KEYUP's own settle pass to immediately pull the
                // whole body back down again to find the new resting
                // angle, is what actually produced the visible "one tap
                // and the rectangle drops" a real report described (see
                // WM_KEYUP's own comment on that settle pass, unconditional
                // by explicit request). A legged Rocky doesn't have this
                // problem -- it's normally resting on the foot/knee, well
                // clear of the rectangle's own corners, so this almost
                // never fires for it anyway. Leaving the corner embedded
                // here instead just lets applyGravityStep's own contact
                // search (inside that same settle pass) resolve it in one
                // direction -- back up to the true contact point -- rather
                // than pushing up and then immediately dropping back down.
                //
                // Also skipped for a leg-only Rocky (bodyHidden, leg still
                // visible): same reported symptom ("rotating it with
                // arrows, steps the gravity"), same fix as the legless
                // case, by explicit request -- arrow-key rotation for this
                // configuration should just rotate, nothing else.
                if (!(app.robotScene.activeKind == ROBOT_KIND_ROCKY
                      && (app.robotScene.rocky.legHidden || app.robotScene.rocky.bodyHidden)))
                    resolveUpwardIfPenetrating(hWnd, SIMULATION_SLOPE_CORRECTION_MAX);

                remaining -= sub;
            }

            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        return 0;
    }
    case WM_KEYUP:
    {
        // Kick off a topple the instant a manual whole-body rotate is
        // released, in case it left the robot resting somewhere no
        // longer actually stable (or lifted clean off the ground) --
        // see advancePostRotateSettle's own comment for the full
        // reasoning. Re-arms rockySettleConverged/both step sizes first,
        // same fix E/Q's own knee-push re-arm above uses and for the
        // same reason: without it, a robot that had already fully
        // settled before this rotate would leave rockySettleConverged
        // still TRUE, and advanceRockySettle would just no-op forever --
        // the topple would never actually move anything.
        if ((wParam == VK_LEFT || wParam == VK_RIGHT) && appMode == APP_MODE_SIMULATION)
        {
            // Rotation HUD: the key's up, so stop showing it live and start
            // the same hold-then-fade countdown the gravity/Walk toasts use
            // (see rotateKeyHeld's own comment) -- InvalidateRect right here
            // rather than waiting for the settle timer's own next tick, so
            // the fade visibly starts on this exact frame instead of
            // whatever the first AUTO_GRAVITY_TIMER_ID tick happens to be.
            rotateKeyHeld = FALSE;
            rotateToastStartTick = GetTickCount();
            InvalidateRect(hWnd, NULL, FALSE);

            // Skipped entirely for a legless Rocky: this settle pass is
            // exactly what a real report showed as "pressing left/right
            // once moves the robot down" -- postRotateSettleActive's own
            // driver (advancePostRotateSettle) calls applyGravityStep --
            // a REAL gravity step, translating the robot down first and
            // only correcting back if that step happened to land -- every
            // single tick for as long as it takes to reconverge, and with
            // no leg (Probe 1 skipped, Probe 2 alone driving convergence,
            // no push-up left to undo now that WM_KEYDOWN's own corner-fix
            // is skipped too -- see its own comment just above) that takes
            // long enough to read as a real, visible fall rather than an
            // imperceptible snap. A legged Rocky keeps this exactly as
            // before -- it's normally resting on the foot/knee already,
            // so this pass is nearly always a no-op for it, and removing
            // it there was never what was reported as broken.
            //
            // Also skipped for a leg-only Rocky (bodyHidden): same real
            // gravity step, same visible "rotating it steps the gravity"
            // symptom, same fix, by explicit request -- arrow-key rotation
            // here should just rotate, no automatic settle/drop tacked on.
            if (!(app.robotScene.activeKind == ROBOT_KIND_ROCKY
                  && (app.robotScene.rocky.legHidden || app.robotScene.rocky.bodyHidden)))
            {
                rockySettleConverged = FALSE;
                rockyKneeSettleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
                rockyBodySettleStep = SIMULATION_BODY_SETTLE_STEP_DEG;
                rockyKneeSettleSuppressed = FALSE; // whole-body rotate: nothing else drives kneeAngle, Probe 1 is safe here

                postRotateSettleActive = TRUE;
                SetTimer(hWnd, AUTO_GRAVITY_TIMER_ID, SIMULATION_AUTO_GRAVITY_INTERVAL_MS, NULL);
            }
        }

        // A single E/Q tap (or a held one, via Windows auto-repeat
        // re-arming rockyKneeSettleSuppressed on every WM_KEYDOWN) leaves
        // Probe 1 (knee-bending) suppressed for the whole settle session
        // that follows it -- see rockyKneeSettleSuppressed's own comment.
        // Nothing else ever clears it back to FALSE for Rocky: the
        // "normal" path (advancePostRotateSettle's atRest branch, right
        // above) can only fire once BOTH rockyKneeSettleStep and
        // rockyBodySettleStep have shrunk below
        // SIMULATION_LEG_SETTLE_MIN_STEP_DEG, but Probe 1's own step-
        // shrinking code lives entirely inside the block this suppression
        // skips -- so while suppressed, rockyKneeSettleStep never moves,
        // convergence can never happen, and un-suppression (which only
        // happens inside that same atRest branch) can't either. That's a
        // permanent deadlock: after just one E/Q tap, Probe 1 stays dead
        // for the rest of the session, and if closing some residual gap
        // genuinely needed knee bending (not just the body rotate Probe 2
        // still runs) rather than a plain vertical drop, the robot is
        // left visibly floating with nothing left able to fix it. This is
        // what a real report described as "i only pressed e once and the
        // robot is already floating". Releasing the key is the real
        // signal that the user is done driving kneeAngle by hand, so
        // mirror VK_LEFT/VK_RIGHT's own keyup handling just above and
        // clear the suppression here -- the settle timer this key's own
        // keydown handler already started picks Probe 1 back up on its
        // very next tick.
        if ((wParam == 'E' || wParam == 'Q') && appMode == APP_MODE_SIMULATION && app.robotScene.activeKind == ROBOT_KIND_ROCKY)
        {
            rockyKneeSettleSuppressed = FALSE;
        }
        return 0;
    }
    case WM_SETCURSOR:
    {
        // Middle-mouse pan (WM_MBUTTONDOWN/UP, further down) works
        // identically in both Design and Simulation mode -- `panning` is
        // set the instant the middle button goes down and cleared the
        // instant it comes back up, so it's an exact mirror of "is the
        // middle button currently held" with nothing mode-specific about
        // it. Show the hand cursor for as long as that's true, in every
        // mode, checked first so it always wins over whatever cursor the
        // mode-specific logic below would otherwise pick (e.g. hovering
        // the robot in Simulation mode while panning shouldn't still show
        // the move-robot cursor).
        if (panning && LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }

        // Simulation mode: dragging the robot's own body moves it (see
        // WM_LBUTTONDOWN/WM_MOUSEMOVE below) -- a 4-way move cursor
        // signals that specifically while hovering over the robot
        // (app.hoveringRobotSim, kept up to date every WM_MOUSEMOVE),
        // rather than implying anywhere on the canvas is draggable.
        // Outside Simulation mode, off the robot, or over the window's
        // border/caption/etc (hit-test isn't HTCLIENT), fall through to
        // the default arrow.
        if (appMode == APP_MODE_SIMULATION && LOWORD(lParam) == HTCLIENT && app.hoveringRobotSim)
        {
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        // Simulation mode: no new environment strokes -- a left-click
        // drag started INSIDE the robot's own body instead grabs and
        // moves it, so the user can set its starting position within the
        // scene. A click elsewhere on the canvas (off the robot) does
        // nothing, rather than falling through to stroke-drawing.
        if (appMode == APP_MODE_SIMULATION)
        {
            float wx, wy;
            screenToGL(hWnd, LOWORD(lParam), HIWORD(lParam), &wx, &wy);
            if (isPointInsideActiveRobotBody(wx, wy))
            {
                app.draggingRobotSim = TRUE;
                dragRobotLastWX = wx;
                dragRobotLastWY = wy;
                SetCapture(hWnd);
            }
            return 0;
        }

        // Robot layer is a blank placeholder for now (a separate project
        // will live here later) - no drawing while it's active.
        if (appMode == APP_MODE_DESIGN && designLayer == LAYER_ROBOT) return 0;
        if (canvas.strokeCount >= MAX_STROKES) return 0;
        drawing = TRUE;
        strokeStarts[canvas.strokeCount] = canvas.pointCount;
        strokeColor[canvas.strokeCount] = brushColor;
        strokeThickness[canvas.strokeCount] = thickness;
        strokeLayer[canvas.strokeCount] = designLayer;
        canvas.strokeCount++;

        float x = (float)LOWORD(lParam);
        float y = (float)HIWORD(lParam);
        float aspect = (float)glWindowWidth / (float)glWindowHeight;
        float nx, ny;
        if (aspect >= 1.0f) {
		    nx = ((2.0f * x / glWindowWidth) - 1.0f) * aspect * canvas.zoom;
		    ny = (1.0f - (2.0f * y / glWindowHeight)) * canvas.zoom;
		} else {
		    nx = ((2.0f * x / glWindowWidth) - 1.0f) * canvas.zoom;
		    ny = (1.0f - (2.0f * y / glWindowHeight)) * (1.0f / aspect) * canvas.zoom;
		}
		nx += canvas.panX;   // NEW
		ny += canvas.panY;   // NEW

		// NEW: if the cursor was hovering near an existing stroke's
		// endpoint, snap this new stroke's first point exactly onto it
		// instead of the raw cursor position - lets straight lines be
		// chained end-to-end into a connected polyline.
		if (snapEndpointAvailable)
		{
		    nx = snapEndpointX;
		    ny = snapEndpointY;
		    snapEndpointAvailable = FALSE;
		}

		if (canvas.pointCount < MAX_POINTS - 1) {
		    points[canvas.pointCount++] = nx;
		    points[canvas.pointCount++] = ny;
		}
        return 0;
    }
    case WM_LBUTTONUP:
        // Environment autosave: a stroke was just finished drawing (see
        // WM_LBUTTONDOWN -- `drawing` can only ever be TRUE here for an
        // Environment-layer stroke, since Simulation mode and the Robot
        // layer both return early there before it's ever set). Persist the
        // drawing right away so it's never more than one stroke stale if
        // the app is closed unexpectedly. Checked BEFORE `drawing` is
        // reset below, since that's what this check is testing.
        if (drawing)
        {
            saveEnvironmentAutosave();
        }
        drawing = FALSE;
        shiftHoldActive  = FALSE;   // NEW: end any in-progress dwell-snap tracking
        shiftHoldSnapped = FALSE;   // NEW
        if (app.draggingRobotSim)
        {
            app.draggingRobotSim = FALSE;

            // Auto gravity was silently paused for the duration of the drag
            // (see advanceAutoGravity's app.draggingRobotSim check) -- pick
            // back up from rest now that the robot's been let go, same as a
            // fresh Shift+G toggle-on or a landing, rather than resuming
            // with whatever velocity had built up before the drag started
            // (the robot may now be somewhere completely different) or
            // jumping by however long the drag itself lasted.
            if (autoGravityActive)
            {
                autoGravityVelocity = 0.0f;
                autoGravityLastTickTime = GetTickCount();
            }

            // Kick off a settle pass on release -- but ONLY if the robot is
            // already touching/overlapping the ground exactly where it was
            // just dropped, same mechanism as VK_LEFT/RIGHT's own WM_KEYUP
            // kickoff below (see advancePostRotateSettle's own comment for
            // the full reasoning). Without this at all, a plain drag-and-
            // drop with Auto Gravity OFF called applyGravityStep exactly
            // zero times, which meant advanceRockySettle (Probe 1/Probe 2,
            // Rocky's knee-bend and body-pivot) never ran even once -- the
            // robot just stayed wherever the mouse let go of it, floating
            // parts and all, with nothing left able to fix it short of
            // manually turning Auto Gravity on or pressing G. That's what a
            // real report showed: Rocky dropped in its home pose, resting
            // on the knee circle alone with the foot circle visibly hanging
            // in mid-air and not a single [SETTLE] line ever printed --
            // not a bug in either probe's own search, just neither one ever
            // getting a chance to run.
            //
            // The robotCollidesWithEnvironment() guard matters just as much
            // as the kickoff itself: this timer runs the EXACT same driver
            // Auto Gravity's own timer does (applyGravityStep every tick,
            // which falls the robot however far it takes to land) -- an
            // earlier version of this fix started it unconditionally on
            // every release, which meant releasing the robot ANYWHERE, even
            // high up in the air, made it fall all the way down on its own,
            // Auto Gravity OFF or not. That defeats the whole point of the
            // toggle. Only a pose that's ALREADY touching something needs
            // Probe 1/Probe 2 at all -- that's the entire "home position,
            // knee down but foot floating" scenario this was meant to fix
            // -- a robot released genuinely airborne is supposed to just
            // hang there until the user turns gravity on or presses G,
            // same as it always has.
            if (robotCollidesWithEnvironment())
            {
                // Re-arms the convergence flag and both step sizes first,
                // same reason the other two kickoffs do it: a robot that
                // had already fully settled before this drag would
                // otherwise leave rockySettleConverged still TRUE and the
                // driver would just no-op forever. rockyKneeSettleSuppressed
                // is cleared too -- nothing else is driving kneeAngle right
                // now, so Probe 1 is safe to run, same as VK_LEFT/RIGHT.
                // Harmless to start even when nothing actually needs to
                // move beyond this -- advancePostRotateSettle notices
                // there's nothing left to do and shuts its own timer back
                // off within a tick or two.
                rockySettleConverged = FALSE;
                rockyKneeSettleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
                rockyBodySettleStep = SIMULATION_BODY_SETTLE_STEP_DEG;
                rockyKneeSettleSuppressed = FALSE;

                postRotateSettleActive = TRUE;
                SetTimer(hWnd, AUTO_GRAVITY_TIMER_ID, SIMULATION_AUTO_GRAVITY_INTERVAL_MS, NULL);
            }

            ReleaseCapture();
        }
        return 0;
	case WM_MBUTTONDOWN:
	{
	    panning = TRUE;
	    panLastX = LOWORD(lParam);
	    panLastY = HIWORD(lParam);
	    SetCapture(hWnd);

	    // Set the hand cursor immediately on the down-click rather than
	    // only relying on WM_SETCURSOR's own panning check (further up
	    // this switch) -- WM_SETCURSOR for THIS same button-down event is
	    // actually dispatched by Windows BEFORE this handler runs (cursor
	    // messages precede the button message that triggers them), so at
	    // that point `panning` was still FALSE and it fell through to
	    // whatever cursor was already showing. Setting it here, right
	    // after panning actually becomes TRUE, means the cursor changes
	    // the instant the button goes down instead of only catching up
	    // once the mouse happens to move again.
	    SetCursor(LoadCursor(NULL, IDC_HAND));
	    return 0;
	}
	case WM_MBUTTONUP:
	{
	    panning = FALSE;
	    ReleaseCapture();
	    return 0;
	}
    case WM_MOUSEMOVE:
	{
	    // See advanceAutoGravity's comment: WM_TIMER is low-priority and
	    // gets starved by a steady stream of WM_MOUSEMOVE, which otherwise
	    // visibly pauses the fall for as long as the mouse keeps moving.
	    // Calling it here too means mouse movement itself keeps the fall
	    // going instead of blocking it -- no-ops instantly if auto gravity
	    // isn't currently on.
	    advanceAutoGravity(hWnd);

	    // Same starvation concern, for a rotate-release topple still in
	    // progress -- no-ops instantly once postRotateSettleActive is off.
	    advancePostRotateSettle(hWnd);

	    // Same reasoning, for the gait cycle -- no-ops instantly if Walk
	    // isn't currently on.
	    advanceGait(hWnd);

	    if (app.draggingRobotSim)
	    {
	        float wx, wy;
	        screenToGL(hWnd, LOWORD(lParam), HIWORD(lParam), &wx, &wy);

	        translateActiveRobot(wx - dragRobotLastWX, wy - dragRobotLastWY);

	        dragRobotLastWX = wx;
	        dragRobotLastWY = wy;

	        InvalidateRect(hWnd, NULL, FALSE);
	        return 0;
	    }

	    // Simulation mode, not currently dragging: keep app.hoveringRobotSim
	    // (used by WM_SETCURSOR/WM_LBUTTONDOWN above, and by renderRobot for
	    // the yellow hover highlight) up to date so the move cursor -- and
	    // the ability to start a drag -- only ever appears while genuinely
	    // over the robot's body.
	    if (appMode == APP_MODE_SIMULATION)
	    {
	        float wx, wy;
	        screenToGL(hWnd, LOWORD(lParam), HIWORD(lParam), &wx, &wy);
	        app.hoveringRobotSim = isPointInsideActiveRobotBody(wx, wy);

	        // Keep simHoveredJoint current too -- see its own comment and
	        // findHoveredJointSim's, used by WM_MOUSEWHEEL below to decide
	        // whether a Shift+scroll rotates the hovered joint instead of
	        // zooming. (WM_KEYDOWN's plain Left/Right is unrelated -- that
	        // rotates the WHOLE robot, not a specific joint, so it doesn't
	        // read simHoveredJoint at all.)
	        simHoveredJoint = findHoveredJointSim(app.robotScene.robot, wx, wy);

	        // Mirror it into the SAME hover fields Design > Robot mode's own
	        // hip/knee handles use (app.hoverHip/hoverKnee) -- renderer.c's
	        // drawSemniBody/drawThigh/drawShin already color the thigh/shin
	        // from these whenever Shift is ALSO held (RenderState.shiftHeld,
	        // sampled live every frame in renderRobot -- same field, same
	        // gating Design mode itself uses), so this is all that's needed
	        // to get the exact same "which limb is about to rotate"
	        // highlight Design mode shows, for free.
	        app.hoverHip  = (simHoveredJoint == 2);
	        app.hoverKnee = (simHoveredJoint == 3);

	        InvalidateRect(hWnd, NULL, FALSE);
	    }

	    if (panning)
	    {
	        // Same reasoning as WM_MBUTTONDOWN's own SetCursor call --
	        // don't rely solely on WM_SETCURSOR to keep the hand cursor
	        // showing for the rest of the drag (a captured middle-button
	        // drag doesn't reliably keep re-triggering WM_SETCURSOR on
	        // this window the way an uncaptured hover does), just
	        // explicitly re-assert it on every move that's actually
	        // panning. Cheap enough to call unconditionally here (Windows
	        // no-ops a SetCursor call for the cursor that's already
	        // active) rather than trying to track whether it's already
	        // been set this drag.
	        SetCursor(LoadCursor(NULL, IDC_HAND));

	        int mx = LOWORD(lParam);
	        int my = HIWORD(lParam);
	        int dx = mx - panLastX;
	        int dy = my - panLastY;

	        // Simulation mode: pan through sim_camera (shared with the
	        // robot) instead of canvas.panX/panY -- same reasoning as
	        // WM_MOUSEWHEEL above.
	        if (appMode == APP_MODE_SIMULATION)
	        {
	            simCameraPan(dx, dy);
	        }
	        else
	        {
	            float aspect = (float)glWindowWidth / (float)glWindowHeight;
	            float worldPerPixelX, worldPerPixelY;

	            if (aspect >= 1.0f) {
	                worldPerPixelX = (2.0f * aspect * canvas.zoom) / glWindowWidth;
	                worldPerPixelY = (2.0f * canvas.zoom) / glWindowHeight;
	            } else {
	                worldPerPixelX = (2.0f * canvas.zoom) / glWindowWidth;
	                worldPerPixelY = (2.0f * (canvas.zoom / aspect)) / glWindowHeight;
	            }

	            canvas.panX -= dx * worldPerPixelX;
	            canvas.panY += dy * worldPerPixelY;
	        }

	        panLastX = mx;
	        panLastY = my;

	        InvalidateRect(hWnd, NULL, FALSE);
	        return 0;
	    }

	    // Simulation mode never draws new strokes (WM_LBUTTONDOWN above
	    // returns early for it unconditionally), so the segment-hover/
	    // endpoint-snap preview below has nothing to actually feed into --
	    // and it computes world coords from canvas.zoom/panX/panY directly,
	    // which no longer match what's on screen once sim_camera is driving
	    // the view (see UpdateProjection). Skip it outright rather than let
	    // it show a highlight in the wrong place.
	    if (appMode == APP_MODE_SIMULATION)
	    {
	        if (hoveredSegment != -1 || snapEndpointAvailable)
	        {
	            hoveredSegment = -1;
	            snapEndpointAvailable = FALSE;
	            InvalidateRect(hWnd, NULL, FALSE);
	        }
	        // Must return here, not fall through -- the stroke-continuation
	        // code below this whole if/else-if assumes drawing == TRUE
	        // (indexes strokeStarts[canvas.strokeCount - 1] unconditionally),
	        // which is never true in Simulation mode but would still be
	        // unsafe to reach with strokeCount == 0.
	        return 0;
	    }
	    else if (!drawing || !(wParam & MK_LBUTTON))
	    {
	        // NEW: hover detection (segment overlay + stroke endpoint
	        // snapping) when not actively drawing or panning. World coords
	        // computed once here so both checks below can share them.
	        float hx = (float)LOWORD(lParam);
	        float hy = (float)HIWORD(lParam);
	        float hAspect = (float)glWindowWidth / (float)glWindowHeight;
	        float hwx, hwy;
	        if (hAspect >= 1.0f) {
	            hwx = ((2.0f * hx / glWindowWidth) - 1.0f) * hAspect * canvas.zoom;
	            hwy = (1.0f - (2.0f * hy / glWindowHeight)) * canvas.zoom;
	        } else {
	            hwx = ((2.0f * hx / glWindowWidth) - 1.0f) * canvas.zoom;
	            hwy = (1.0f - (2.0f * hy / glWindowHeight)) * (1.0f / hAspect) * canvas.zoom;
	        }
	        hwx += canvas.panX;
	        hwy += canvas.panY;

	        // NEW: does hovering land near an existing stroke's start/end
	        // point? If so, the next stroke drawn will snap to it (chains
	        // straight lines into a connected polyline).
	        float newSnapX, newSnapY;
	        BOOL newSnapAvailable = findNearestStrokeEndpoint(hwx, hwy, &newSnapX, &newSnapY);
	        if (newSnapAvailable != snapEndpointAvailable ||
	            (newSnapAvailable && (newSnapX != snapEndpointX || newSnapY != snapEndpointY)))
	        {
	            snapEndpointAvailable = newSnapAvailable;
	            snapEndpointX = newSnapX;
	            snapEndpointY = newSnapY;
	            InvalidateRect(hWnd, NULL, FALSE);
	        }

	        // Needed to actually receive WM_MOUSELEAVE below (used to clear
	        // both the segment hover and the endpoint-snap highlight) -
	        // registered unconditionally now since endpoint snapping isn't
	        // gated on canvas.showSegments the way segment hover is.
	        TRACKMOUSEEVENT tme = {0};
	        tme.cbSize = sizeof(tme);
	        tme.dwFlags = TME_LEAVE;
	        tme.hwndTrack = hWnd;
	        TrackMouseEvent(&tme);

	        if (canvas.showSegments && canvas.segmentResultCount > 0)
	        {
	            int newHover = findHoveredSegment(hwx, hwy);
	            if (newHover != hoveredSegment)
	            {
	                hoveredSegment = newHover;
	                InvalidateRect(hWnd, NULL, FALSE);
	            }
	        }
	        else if (hoveredSegment != -1)
	        {
	            hoveredSegment = -1;
	            InvalidateRect(hWnd, NULL, FALSE);
	        }

	        return 0;
	    }

	    // Shared with WM_KEYDOWN's VK_SHIFT handling below (see its comment)
	    // -- both funnel through updateDrawingPoint so a fresh point/preview
	    // can never disagree about how it was computed depending on which
	    // message happened to trigger it.
	    updateDrawingPoint(hWnd, LOWORD(lParam), HIWORD(lParam),
	                       (wParam & MK_SHIFT) != 0, (wParam & MK_CONTROL) != 0);
	    return 0;
	}
	case WM_MOUSELEAVE:
	{
	    if (app.hoveringRobotSim)
	    {
	        app.hoveringRobotSim = FALSE;
	        InvalidateRect(hWnd, NULL, FALSE);
	    }
	    if (hoveredSegment != -1)
	    {
	        hoveredSegment = -1;
	        InvalidateRect(hWnd, NULL, FALSE);
	    }
	    if (snapEndpointAvailable)
	    {
	        snapEndpointAvailable = FALSE;
	        InvalidateRect(hWnd, NULL, FALSE);
	    }
	    if (simHoveredJoint != -1)
	    {
	        simHoveredJoint = -1;
	        app.hoverHip = FALSE;
	        app.hoverKnee = FALSE;
	        InvalidateRect(hWnd, NULL, FALSE);
	    }
	    return 0;
	}
	case WM_COMMAND:
	{
	    if (LOWORD(wParam) == ID_UPLOAD)
	    {
	        RunUploadPipeline();
	        SendMessage(hWndUI, WM_COMMAND, MAKEWPARAM(ID_VIEW_SEGMENTS, BN_CLICKED), 0);
	        if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
	    }
	    else if (LOWORD(wParam) == ID_SLOW_MOTION)
	    {
	        // BS_AUTOCHECKBOX already flipped its own check state before this
	        // notification fires, so read it back rather than tracking a
	        // separate bool -- the button IS the toggle state (same pattern
	        // ui.c's Trace/Comparison Mode buttons use).
	        BOOL nowChecked = (SendMessage(hSlowMotionBtn, BM_GETCHECK, 0, 0) == BST_CHECKED);
	        simTimeScale = nowChecked ? SIMULATION_SLOW_MOTION_SCALE : 1.0f;

	        // Clicking a button gives IT keyboard focus by default -- left
	        // there, G/Shift+G/Ctrl+Numpad0 (all handled in this window's own
	        // WM_KEYDOWN) would stop working after the first Slow Motion
	        // click, since WM_KEYDOWN would go to the button instead of here.
	        // Hand focus straight back to the main window so keyboard control
	        // keeps working immediately, with no extra click needed.
	        SetFocus(hWnd);
	    }
	    else if (LOWORD(wParam) == ID_WALK_TOGGLE)
	    {
	        // ToggleGait maintains its own authoritative gaitActive bool
	        // (needed since Shift+W, WM_KEYDOWN, drives this same toggle
	        // without ever touching the button) and re-syncs hWalkBtn's
	        // checkbox to match every time -- BS_AUTOCHECKBOX already
	        // flipped it once on this real click, but ToggleGait's own
	        // BM_SETCHECK just re-affirms the same state, so this is safe
	        // to call unconditionally rather than reading BM_GETCHECK back.
	        ToggleGait(hWnd);

	        // Same reasoning as ID_SLOW_MOTION's SetFocus just above --
	        // hand keyboard focus back so G/Shift+G/Shift+W/arrow keys etc.
	        // keep working immediately after the click.
	        SetFocus(hWnd);
	    }
	    else if (LOWORD(wParam) == ID_RESET_ROBOT)
	    {
	        // Wipe the console FIRST, unconditionally -- even if there's no
	        // snapshot to restore yet, Reset is the one button whose whole
	        // point is "start over", and a debug log still full of whatever
	        // the PREVIOUS run printed makes it hard to tell that anything
	        // just happened, or to find this run's own prints among the
	        // last one's leftovers.
	        ClearConsoleLog();

	        if (haveSimulationStartSnapshot)
	        {
	            app.robotScene = simulationStartSnapshot;

	            // Leftover motion state belongs to whatever pose the robot
	            // was just IN, not the one it's being reset back to --
	            // without clearing this, a robot reset mid-fall would snap
	            // back to its starting pose and then immediately keep
	            // falling at its old speed, which reads as the reset not
	            // having really happened.
	            autoGravityVelocity = 0.0f;

	            printf("[RESET] robot restored to its Simulation-start pose (kind=%d)\n", app.robotScene.activeKind);
	            InvalidateRect(hWnd, NULL, FALSE);
	        }

	        // Same reasoning as ID_SLOW_MOTION's SetFocus just above.
	        SetFocus(hWnd);
	    }
	    else if (LOWORD(wParam) == ID_LAYER_ROBOT || LOWORD(wParam) == ID_LAYER_ENVIRONMENT || LOWORD(wParam) == ID_MODE_SIMULATION)
	    {
	        // "Mode" is the second top-level popup (index 1, after "File");
	        // "Design Mode" is the first item within it (index 0) and is
	        // itself a submenu holding the two layer choices.
	        HMENU hMenuBar = GetMenu(hWnd);
	        HMENU hModeMenu = GetSubMenu(hMenuBar, 1);
	        HMENU hDesignMenu = GetSubMenu(hModeMenu, 0);

	        if (LOWORD(wParam) == ID_MODE_SIMULATION)
	        {
	            appMode = APP_MODE_SIMULATION;

	            // Snapshot whichever pose the robot is in RIGHT NOW, before
	            // anything below or afterward (gravity, E/Q, dragging...)
	            // gets a chance to move it -- this is what ID_RESET_ROBOT's
	            // "Reset" button below restores. Re-captured every time
	            // Simulation is (re-)entered, so leaving and coming back
	            // with the robot posed differently updates what "starting
	            // position" means for the NEXT reset, rather than forever
	            // remembering only the very first entry this session.
	            simulationStartSnapshot = app.robotScene;
	            haveSimulationStartSnapshot = TRUE;

	            // Design > Robot mode's own hover flags (app.hoverHead/
	            // hoverButt/hoverFoot/hoverHip/hoverKnee) are only ever
	            // updated by input.c's WM_MOUSEMOVE, which stops receiving
	            // messages the moment editor mode leaves EDITOR_MODE_SEMNI --
	            // so if the user was hovering a handle there right before
	            // switching to Simulation, a stale TRUE would otherwise keep
	            // that part highlighted blue here even with the cursor
	            // nowhere near it (drawSemniBody/drawThigh/drawShin read
	            // these fields unconditionally, regardless of editor mode).
	            // Clearing them here guarantees Simulation always starts
	            // with a clean, accurate hover state; WM_MOUSEMOVE/
	            // WM_MOUSELEAVE below keep hoverHip/hoverKnee current for
	            // the rest of the session (simHoveredJoint's own comment).
	            app.hoverHead = FALSE;
	            app.hoverButt = FALSE;
	            app.hoverFoot = FALSE;
	            app.hoverHip = FALSE;
	            app.hoverKnee = FALSE;
	            simHoveredJoint = -1;

	            // Belt-and-suspenders alongside the hover reset above: these
	            // normally always get cleared on WM_LBUTTONUP (input.c), but
	            // reset them here too on the off chance a drag ever got left
	            // stuck active (e.g. the button-up landed somewhere that
	            // didn't reach input.c) -- setColor's `active` parameter now
	            // takes priority over hoveringWhole's yellow (see its own
	            // comment), so a stale TRUE here would otherwise paint a
	            // spurious blue highlight throughout Simulation instead of
	            // being silently masked the way it used to be.
	            app.draggingSeamArc1 = 0;
	            app.draggingSeamArc2 = 0;
	            app.draggingInner = 0;
	            app.draggingKnee = 0;
	            app.draggingThigh1 = 0;
	            app.draggingThigh2 = 0;
	            app.draggingFoot = 0;
	            app.draggingShin1 = 0;
	            app.draggingShin2 = 0;
	            app.activeHandle = 0;

	            // Ground collision (robotCollidesWithEnvironment ->
	            // pointCollidesWithAnyEnvironmentStroke) tests against the
	            // arc-fitted RECONSTRUCTION (segmentPointsWorld) now, not the
	            // raw hand-drawn strokes -- see that function's comment for
	            // why. That data is only produced by tracing (View Segments/
	            // Comparison Mode both trigger it on demand, see ui.c), which
	            // goes stale the moment the user draws or edits another
	            // stroke afterward. Re-tracing right here, every
	            // time Simulation is entered, closes both gaps for free: it's
	            // always present and always current, without the user having
	            // to think about it. Cheap to do unconditionally -- environment
	            // strokes can't be added or edited while Simulation is active
	            // (WM_LBUTTONDOWN only grabs the robot in this mode), so this
	            // is the one moment per Simulation session where re-tracing
	            // actually needs to happen at all.
	            //
	            // Console noise, not useful noise: this automatic re-trace
	            // is never something the user asked for (unlike View
	            // Segments/Comparison Mode/Save, which stay verbose), so
	            // suppress its [DEBUG] Path Analysis/Arc Segmentation dumps
	            // specifically -- see gSuppressTraceDebugPrints' own comment.
	            gSuppressTraceDebugPrints = TRUE;
	            RunTracePipeline();
	            gSuppressTraceDebugPrints = FALSE;

	            // Kick off a settle pass -- but ONLY if the robot is already
	            // touching/overlapping the freshly-traced environment right
	            // now, exactly where it was left in Design mode. This has to
	            // come AFTER RunTracePipeline just above: robotCollidesWithEnvironment
	            // reads segmentPointsWorld, which tracing is what produces,
	            // so checking any earlier would be testing against stale (or
	            // entirely absent) ground data. Gating on actual contact
	            // matters just as much as the placement: this timer runs the
	            // exact same driver Auto Gravity's own timer does
	            // (applyGravityStep every tick), so starting it unconditionally
	            // here -- as an earlier version of this fix did -- made ANY
	            // entry into Simulation mode fall all the way to the ground on
	            // its own, even with Auto Gravity OFF, which defeats the whole
	            // point of that toggle. Only touching-right-now poses need
	            // Probe 1/Probe 2 at all (that's the entire "home position,
	            // knee down but foot floating" scenario this was meant to
	            // fix) -- a robot starting out genuinely airborne is supposed
	            // to just sit there until the user turns gravity on or
	            // presses G, same as it always has.
	            if (robotCollidesWithEnvironment())
	            {
	                rockySettleConverged = FALSE;
	                rockyKneeSettleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
	                rockyBodySettleStep = SIMULATION_BODY_SETTLE_STEP_DEG;
	                rockyKneeSettleSuppressed = FALSE;
	                postRotateSettleActive = TRUE;
	                SetTimer(hWnd, AUTO_GRAVITY_TIMER_ID, SIMULATION_AUTO_GRAVITY_INTERVAL_MS, NULL);
	            }

	            printf("[ENV] %d segment(s) traced:\n", canvas.segmentResultCount);
	            for (int s = 0; s < canvas.segmentResultCount; s++)
	            {
	                int start = segmentStarts[s];
	                int count = segmentCounts[s];
	                if (count < 2) continue;
	                float sax = segmentPointsWorld[start * 2];
	                float say = segmentPointsWorld[start * 2 + 1];
	                float sbx = segmentPointsWorld[(start + count - 1) * 2];
	                float sby = segmentPointsWorld[(start + count - 1) * 2 + 1];
	                printf("[ENV]   seg=%d start=(%.5f,%.5f) end=(%.5f,%.5f) thickness=%.5f\n",
	                       s, sax, say, sbx, sby, segmentThicknessWorld[s]);
	            }

	            // Since the environment strokes shown on screen aren't what
	            // collision actually tests against anymore, show the user
	            // what physics sees instead of what they drew -- switches
	            // the Environment view from the original strokes over to
	            // the reconstructed/arc-fitted drawing (same as manually
	            // checking "Comparison Mode"), so it's obvious what the
	            // robot is really going to land on.
	            SetComparisonModeUI(TRUE);

	            // Simulation is driven from the ArcSpline canvas's own
	            // WndProc (this file), not the Semni editor's -- make sure
	            // picking it also backs out of the Semni robot editor if
	            // that's currently active. renderCombinedFrame special-cases
	            // APP_MODE_SIMULATION to draw the environment AND the robot
	            // both at full opacity together (rather than dimming
	            // whichever side isn't "active"), and WM_LBUTTONDOWN/
	            // WM_MOUSEMOVE above switch left-drag from drawing new
	            // strokes to dragging the whole robot into a starting pose.
	            switchEditorMode(EDITOR_MODE_ARCSPLINE, &editorModeState);

	            // Only ONE of Robot (Semni) / Environment / Simulation
	            // should ever show as checked at a time. designLayer itself
	            // is left alone here on purpose -- switching back to Design
	            // Mode later should return to whichever layer was last
	            // active -- but the CHECKMARKS have to be cleared now, or
	            // whichever layer was checked before would still show
	            // checked alongside Simulation, making it look like two
	            // modes are active at once.
	            CheckMenuItem(hDesignMenu, ID_LAYER_ROBOT, MF_BYCOMMAND | MF_UNCHECKED);
	            CheckMenuItem(hDesignMenu, ID_LAYER_ENVIRONMENT, MF_BYCOMMAND | MF_UNCHECKED);
	        }
	        else
	        {
	            // Picking either layer both enters Design mode and selects
	            // which layer new strokes/edits go to.
	            appMode = APP_MODE_DESIGN;
	            designLayer = (LOWORD(wParam) == ID_LAYER_ROBOT) ? LAYER_ROBOT : LAYER_ENVIRONMENT;
	            CheckMenuItem(hDesignMenu, ID_LAYER_ROBOT, MF_BYCOMMAND | (designLayer == LAYER_ROBOT ? MF_CHECKED : MF_UNCHECKED));
	            CheckMenuItem(hDesignMenu, ID_LAYER_ENVIRONMENT, MF_BYCOMMAND | (designLayer == LAYER_ENVIRONMENT ? MF_CHECKED : MF_UNCHECKED));

	            // "Robot (Semni)" used to be a blank placeholder layer -- it's
	            // now the real entry point into the separate Semni robot
	            // editor, which takes over this same window. Picking
	            // Environment (back) returns to the ArcSpline canvas.
	            switchEditorMode(designLayer == LAYER_ROBOT ? EDITOR_MODE_SEMNI : EDITOR_MODE_ARCSPLINE, &editorModeState);
	        }

	        CheckMenuItem(hModeMenu, ID_MODE_SIMULATION, MF_BYCOMMAND | (appMode == APP_MODE_SIMULATION ? MF_CHECKED : MF_UNCHECKED));

	        // Slow Motion button: only makes sense in Simulation, so it
	        // shows/hides right alongside it. Leaving Simulation also resets
	        // simTimeScale/the checkbox back to normal speed rather than
	        // leaving Slow Motion silently armed for the next time Simulation
	        // is entered -- it's a temporary "watch this closely" aid, not a
	        // setting that should persist invisibly across sessions.
	        if (hSlowMotionBtn)
	        {
	            if (appMode == APP_MODE_SIMULATION)
	            {
	                ShowWindow(hSlowMotionBtn, SW_SHOW);
	            }
	            else
	            {
	                ShowWindow(hSlowMotionBtn, SW_HIDE);
	                SendMessage(hSlowMotionBtn, BM_SETCHECK, BST_UNCHECKED, 0);
	                simTimeScale = 1.0f;
	            }
	        }

	        // Same show/hide-on-mode-switch treatment for "Walk" -- also
	        // stops the gait cycle and kills its timer on the way out (same
	        // reasoning as autoGravityActive's own reset just below: doesn't
	        // make sense to keep animating once Simulation isn't active,
	        // and leaving it "on" would restart mid-cycle, at whatever pose
	        // gaitBaseline still holds from before, the next time Simulation
	        // is entered -- confusing rather than a useful persisted state).
	        if (hWalkBtn)
	        {
	            if (appMode == APP_MODE_SIMULATION)
	            {
	                ShowWindow(hWalkBtn, SW_SHOW);
	            }
	            else
	            {
	                ShowWindow(hWalkBtn, SW_HIDE);
	                SendMessage(hWalkBtn, BM_SETCHECK, BST_UNCHECKED, 0);
	                if (gaitActive)
	                {
	                    gaitActive = FALSE;
	                    KillTimer(hWnd, AUTO_GAIT_TIMER_ID);
	                }
	            }
	        }

	        // "Reset" -- same show/hide-on-mode-switch treatment. Nothing
	        // to un-check (it's a push button, not a toggle) and nothing to
	        // stop on the way out -- it doesn't drive a timer or any
	        // ongoing state of its own, it just acts once when clicked.
	        if (hResetBtn)
	        {
	            ShowWindow(hResetBtn, appMode == APP_MODE_SIMULATION ? SW_SHOW : SW_HIDE);
	        }

	        // The Environment-only panel (hWndUI) can't rely on WM_TIMER to
	        // fade itself out here -- see HideUIPanelImmediately's comment --
	        // so force it closed immediately whenever the mode we just
	        // switched TO isn't Design > Environment, in case it was left
	        // showing from before this switch.
	        if (!(appMode == APP_MODE_DESIGN && designLayer == LAYER_ENVIRONMENT))
	            HideUIPanelImmediately();

	        // Same reasoning as the panel hide above: Shift+G's auto-gravity
	        // (see WM_KEYDOWN/WM_TIMER) shouldn't keep running once
	        // Simulation isn't the active mode anymore -- WM_TIMER's own
	        // defensive check would eventually catch this too, but turning
	        // it off immediately here avoids even one extra stray tick.
	        if (appMode != APP_MODE_SIMULATION && autoGravityActive)
	        {
	            autoGravityActive = FALSE;
	            autoGravityVelocity = 0.0f;
	            KillTimer(hWnd, AUTO_GRAVITY_TIMER_ID);
	        }

	        // Don't let the toast linger into whatever mode we just switched
	        // to -- it only makes sense as feedback for a Shift+G press made
	        // while actually in Simulation.
	        if (appMode != APP_MODE_SIMULATION)
	            gravityToastStartTick = 0;

	        // Same reasoning, for Shift+W's "WALK ON/OFF" toast.
	        if (appMode != APP_MODE_SIMULATION)
	            gaitToastStartTick = 0;

	        if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
	    }
	    else if (LOWORD(wParam) == ID_SAVE)
	    {
	        // Single File > Save entry point for the whole app -- folded in
	        // from what used to be two separate things: this dialog-based
	        // ArcSpline-only save, and the Robot editor's own "Save" button
	        // (input.c's now-removed ID_SAVE_BUTTON). Used to only save
	        // whichever HALF of the app was currently showing (Robot editor
	        // vs Environment canvas), picked via editorModeState.currentMode
	        // -- but that meant Save while posing a robot silently skipped
	        // the environment (and vice versa), even though both are
	        // independent, already-drawn/posed things sitting in memory
	        // regardless of which one you're currently looking at. Now
	        // always does BOTH, every time, so one Save reliably captures
	        // the whole scene: the Environment canvas AND whichever robot
	        // kind is currently active (app.robotScene.activeKind) -- not
	        // "every robot kind", just the one actually being worked on,
	        // same as before.

	        // --- Environment: EnvExport\Env.bmp + Env.txt ---
	        //
	        // Env.txt's segments come from canvas.segmentResultCount,
	        // which is only produced by tracing (View Segments/Comparison
	        // Mode trigger it on demand, see ui.c) -- stale the moment
	        // another stroke is drawn afterward. Re-tracing right here
	        // closes both gaps for free -- same reasoning (and same
	        // RunTracePipeline call) the ID_MODE_SIMULATION handler above
	        // already uses so ground-collision data is always current on
	        // entry. Gated on CanvasHasTraceableContent() (pipeline.c) now,
	        // not unconditional any more -- a robot-only Save (nothing ever
	        // drawn on the Environment canvas) has nothing to trace, so
	        // skip the call entirely instead of running it just to print
	        // "Canvas is empty" to the console every single time.
	        if (CanvasHasTraceableContent())
	            RunTracePipeline();

	        CreateDirectoryA("EnvExport", NULL);

	        // If comparison mode is on and segments exist, save reconstructed drawing
	        if (canvas.comparisonMode && canvas.segmentResultCount > 0)
	        {
	            Image* img = (Image*)malloc(sizeof(Image));
	            if (img)
	            {
	                img->width = glWindowWidth;
	                img->height = glWindowHeight;
	                img->data = (uint8_t*)malloc((size_t)img->width * img->height * 3);
	                img->bin = NULL;

	                if (img->data)
	                {
	                    renderSegmentsToImage(img, segmentPointsWorld, segmentStarts, segmentCounts,
	                                         segmentAvgRadiusPx, canvas.segmentResultCount,
	                                         img->width, img->height);
	                    saveBMP_UI("EnvExport\\Env.bmp", img, NULL, BMP_RGB);
	                }

	                free(img->data);
	                free(img);
	            }
	        }
	        else
	        {
	            // Save original drawing. Guarded on the exact same
	            // pointCount/strokeCount check canvasToImage() itself uses
	            // (not CanvasHasTraceableContent() above -- a pending,
	            // not-yet-traced uploaded BMP still has no actual canvas
	            // stroke, so canvasToImage() would return NULL and print
	            // "Canvas is empty" anyway in that case). Save still always
	            // runs this branch even when the user only came here to
	            // save a robot and never drew an environment at all (a
	            // perfectly normal case, not an error), it just no longer
	            // calls into canvasToImage() at all in that case, so
	            // there's no "Canvas is empty" console print and no dialog.
	            Image* img = (canvas.pointCount >= 1 && canvas.strokeCount >= 1) ? canvasToImage() : NULL;
	            if (img)
	            {
	                saveBMP_UI("EnvExport\\Env.bmp", img, img->bin, BMP_RGB);
	                free(img->data);
	                free(img->bin);
	                free(img->radius); // canvasToImage's own exact-radius raster -- see bmp.h
	                free(img);
	            }
	        }

	        saveEnvironmentSegmentsAsTxt("EnvExport\\Env.txt");

	        // --- Robot: whichever kind is currently active ---
	        //
	        // Each kind saves into its own export folder so posing one
	        // robot never overwrites another's saved files. Exact same
	        // per-kind logic the old Robot editor Save button used.
	        switch (app.robotScene.activeKind)
	        {
	            case ROBOT_KIND_ROCKY:
	            {
	                CreateDirectoryA("RockyExport", NULL);

	                saveCanvasAsBMP("RockyExport\\rocky.bmp", app.hwndMain, &app);
	                // rocky.txt (saveRockyAsEquations) is no longer written
	                // here on every File > Save -- rocky.bmp alone is
	                // enough for this export, per explicit user request.
	                // saveRockyAsEquations itself is untouched and still
	                // used elsewhere (input.c's Standing/Home pose buttons,
	                // Poses\rocky_standing.txt / Poses\rocky_home.txt).

	                // Rob.txt/Arm.txt export (see save.c's
	                // saveRockyAsRobArm) -- bodyWeight/legWeight (the
	                // mass-center ratio) are already live in the struct
	                // (the ratio slider and on-canvas drag both write
	                // straight into it, no edit box to re-read anymore --
	                // see ID_WEIGHT_RATIO_SLIDER's own comment), so only
	                // the separate Weight edit box (the real mass actually
	                // written as Rob.txt's own weight value) needs a
	                // GetWindowText here, same as Stilo/Semni's own cases
	                // below already just trust the live struct for the
	                // ratio.
	                wchar_t weightBuf[64];
	                GetWindowText(app.ui.hActualWeightEdit, weightBuf, 64);
	                app.robotScene.rocky.actualWeight = (float)wcstod(weightBuf, NULL);

	                saveRockyAsRobArm(&app);
	                break;
	            }

	            case ROBOT_KIND_STILO:
	                CreateDirectoryA("StiloExport", NULL);
	                saveCanvasAsBMP("StiloExport\\stilo.bmp", app.hwndMain, &app);

	                // Rob.txt (torso, both hip joints) + Leg1.txt/Leg2.txt
	                // (see save.c's saveStiloAsRobLeg) -- same Rocky-style
	                // convention as saveRockyAsRobArm above, generalized to
	                // Stilo's arc-based torso and two independent legs.
	                // Supersedes the old stilo.txt KEY=value equations dump
	                // (saveStiloAsEquations), no longer written here.
	                saveStiloAsRobLeg(&app);
	                break;

	            case ROBOT_KIND_SEMNI:
	            default:
	                CreateDirectoryA("SemniExport", NULL);
	                saveCanvasAsBMP("SemniExport\\semni.bmp", app.hwndMain, &app);

	                // Rob.txt (torso) + Leg.txt (hip/knee/foot leg) -- see
	                // save.c's saveSemniAsRobLeg, same Rocky-style
	                // convention as saveRockyAsRobArm above, generalized to
	                // Semni's arc-based torso and two-stage leg. Supersedes
	                // the old semni.txt KEY=value equations dump
	                // (saveRobotAsEquations), no longer written here.
	                saveSemniAsRobLeg(&app);
	                break;
	        }

	        // Robot editor's own controls (e.g. the Body/Leg Weight edit
	        // boxes just read above) can be left with keyboard focus after
	        // this -- only relevant while that editor is actually showing.
	        if (editorModeState.currentMode == EDITOR_MODE_SEMNI)
	            SetFocus(app.hwndMain);
	    }
	    else if (LOWORD(wParam) == ID_HELP)
	    {
	        // Reference list of every keyboard/mouse control for posing and
	        // simulating the robot -- File > Controls Help... (main.c's
	        // buildMainMenu). A proper modal reference window (see
	        // help_dialog.c) instead of a single-string MessageBox: section
	        // headers, a two-column key/description layout, and an OK
	        // button, reads far more like a real "Keyboard Shortcuts" page.
	        // Replaces an earlier, now-removed floating on-screen hint that
	        // only ever showed one gesture (Shift + Scroll rotate) at a
	        // time; this covers all of them at once, reachable whenever
	        // needed instead of only while hovering the right joint. Scoped
	        // to Robot Editing + Simulation (posing/moving the ROBOT
	        // itself), not the Environment canvas's own drawing controls,
	        // since that's what was actually asked for.
	        showControlsHelpDialog(hWnd);
	    }
	    else if (LOWORD(wParam) == ID_TOGGLE_HIDE_INACTIVE)
	    {
	        // View > Hide Inactive Layer -- independent on/off toggle, not
	        // part of the Design/Simulation mode-switching group above, so
	        // it gets its own small handler rather than folding into that
	        // one. The menu item IS the toggle state (flipped here, then
	        // the checkmark re-synced to match) same as the mode radio
	        // group's own CheckMenuItem calls just do it for one item
	        // instead of a group.
	        hideInactiveLayer = !hideInactiveLayer;

	        HMENU hMenuBar = GetMenu(hWnd);
	        HMENU hViewMenu = GetSubMenu(hMenuBar, 2);
	        CheckMenuItem(hViewMenu, ID_TOGGLE_HIDE_INACTIVE, MF_BYCOMMAND | (hideInactiveLayer ? MF_CHECKED : MF_UNCHECKED));

	        InvalidateRect(hWnd, NULL, FALSE);
	    }
	    return 0;
	}
	case WM_PAINT:
	{
	    // The actual drawing now lives in renderCombinedFrame (both
	    // subsystems, one dimmed) so it can also be driven directly by
	    // main.c's per-frame loop instead of only through Invalidate/
	    // UpdateWindow -- WM_PAINT just needs BeginPaint/EndPaint around it
	    // so an OS-triggered repaint (e.g. window restore) still clears the
	    // invalid region and shows the correct combined frame.
	    PAINTSTRUCT ps;
	    BeginPaint(hWnd, &ps);
	    renderCombinedFrame();
	    EndPaint(hWnd, &ps);
	    return 0;
	}
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: KillTimer(hWnd, UI_HOTZONE_TIMER_ID); KillTimer(hWnd, AUTO_GRAVITY_TIMER_ID); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
