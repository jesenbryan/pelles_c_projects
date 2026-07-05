#include "utils.h"

int savePathCSV(const char *filename, Point path[], int numPoints)
{
    char pathBuf[MAX_PATH];

    // If no filename → open save dialog
    if (filename == NULL || filename[0] == '\0')
    {
        if (!saveFileDialog(pathBuf, MAX_PATH, "csv", 2))
        {
            printf("Save cancelled\n");
            return 0;
        }
        filename = pathBuf;
    }

    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
        return 0;

    fprintf(fp, "x,y\n");

    for (int i = 0; i < numPoints; i++)
    {
        fprintf(fp, "%d,%d\n", path[i].x, path[i].y);
    }

    fclose(fp);
    return 1;
}
