#pragma once

#include "bmp.h"

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point* points;
    int count;
} Path;

Path extractPath(Image* img);
