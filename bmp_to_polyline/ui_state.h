#pragma once
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <gl/GL.h>

// Constants
#define MAX_POINTS 200000
#define MAX_STROKES 1000
#define ID_CLEAR  1
#define ID_COLOR  2
#define ID_TRACE  3
#define ID_UPLOAD 4   

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

// Canvas state that should always reset together (see ResetCanvas)
typedef struct {
    int   pointCount;
    int   strokeCount;
    BOOL  hasBackgroundImage;
    BOOL  hasEndpointMarkers;
    float panX;
    float panY;
    float zoom;
} CanvasState;

extern CanvasState canvas;

void ResetCanvas(void);
