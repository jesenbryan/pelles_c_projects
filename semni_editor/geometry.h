// geometry.h
#pragma once

#include <math.h>

// ============================================================================
// SHARED DATA STRUCTURES
// ============================================================================

typedef struct {
    float x;
    float y;
} Point;

typedef struct {
    float cx, cy, r;
} Circle;

typedef struct {
    Point center;
    float radius;
    int valid; // 0 if the 3 points were (nearly) collinear and no circle exists
} Circle_Semni;

// ============================================================================
// SEMNI ROBOT STRUCTURES
// ============================================================================

typedef struct {
    Point center;
    float radius;
} Fillet;

typedef struct {
    float centerDeg;    // angle (around c1) of the point on c1 farthest from c2
    float halfWidthDeg; // the safe range extends this far to either side
} SafeAngleRange;

// ============================================================================
// ARC-SPLINE STRUCTURES
// ============================================================================

#define MAX_ARC_SEGMENTS 256

typedef struct {
    Point* pts;
    int count;
    double avgCurvature;
    
    int start;
    int end;
    Circle circle;
} ArcSegment;

// ============================================================================
// SEMNI ROBOT GEOMETRY FUNCTIONS
// ============================================================================

// Rotation transformations
Point rotatePoint(Point p, Point center, float angleDeg);
Point inverseRotate(Point p, Point center, float angleDeg);

// Joint transformations
Point jointToWorld(Point local, Point pivot, float jointAngle, Point center, float bodyAngle);
Point nestedJointToWorld(Point local, Point innerPivot, float innerAngle, Point outerPivot, float outerAngle, Point center, float bodyAngle);

// Axis constraining
Point constrainToAxis(Point pivot, Point axisRef, Point target, float minLength);
float perpOffsetOnAxis(Point p, Point axisStart, Point axisEnd);
Point recenterOnAxis(Point p, Point axisStart, Point axisEnd);

// Circle operations
Point circleEdge(Point center, float radius, float angleDeg);
int isNear(Point a, Point b, float radius);
Circle_Semni circumcircle(Point p0, Point p1, Point p2);
Point circleTowardPoint(Point center, float radius, Point target);
Point internalTangentPoint(Point c1, float r1, Point c2, float r2);

// Fillet (tangent arc) operations
Fillet filletFromAttachAngle(Point c1, float r1, Point c2, float r2, float angleDeg, float minRadius, float maxRadius);
Fillet filletFromAttachAngleConcave(Point c1, float r1, Point c2, float r2, float angleDeg, float minRadius, float maxRadius);

// Safe angle ranges
SafeAngleRange filletSafeAngleRange(Point c1, float r1, Point c2, float r2, float maxRadius);
SafeAngleRange filletSafeAngleRangeConcave(Point c1, float r1, Point c2, float r2, float maxRadius);
float clampToSafeAngleRange(float angleDeg, SafeAngleRange range, float marginDeg);

// Fillet utilities
Point filletBulgePoint(Point c1, float r1, Point c2, float r2, float angleDeg, float minRadius, float maxRadius, Point target);
Point circleAtX(Point center, float radius, float targetX, Point preferNear);
Point circleAtAxisMid(Point center, float radius, Point axisStart, Point axisEnd, Point preferNear);

// ============================================================================
// ARC-SPLINE SEGMENTATION FUNCTIONS
// ============================================================================

// Curvature analysis
double curvature(Point a, Point b, Point c);

// Path resampling
int resamplePath(Point* in, int n, Point* out, double step);

// Segmentation
int segmentPath(Point* path, int n, int* segmentId, double threshold);
int segmentArcs(Point* path, int n, double tolerance, ArcSegment* segments, int maxSegments);

// Circle fitting
int fitCircleToPoints(Point* pts, int n, Circle* out);

// Arc segment building (primary entry point for arc-spline)
int buildSegments(Point* path, int n, ArcSegment* out);
