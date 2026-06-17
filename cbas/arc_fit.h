#pragma once

typedef struct {
    float cx, cy;
    float r;
    float startAngle;
    float endAngle;
} Arc;

Arc fitArc(Point* pts, int n);
