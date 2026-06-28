#pragma once

#include <math.h>

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point* pts;
    int count;
    int capacity;
} Path;

// local

//typedef struct {
    //float x, y;
//} PointF;

typedef struct {
    float cx, cy, r;
} Circle;

typedef struct {
    Point* pts;
    int count;
    double avgCurvature;

	// local
	int start;
    int end;
    Circle circle;

} ArcSegment;

double curvature(Point a, Point b, Point c);

int segmentArcs(Point* path, int n, double tolerance, ArcSegment* segments, int maxSegments);

int segmentPath(Point* path, int n, int* segmentId, double threshold);

int buildSegments(Point* path, int n, ArcSegment* out);
