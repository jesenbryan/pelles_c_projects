#pragma once
#include "bmp.h"
#include "ui_state.h"
#include "geometry.h"

Image* canvasToImage(void);
void displayImageOnCanvas(Image* img);

// NEW: converts pixel-space (sx,sy)/(ex,ey) endpoints into world
// coordinates and stores them for WM_PAINT to draw.
// stretched = TRUE for an uploaded BMP (shown via a full-view texture quad),
// FALSE for a canvas drawing (1:1 with the window at capture time).
void setEndpointMarkers(int imgW, int imgH, int sx, int sy, int ex, int ey, BOOL stretched);
void setSegmentOverlay(ArcSegment* segments, int count, int imgW, int imgH, BOOL stretched);
