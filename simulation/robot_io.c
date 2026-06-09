#include <stdio.h>
#include "robot.h"

int loadRobot(const char* filename, Semni* r)
{
    FILE* f = fopen(filename, "r");
    if (!f) return 0;

    // IMPORTANT: order must match your save function exactly

    fscanf(f, "%f %f %f",
        &r->leftX, &r->rightX, &r->y);

    fscanf(f, "%f %f",
        &r->leftRadius, &r->rightRadius);

    fscanf(f, "%f %f",
        &r->topCtrl.x, &r->topCtrl.y);

    fscanf(f, "%f %f",
        &r->bottomCtrl.x, &r->bottomCtrl.y);

    fscanf(f, "%f %f %f",
        &r->innerCircle.x, &r->innerCircle.y, &r->innerRadius);

    fscanf(f, "%f",
        &r->angle);

    fclose(f);
    return 1;
}
