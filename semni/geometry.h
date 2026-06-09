// geometry.h
#ifndef GEOMETRY_H
#define GEOMETRY_H

typedef struct {
    float x;
    float y;
} Point;

Point rotatePoint(Point p, Point center, float angleDeg);
Point inverseRotate(Point p, Point center, float angleDeg);

Point circleEdge(Point center, float radius, float angleDeg);

int isNear(Point a, Point b, float radius);

void buildBezierSegment2(RobotSegment* seg, Point p0, Point p1, Point p2);

#endif
