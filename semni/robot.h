// robot.h
#ifndef ROBOT_H
#define ROBOT_H

#include "geometry.h"
#include "app.h"

typedef enum {
    SEG_BEZIER,
    SEG_ARC
} SegmentType;

typedef struct {
    SegmentType type;

    Point vertices[256];
    int vertexCount;

    Point position;
    float rotation;
} RobotSegment;

typedef struct {
    RobotSegment segments[4];

    int parent[4];   // connection tree
} RobotModel;

Point getCenter(Semni b);

#endif
