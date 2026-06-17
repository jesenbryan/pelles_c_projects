#pragma once

typedef enum {
    SEG_LINE,
    SEG_ARC
} SegmentType;

typedef struct {
    SegmentType type;
    Point* pts;
    int count;
} Segment;
