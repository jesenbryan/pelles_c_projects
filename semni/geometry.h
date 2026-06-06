// geometry.h
#ifndef GEOMETRY_H
#define GEOMETRY_H

typedef struct {
    float x;
    float y;
} Point;

// functions
Point rotatePoint(Point p, Point center, float angleDeg);
Point inverseRotate(Point p, Point center, float angleDeg);

Point circleEdge(Point center, float radius, float angleDeg);

int isNear(Point a, Point b, float radius);

#endif
