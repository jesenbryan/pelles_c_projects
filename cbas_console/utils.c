#include "extract.h"
#include <stdio.h>

//float
//void savePathCSV(const char* filename, Path path)
//{
    //FILE* f = fopen(filename, "w");
    //if (!f) return;

    //fprintf(f, "x,y\n");

    //for (int i = 0; i < path.count; i++)
    //{
        //fprintf(f, "%f,%f\n",
            //path.points[i].x,
            //path.points[i].y);
    //}

    //fclose(f);
    //printf("Saved path to %s\n", filename);
//}

//int
void savePathCSV(const char* filename, Path path)
{
    FILE* f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "x,y\n");

    for (int i = 0; i < path.count; i++)
    {
        fprintf(f, "%d,%d\n",
            path.points[i].x,
            path.points[i].y);
    }

    fclose(f);
    printf("Saved path to %s\n", filename);
}
