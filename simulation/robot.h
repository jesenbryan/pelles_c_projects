#ifndef ROBOT_H
#define ROBOT_H

#include "geometry.h"

typedef struct {
    float leftX, rightX;
    float y;

    float leftRadius;
    float rightRadius;

    Point topCtrl;
    Point bottomCtrl;

    Point innerCircle;
    float innerRadius;

    float angle;
} Semni;

int loadRobot(const char* filename, Semni* r);
void drawRobot(Semni* r);

#endif
