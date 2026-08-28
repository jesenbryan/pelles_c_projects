#pragma once
#include "bmp.h"
#include "ui_state.h"
#include "geometry.h"

Image* canvasToImage(void);
void displayImageOnCanvas(Image* img);

void setSegmentOverlay(ArcSegment* segments, int count, int imgW, int imgH, BOOL stretched);

// NEW: converts one pixel-space branch/junction point into world
// coordinates and appends it to branchMarkersWorld for WM_PAINT to draw.
// No-op once MAX_BRANCH_MARKERS has been reached.
void addBranchMarker(int imgW, int imgH, int px, int py, BOOL stretched);
