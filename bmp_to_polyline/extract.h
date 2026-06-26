#pragma once

#include "bmp.h"

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point* pts;
    int count;
    int capacity;
} Path;

uint8_t* create_binary(Image* img);

//int find_any_curve_pixel(uint8_t* bin, int w, int h, int* sx, int* sy);

void thinningZhangSuen(unsigned char* img, int w, int h);

int find_start_end_pixels(uint8_t* bin, int w, int h, int* sx, int* sy, int* ex, int* ey);

int tracePath(
    uint8_t* bin,
    int width,
    int height,
    int sx,
    int sy,
    int ex,
    int ey,
    Point path[],
    int maxPoints);
