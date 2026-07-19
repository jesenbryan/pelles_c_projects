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
#include <math.h>
#include <string.h>

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

void ResetCanvas(void)
{
    canvas.pointCount = 0;
    canvas.strokeCount = 0;
    canvas.hasBackgroundImage = FALSE;
    canvas.hasEndpointMarkers = FALSE;
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
}

GLuint canvasTexture = 0;

float bgLeft = -1.0f, bgRight = 1.0f, bgBottom = -1.0f, bgTop = 1.0f; // NEW

float branchMarkersWorld[MAX_BRANCH_MARKERS * 2];
int   branchMarkerCount = 0;

float markerStartX = 0.0f, markerStartY = 0.0f;
float markerEndX   = 0.0f, markerEndY   = 0.0f;

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
    float robotHalfY = 1.5f / robotZoom;
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
    float robotHalfY = 1.5f / robotZoom;

    float envZoom = 1.0f / simCameraGetZoom();
    float envHalfY = (aspect >= 1.0f) ? envZoom : (envZoom / aspect);

    return rlen * (envHalfY / robotHalfY);
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
// Tests against each segment's own segmentThicknessWorld on top of eRadius
// (the CALLER's own radius, e.g. the robot's body circle) -- segmentPointsWorld
// is only the fitted arc's bare mathematical CENTERLINE, but the
// reconstructed line is RENDERED with real thickness around that
// centerline (canvas.c's ghost overlay, segmentAvgRadiusPx). Without
// adding it in here too, the robot's contact point would track an
// invisible line instead of the visible surface it's actually approaching
// on screen -- sinking into it on one side, or stopping visibly short on
// the other, depending on the local angle of approach.
static BOOL pointCollidesWithAnyEnvironmentStroke(float ecx, float ecy, float eRadius)
{
    for (int s = 0; s < canvas.segmentResultCount; s++)
    {
        int start = segmentStarts[s];
        int count = segmentCounts[s];
        if (count < 2) continue;

        float combinedRadius = eRadius + segmentThicknessWorld[s];

        for (int i = 0; i < count - 1; i++)
        {
            float ax = segmentPointsWorld[(start + i) * 2];
            float ay = segmentPointsWorld[(start + i) * 2 + 1];
            float bx = segmentPointsWorld[(start + (i + 1)) * 2];
            float by = segmentPointsWorld[(start + (i + 1)) * 2 + 1];

            if (distPointToSegment(ecx, ecy, ax, ay, bx, by) < combinedRadius)
                return TRUE;
        }
    }

    return FALSE;
}

// TRUE if any of the robot's 5 body circles (head/butt/hip/knee/ankle --
// see computeSemniBodyCircles, renderer.h) OR its 6 connecting fillet arcs
// (seam1/2, thigh1/2, shin1/2 -- see computeSemniArcPoints) overlaps any
// Environment-layer stroke, in the robot's CURRENT pose/position. Used by
// the G ("gravity") hotkey below to stop the robot from sinking through
// whatever ground the user has drawn. The arcs are sampled as poly-lines
// (computeSemniArcPoints walks the exact same trimmed curve drawArc
// renders, not the full untrimmed fillet circle -- that would be far too
// generous) and tested with a fixed stand-in thickness, since -- unlike
// the circles -- they're drawn as bare curves with no radius of their own.
static BOOL robotCollidesWithEnvironment(Semni robot)
{
    CircleSegment bodyCircles[NUM_ROBOT_BODY_CIRCLES];
    computeSemniBodyCircles(robot, bodyCircles);

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
    computeSemniArcPoints(robot, arcPts, arcCounts);

    float eArcThickness = robotLengthToEnvWorld(SIMULATION_ARC_COLLISION_THICKNESS);

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

// Simulation mode only: which of the robot's ROTATABLE joints (hip = body
// circle index 2, knee = index 3 -- see computeSemniBodyCircles' documented
// ordering, renderer.h) the given world-space point falls inside, or -1 if
// neither. Head/butt/ankle are deliberately excluded -- they have no
// independent rotation angle of their own to spin (ankle's position is
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

// Slope response: scans the robot's 5 body circles against every fitted
// environment edge (same data pointCollidesWithAnyEnvironmentStroke reads)
// and returns the tangent angle (in Semni.angle's own convention -- see
// SIMULATION_SLOPE_ALIGN_RATE's comment in config.h) of whichever single
// edge is the closest match to an actual resting contact, i.e. the smallest
// (distance - combinedRadius) over every body-circle/edge pair. That's
// normally a small negative number (a hair of overlap) right after
// applyGravityStep's own binary search settles the robot onto something,
// which is exactly the edge we want the whole body to lean flush against.
//
// Only tested against the body circles, not the connecting fillet arcs
// (unlike robotCollidesWithEnvironment) -- the circles are where an actual
// standing/lying rest contact happens in practice, and skipping the arcs
// keeps this cheap enough to call every landed tick without a second
// thought. Returns FALSE (leaving *outAngleDeg untouched) if there's no
// environment to compare against at all (e.g. tracing never ran).
static BOOL findGroundContactAngleDeg(Semni robot, float* outAngleDeg)
{
    CircleSegment bodyCircles[NUM_ROBOT_BODY_CIRCLES];
    computeSemniBodyCircles(robot, bodyCircles);

    BOOL found = FALSE;
    float bestGap = 0.0f;
    float bestAx = 0.0f, bestAy = 0.0f, bestBx = 0.0f, bestBy = 0.0f;

    for (int c = 0; c < NUM_ROBOT_BODY_CIRCLES; c++)
    {
        float ecx, ecy;
        robotPointToEnvWorld(bodyCircles[c].center.x, bodyCircles[c].center.y, &ecx, &ecy);
        float eRadius = robotLengthToEnvWorld(bodyCircles[c].radius);

        for (int s = 0; s < canvas.segmentResultCount; s++)
        {
            int start = segmentStarts[s];
            int count = segmentCounts[s];
            if (count < 2) continue;

            float combinedRadius = eRadius + segmentThicknessWorld[s];

            for (int i = 0; i < count - 1; i++)
            {
                float ax = segmentPointsWorld[(start + i) * 2];
                float ay = segmentPointsWorld[(start + i) * 2 + 1];
                float bx = segmentPointsWorld[(start + (i + 1)) * 2];
                float by = segmentPointsWorld[(start + (i + 1)) * 2 + 1];

                float gap = distPointToSegment(ecx, ecy, ax, ay, bx, by) - combinedRadius;

                if (!found || gap < bestGap)
                {
                    found = TRUE;
                    bestGap = gap;
                    bestAx = ax; bestAy = ay; bestBx = bx; bestBy = by;
                }
            }
        }
    }

    if (!found) return FALSE;

    // robotPointToEnvWorld/robotLengthToEnvWorld map robot-world to
    // env-world via a uniform scale + pan only (see their own comments --
    // both axes always share the same scale factor), which preserves
    // angles exactly, so this env-space tangent angle IS the robot-space
    // angle already, no separate inverse transform needed.
    float angleDeg = atan2f(bestBy - bestAy, bestBx - bestAx) * (180.0f / 3.14159265f);

    // A line segment's tilt-from-horizontal only means anything mod 180
    // (it has no inherent direction) -- wrap into (-90, 90] so an edge
    // walked "backwards" (e.g. measuring ~178 degrees for a near-flat
    // edge) reads as the equivalent near-level ~-2 degrees, not a target
    // that reads as almost upside down.
    while (angleDeg > 90.0f)  angleDeg -= 180.0f;
    while (angleDeg <= -90.0f) angleDeg += 180.0f;

    *outAngleDeg = angleDeg;
    return TRUE;
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
    if (!robotCollidesWithEnvironment(app.robotScene.robot)) return;

    float safe = maxCorrection;    // largest offset assumed to clear
    float blocked = 0.0f;          // smallest offset confirmed NOT to clear

    for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
    {
        float mid = (safe + blocked) * 0.5f;

        translateRobot(&app.robotScene.robot, 0.0f, mid);
        BOOL hit = robotCollidesWithEnvironment(app.robotScene.robot);
        translateRobot(&app.robotScene.robot, 0.0f, -mid);

        if (hit) blocked = mid;
        else     safe = mid;
    }

    translateRobot(&app.robotScene.robot, 0.0f, safe);
    InvalidateRect(hWnd, NULL, FALSE);
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
static BOOL applyGravityStep(HWND hWnd, float step)
{
    translateRobot(&app.robotScene.robot, 0.0f, -step);

    BOOL landed = robotCollidesWithEnvironment(app.robotScene.robot);
    if (landed)
    {
        // Back out to the last known-safe position (before this step), then
        // binary-search the largest downward offset within [0, step] that
        // doesn't collide, converging on the true contact point instead of
        // leaving a whole-step-sized gap above it.
        translateRobot(&app.robotScene.robot, 0.0f, step);

        float safe = 0.0f;      // largest offset confirmed NOT to collide
        float blocked = step;   // smallest offset confirmed TO collide

        for (int i = 0; i < GRAVITY_CONTACT_SEARCH_ITERATIONS; i++)
        {
            float mid = (safe + blocked) * 0.5f;

            translateRobot(&app.robotScene.robot, 0.0f, -mid);
            BOOL hit = robotCollidesWithEnvironment(app.robotScene.robot);
            translateRobot(&app.robotScene.robot, 0.0f, mid); // undo probe

            if (hit) blocked = mid;
            else     safe = mid;
        }

        translateRobot(&app.robotScene.robot, 0.0f, -safe);

        // Slope response: lean the whole body a little further toward
        // whatever edge it just settled onto (see config.h's
        // SIMULATION_SLOPE_ALIGN_RATE comment for why this is a rigid
        // whole-body rotation, not per-joint IK). Runs every landed tick,
        // not just the first one that touches down, so a fall from a
        // steep angle settles into the slope gradually across a few ticks
        // instead of snapping straight to it.
        float targetAngleDeg;
        if (findGroundContactAngleDeg(app.robotScene.robot, &targetAngleDeg))
        {
            float diff = targetAngleDeg - app.robotScene.robot.angle;
            while (diff > 180.0f)  diff -= 360.0f;
            while (diff < -180.0f) diff += 360.0f;

            // Scaled by simTimeScale (Slow Motion, see its own comment
            // above) so the settling lean slows down right along with the
            // fall itself, instead of always snapping into its final tilt
            // at the same real-world pace regardless of how slow gravity
            // is currently running.
            float maxStepDeg = SIMULATION_SLOPE_ALIGN_MAX_STEP_DEG * simTimeScale;
            float step2 = diff * SIMULATION_SLOPE_ALIGN_RATE * simTimeScale;
            if (step2 > maxStepDeg)       step2 = maxStepDeg;
            else if (step2 < -maxStepDeg) step2 = -maxStepDeg;

            app.robotScene.robot.angle += step2;

            // Rotating around getCenter() can push a limb a hair into the
            // ground even though the body as a whole was already resting
            // on it -- re-settle immediately so the lean can never read as
            // sinking into the terrain.
            resolveUpwardIfPenetrating(hWnd, SIMULATION_SLOPE_CORRECTION_MAX);
        }
    }

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
// so the robot's own applyProjection can share it via a plain 1.5/zoom --
// see sim_camera.h). Inverting it here (1.0f / simCameraGetZoom(), 1.0
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

    autoGravityVelocity += SIMULATION_AUTO_GRAVITY_ACCEL_PER_MS2 * simElapsed;
    if (autoGravityVelocity > SIMULATION_AUTO_GRAVITY_MAX_VELOCITY_PER_MS)
        autoGravityVelocity = SIMULATION_AUTO_GRAVITY_MAX_VELOCITY_PER_MS;

    float step = autoGravityVelocity * simElapsed;

    if (applyGravityStep(hWnd, step))
        autoGravityVelocity = 0.0f; // landed -- next fall starts from rest
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

            float halfW = (strokeThickness[s] * canvas.zoom) / (float)glWindowWidth;

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

	    float ghostAlpha = isComparisonActive ? 0.95f : 0.35f;
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
	        float ghostHalfW = (segmentAvgRadiusPx[s] * canvas.zoom) / (float)glWindowWidth;
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

    // Endpoints are part of the trace overlay, so they follow the same
    // visibility toggle (Trace / View Segments) as the rest of it,
    // instead of staying on screen after the overlay is hidden.
    if (canvas.hasEndpointMarkers && canvas.showSegments)
    {
        float markerRadius = 0.02f * canvas.zoom;
        drawMarkerDisc(markerStartX, markerStartY, markerRadius, 1.0f, 0.0f, 0.0f, opacity);
        drawMarkerDisc(markerEndX,   markerEndY,   markerRadius, 0.0f, 0.0f, 1.0f, opacity);
    }

    // Branch/junction points (a Y/T/X-shaped stroke splits into multiple
    // edges here) - green, so they read as distinct from the red/blue
    // start-end pair above.
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

    float robotDimAmount  = (semniActive || simulationActive) ? 0.0f : INACTIVE_MODE_DIM_ALPHA;
    float canvasDimAmount = (!semniActive || simulationActive) ? 0.0f : INACTIVE_MODE_DIM_ALPHA;

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
        renderRobotScene(&app, robotDimAmount);
    }
    else
    {
        // Semni underneath...
        graphicsOnResize(glWindowWidth, glWindowHeight);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        renderRobotScene(&app, robotDimAmount);

        // ...ArcSpline on top
        canvasRenderFrame(canvasDimAmount);
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
        fontBase = glGenLists(96);
        wglUseFontBitmaps(hDC, 32, 96, fontBase);
        SelectObject(hDC, hOldFont);
        DeleteObject(hFont);

        UpdateProjection();
        glLoadIdentity();

        SetTimer(hWnd, UI_HOTZONE_TIMER_ID, UI_HOTZONE_INTERVAL_MS, NULL); // NEW

        // Slow Motion toggle -- see hSlowMotionBtn's own comment above.
        // Same BS_AUTOCHECKBOX | BS_PUSHLIKE "checkbox that looks/behaves
        // like a toggle button" style ui.c's Trace/Comparison Mode buttons
        // use. Created without WS_VISIBLE -- appMode starts in Design, not
        // Simulation, so it should start hidden; WM_COMMAND's
        // ID_MODE_SIMULATION handling shows/hides it from then on.
        hSlowMotionBtn = CreateWindowEx(0, L"BUTTON", L"Slow Motion",
                            WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                            10, 10, 120, 28, hWnd, (HMENU)ID_SLOW_MOTION,
                            GetModuleHandle(NULL), NULL);
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
            }
            else
            {
                autoGravityActive = FALSE;
                KillTimer(hWnd, AUTO_GRAVITY_TIMER_ID);
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

            BOOL inHotZone = PtInRect(&hotZone, pt);

            if (inHotZone != hotZoneHighlighted)
            {
                hotZoneHighlighted = inHotZone;   // NEW: drives the corner indicator's look
                InvalidateRect(hWnd, NULL, FALSE);
            }

            BOOL inUIWindow = FALSE;
            if (uiShown)
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
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

        // Simulation mode, hovering the hip or knee body circle: rotate
        // that joint instead of zooming -- see simHoveredJoint/
        // findHoveredJointSim's comments. Plain scroll (no modifier
        // needed) since hovering a joint already makes the intent
        // unambiguous; scrolling anywhere else on the canvas still zooms
        // exactly as before.
        if (appMode == APP_MODE_SIMULATION && simHoveredJoint != -1)
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
        if (appMode == APP_MODE_SIMULATION)
        {
            simCameraZoom(zDelta > 0 ? ZOOM_STEP : (1.0f / ZOOM_STEP));
        }
        else
        {
            if (zDelta > 0) canvas.zoom *= 0.9f;
            else            canvas.zoom *= 1.1f;
            if (canvas.zoom < 0.1f)  canvas.zoom = 0.1f;
            if (canvas.zoom > 10.0f) canvas.zoom = 10.0f;
        }

        UpdateProjection();
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
            applyGravityStep(hWnd, SIMULATION_GRAVITY_STEP);
            return 0;
        }

        return 0;
    }
    case WM_KEYUP:
    {
        // Mirror image of WM_KEYDOWN's VK_SHIFT branch above: releasing
        // Shift should restore the pre-Shift freehand drawing immediately,
        // not just whenever the next WM_MOUSEMOVE happens to arrive (which,
        // same as on the press side, Windows won't send just because a
        // modifier key's state changed). Pulling the cursor position here
        // and calling updateDrawingPoint with shiftHeld=FALSE runs the
        // exact restore-from-backup logic it already has for the release
        // case, right now instead of on the next mouse nudge.
        if (wParam == VK_SHIFT && drawing && appMode != APP_MODE_SIMULATION)
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            updateDrawingPoint(hWnd, pt.x, pt.y, FALSE, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            return 0;
        }
        return 0;
    }
    case WM_SETCURSOR:
    {
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
            if (isPointInsideRobotBody(app.robotScene.robot, wx, wy))
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

            ReleaseCapture();
        }
        return 0;
	case WM_MBUTTONDOWN:
	{
	    panning = TRUE;
	    panLastX = LOWORD(lParam);
	    panLastY = HIWORD(lParam);
	    SetCapture(hWnd);
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

	    if (app.draggingRobotSim)
	    {
	        float wx, wy;
	        screenToGL(hWnd, LOWORD(lParam), HIWORD(lParam), &wx, &wy);

	        translateRobot(&app.robotScene.robot, wx - dragRobotLastWX, wy - dragRobotLastWY);

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
	        app.hoveringRobotSim = isPointInsideRobotBody(app.robotScene.robot, wx, wy);

	        // Keep simHoveredJoint current too -- see its own comment and
	        // findHoveredJointSim's, used by WM_MOUSEWHEEL below to decide
	        // whether a scroll rotates the hovered joint instead of zooming.
	        simHoveredJoint = findHoveredJointSim(app.robotScene.robot, wx, wy);

	        InvalidateRect(hWnd, NULL, FALSE);
	    }

	    if (panning)
	    {
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

	            // Ground collision (robotCollidesWithEnvironment ->
	            // pointCollidesWithAnyEnvironmentStroke) tests against the
	            // arc-fitted RECONSTRUCTION (segmentPointsWorld) now, not the
	            // raw hand-drawn strokes -- see that function's comment for
	            // why. That data is normally only produced by a manual Trace
	            // press (the Environment-only panel's button), which the user
	            // could easily forget to do before simulating, and even if
	            // they didn't forget, it goes stale the moment they draw or
	            // edit another stroke afterward. Re-tracing right here, every
	            // time Simulation is entered, closes both gaps for free: it's
	            // always present and always current, without the user having
	            // to think about it. Cheap to do unconditionally -- environment
	            // strokes can't be added or edited while Simulation is active
	            // (WM_LBUTTONDOWN only grabs the robot in this mode), so this
	            // is the one moment per Simulation session where re-tracing
	            // actually needs to happen at all.
	            RunTracePipeline();

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

	        if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
	    }
	    else if (LOWORD(wParam) == ID_SAVE)
	    {
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
	                    saveBMP_UI("", img, NULL, BMP_RGB);
	                }
	                
	                free(img->data);
	                free(img);
	            }
	        }
	        else
	        {
	            // Save original drawing
	            Image* img = canvasToImage();
	            if (img)
	            {
	                saveBMP_UI("", img, img->bin, BMP_RGB);
	                free(img->data);
	                free(img->bin);
	                free(img);
	            }
	            else
	            {
	                MessageBox(hWnd, L"Canvas is empty. Draw something first.", L"Save Error", MB_OK | MB_ICONWARNING);
	            }
	        }
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
