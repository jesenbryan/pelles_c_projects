#pragma once

#include "geometry.h"
#include "bmp.h"

void renderSegmentsToBMP(
    const char* outPath,
    Image* img,
    ArcSegment* segments,
    int segCount
);

// segmentAvgRadiusPx: each segment's original stroke radius, in raw
// source-image pixels (same array canvas.c's ghost overlay uses -- see its
// declaration there for the full explanation of where it comes from). Used
// directly as this segment's stamped radius below, same convention
// canvasToImage (canvas_bridge.c) already uses for real strokes
// (strokeThickness[s]/2 as a raw pixel radius) -- so the reconstructed/
// saved image reads as the same weight as the drawing it was fit from.
void renderSegmentsToImage(Image* img, float* segmentPointsWorld, int* segmentStarts,
                           int* segmentCounts, float* segmentAvgRadiusPx,
                           int segmentResultCount, int imgW, int imgH);
