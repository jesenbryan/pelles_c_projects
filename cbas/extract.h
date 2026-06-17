#pragma once

#include "bmp.h"

typedef struct {
    float x;
    float y;
} Point;

typedef struct {
    Point* points;
    int count;
} Path;

Path extractPath(Image* img);
