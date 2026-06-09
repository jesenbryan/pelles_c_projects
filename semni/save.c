#include <stdio.h>
#include "app.h"
#include "config.h"

void saveDrawing(const char* filename)
{
    FILE* f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "%d\n", app->paintCount);

    for (int i = 0; i < app->paintCount; i += 2)
    {
        float x = app->paintPoints[i];
        float y = app->paintPoints[i + 1];

        fprintf(f, "%f %f\n", x, y);
    }

    fclose(f);
}
