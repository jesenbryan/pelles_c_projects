#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "geometry.h"

typedef struct
{
    Point p1;
    Point p2;
} Wall;

#define MAX_WALLS 1000

extern Wall walls[MAX_WALLS];
extern int wallCount;

extern int drawingWall;
extern Point wallStart;
extern Point mouseGL;

void addWall(Point p1, Point p2);
void drawEnvironment(void);

#endif

