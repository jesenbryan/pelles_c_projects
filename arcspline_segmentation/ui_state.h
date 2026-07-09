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
#define ID_TRACE  3
#define ID_UPLOAD 4   
#define ID_VIEW_SEGMENTS 5
#define ID_SAVE   6
#define ID_COMPARISON 7

// Canvas state that should always reset together (see ResetCanvas)
typedef struct {
    int   pointCount;
    int   strokeCount;
    BOOL  hasBackgroundImage;
    BOOL  hasEndpointMarkers;
    float panX;
    float panY;
    float zoom;
	BOOL  showSegments;         // NEW
    int   segmentResultCount;   // NEW
    BOOL  comparisonMode;       // NEW: show original strokes semi-transparent + bright arcs
} CanvasState;

extern CanvasState canvas;

void ResetCanvas(void);

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
extern int strokeCount;
extern BOOL drawing;
extern GLuint fontBase;

// Background image shown behind strokes (set when a BMP is uploaded)
extern GLuint canvasTexture;

// Fixed world-space bounds of the background quad (computed once at
// upload time using zoom=1 as reference — do NOT recompute per-frame)
extern float bgLeft, bgRight, bgBottom, bgTop;

extern float markerStartX, markerStartY;
extern float markerEndX, markerEndY;

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
