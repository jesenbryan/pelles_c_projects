#pragma once

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point* pts;
    int count;
    int capacity;
} Path;
