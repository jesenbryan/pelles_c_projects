#pragma once

#include <stdlib.h>
#include <stdint.h>
#include "geometry.h"

int tracePath(
    uint8_t *bin,
    int width,
    int height,
    int sx,
    int sy,
    int ex,
    int ey,
    Point path[],
    int maxPoints);
