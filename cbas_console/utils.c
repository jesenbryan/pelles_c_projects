#include "extract.h"
#include <stdio.h>

void savePathCSV(const char* filename, Path path)
{
    FILE* f = fopen(filename, "w");
    if (!f)
    {
        printf("Failed to open file!\n");
        return;
    }

    fprintf(f, "x,y\n");

    for (int i = 0; i < path.count; i++)
    {
        fprintf(f, "%f,%f\n",
                path.points[i].x,
                path.points[i].y);
    }

    fclose(f);

    printf("CSV saved to: %s\n", filename);
}
