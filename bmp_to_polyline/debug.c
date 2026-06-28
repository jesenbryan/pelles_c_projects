#include <stdio.h>
#include "debug.h"

void debugPrintPath(Point* path, int numPoints)
{
    printf("  X | Y\n");
    printf("---------\n");

    for (int i = 0; i < numPoints; i++)
    {
        printf("%3d | %d\n", path[i].x, path[i].y);
    }

    printf("---------\n");
    printf("Number of points: %d\n", numPoints);
}
