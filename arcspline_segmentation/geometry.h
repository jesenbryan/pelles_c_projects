#pragma once

#include <math.h>

// Previously 64 - too tight for a long, genuinely straight line. Every
// failed circle-fit on a (near-)collinear run forces another bisection
// (see recursiveArcFit in geometry.c), and a long straight stroke can need
// well over 100 tiny leaf segments before settling - hitting the old cap
// mid-bisection silently dropped the rest of the stroke instead of just
// producing more (later merged, see buildSegments) segments.
#define MAX_ARC_SEGMENTS 256

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

int fitCircleToPoints(Point* pts, int n, Circle* out);   // NEW

int segmentArcs(Point* path, int n, double tolerance, ArcSegment* segments, int maxSegments);

int segmentPath(Point* path, int n, int* segmentId, double threshold);

int buildSegments(Point* path, int n, ArcSegment* out);
