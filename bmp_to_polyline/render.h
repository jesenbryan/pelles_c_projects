#pragma once

#include "geometry.h"
#include "bmp.h"

void renderSegmentsToBMP(
    const char* outPath,
    Image* img,
    ArcSegment* segments,
    int segCount
);
