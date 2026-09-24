#pragma once
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <gl/GL.h>
#include "geometry.h"

// Constants
#define MAX_POINTS 200000
#define MAX_STROKES 1000
#define ID_CLEAR  1
#define ID_COLOR  2
// 3 (ID_TRACE) is retired -- the standalone Trace button was removed since
// View Segments/Comparison Mode already trace on demand (see ui.c). Left
// unused rather than renumbering everything after it.
#define ID_UPLOAD 4
#define ID_VIEW_SEGMENTS 5
#define ID_SAVE   6
#define ID_COMPARISON 7
#define ID_MODE_SIMULATION  9
#define ID_LAYER_ROBOT       10
#define ID_LAYER_ENVIRONMENT 11
#define ID_SLOW_MOTION       12
#define ID_WALK_TOGGLE       13
#define ID_HELP              14
#define ID_TOGGLE_HIDE_INACTIVE 15
#define ID_RESET_ROBOT       16
#define ID_PROBE2_TOGGLE     17
#define ID_TOGGLE_SHOW_FPS   18
#define ID_TOGGLE_SIM_LOG    19
#define ID_TOGGLE_FPS_LOG    20
#define ID_UNDO              21
#define ID_ABOUT             22
#define ID_DEBUG_LOG_MENU    23   // View > Debug Log -- prints the current mode's debug info

// Author credit -- shown in Help > About RoboBuilder.
#define APP_AUTHOR_NAME L"Jesen Bryan"

// App-wide mode, switched from the "Mode" menu on the GL window.
// Design = draw/trace/edit arc-spline paths (current behavior).
// Simulation = shows the environment and the Semni robot together at full
// opacity (see canvas.c's renderCombinedFrame), robot joint handles
// hidden, and left-drag moves the whole robot (canvas.c's
// WM_LBUTTONDOWN/WM_MOUSEMOVE + robot.c's translateRobot) to set its
// starting position -- animating/running the pose itself is still to come.
typedef enum {
    APP_MODE_DESIGN = 0,
    APP_MODE_SIMULATION
} AppMode;

extern AppMode appMode;

// Simulation-only console logging, toggled from View > Simulation Log and
// View > FPS Log (ID_TOGGLE_SIM_LOG/ID_TOGGLE_FPS_LOG above; both menu
// items are greyed out outside Simulation mode). Both default to FALSE.
// simPhysicsLogEnabled gates every contact/gravity/settle/collision/reset/
// environment line ([CONTACT] [GRAVITY] [SETTLE] [COLLIDE] [RESET] [ENV]);
// simFpsLogEnabled gates the periodic [FPS] line and the [PERF] slow-frame
// warnings. Neither ever prints outside Simulation, even while toggled on
// -- the toggle state is kept, so switching back to Simulation resumes
// logging without having to turn it on again.
extern BOOL simPhysicsLogEnabled;
extern BOOL simFpsLogEnabled;

#include <stdio.h>
#define SIM_LOG(...)     do { if (appMode == APP_MODE_SIMULATION && simPhysicsLogEnabled) printf(__VA_ARGS__); } while (0)
#define SIM_FPS_LOG(...) do { if (appMode == APP_MODE_SIMULATION && simFpsLogEnabled) printf(__VA_ARGS__); } while (0)

// Sub-mode of APP_MODE_DESIGN, picked from the "Design Mode" submenu
// (Mode > Design Mode > Robot (semni) / Environment). Every stroke is
// tagged with the layer that was active when it was drawn (see
// strokeLayer[]) so the two can be edited and rendered independently.
typedef enum {
    LAYER_ROBOT = 0,
    LAYER_ENVIRONMENT
} DesignLayer;

extern DesignLayer designLayer;

// Whether renderCombinedFrame HIDES whichever Design-mode layer isn't
// currently active (opacity 0, fully invisible) instead of just dimming it
// (the normal INACTIVE_MODE_DIM_ALPHA fade) -- see canvas.c's own
// definition/comment. Toggled from the View > Hide Inactive Layer menu item
// (ID_TOGGLE_HIDE_INACTIVE above). TRUE by default.
extern BOOL hideInactiveLayer;

// Whether canvasRenderFrame draws the real-time FPS counter, top-right,
// above the Probe 2 Settle button -- see canvas.c's own definition for the
// full comment. Toggled from the View > Show FPS menu item
// (ID_TOGGLE_SHOW_FPS above). TRUE by default.
extern BOOL showFpsOverlay;

// Canvas state that should always reset together (see ResetCanvas)
typedef struct {
    int   pointCount;
    int   strokeCount;
    BOOL  hasBackgroundImage;
    float panX;
    float panY;
    float zoom;
	BOOL  showSegments;         // NEW
    int   segmentResultCount;   // NEW
    BOOL  comparisonMode;       // NEW: show original strokes semi-transparent + bright arcs
} CanvasState;

extern CanvasState canvas;

void ResetCanvas(void);
// Removes the most recently drawn Environment stroke (Undo button in the
// Environment panel, and Ctrl+Z). No-op while a stroke is being drawn or
// when there's nothing to undo. Defined in canvas.c.
void UndoLastStroke(void);

// Global state shared across modules
extern HWND hWndGL;
extern HWND hWndUI;
extern int glWindowWidth;
extern int glWindowHeight;
extern float thickness;
extern COLORREF brushColor;

extern float points[MAX_POINTS];
extern int pointCount;
extern int strokeStarts[MAX_STROKES];
extern float strokeThickness[MAX_STROKES];
extern COLORREF strokeColor[MAX_STROKES];
extern DesignLayer strokeLayer[MAX_STROKES];  // which design layer each stroke belongs to
extern int strokeCount;
extern BOOL drawing;
extern GLuint fontBase;

// Background image shown behind strokes (set when a BMP is uploaded)
extern GLuint canvasTexture;

// Fixed world-space bounds of the background quad (computed once at
// upload time using zoom=1 as reference — do NOT recompute per-frame)
extern float bgLeft, bgRight, bgBottom, bgTop;

// Branch/junction points found while tracing (a Y/T/X-shaped stroke
// splits into multiple edges at these points) - drawn as their own
// marker.
#define MAX_BRANCH_MARKERS 64
extern float branchMarkersWorld[MAX_BRANCH_MARKERS * 2];
extern int   branchMarkerCount;

#define MAX_SEGMENT_POINTS 10000
extern float segmentPointsWorld[MAX_SEGMENT_POINTS * 2];
extern int   segmentStarts[MAX_ARC_SEGMENTS];
extern int   segmentCounts[MAX_ARC_SEGMENTS];

// NEW: the FULL circle each arc segment was cut from, in world space -
// drawn as a faint "ghost circle" outline when canvas.showSegments is on.
// radius <= 0 means "this segment is a straight line, no ghost circle".
extern float segmentCircleCenterWorld[MAX_ARC_SEGMENTS * 2];
extern float segmentCircleRadiusWorld[MAX_ARC_SEGMENTS];   // world X radius
extern float segmentCircleRadiusWorldY[MAX_ARC_SEGMENTS];  // world Y radius (differs from X
                                                             // when the background is stretched
                                                             // non-uniformly to fill the window)

// Each segment's recovered original stroke radius, in raw source-image
// pixels (same unit as strokeThickness[]) -- see canvas.c's definition for
// the full explanation.
extern float segmentAvgRadiusPx[MAX_ARC_SEGMENTS];

// Same radius, converted into world-space (EWS) units instead -- used as
// extra ground-collision tolerance in Simulation. See canvas.c's
// definition for the full explanation.
extern float segmentThicknessWorld[MAX_ARC_SEGMENTS];
