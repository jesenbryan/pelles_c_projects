#pragma once

#include "geometry.h"
#include "bmp.h"

void renderSegmentsToBMP(
    const char* outPath,
    Image* img,
    ArcSegment* segments,
    int segCount
);

void renderSegmentsToImage(Image* img, float* segmentPointsWorld, int* segmentStarts, 
                           int* segmentCounts, int segmentResultCount, int imgW, int imgH);
