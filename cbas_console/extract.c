#include <stdio.h>
#include <stdlib.h>
#include "extract.h"

int cmp_x(const void* a, const void* b)
{
    Point* p1 = (Point*)a;
    Point* p2 = (Point*)b;
    return (p1->x - p2->x);
}

int cmp_y(const void* a, const void* b)
{
    Point* p1 = (Point*)a;
    Point* p2 = (Point*)b;
    return (p1->y - p2->y);
}

int isLinePixel(Image* img, int x, int y)
{
    int fy = img->height - 1 - y;

    unsigned char* p = &img->data[(fy * img->width + x) * 3];

    int gray = (p[0] + p[1] + p[2]) / 3;

    return gray < 80;
}

Path extractPath(Image* img)
{
    Path path;
    path.points = malloc(sizeof(Point) * img->width * img->height);
    path.count = 0;

    // 1. collect pixels
    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            if (isLinePixel(img, x, y))
            {
                path.points[path.count++] = (Point){x, y};
            }
        }
    }

    if (path.count == 0)
        return path;

    // 2. detect orientation
    int minx = path.points[0].x;
    int maxx = path.points[0].x;
    int miny = path.points[0].y;
    int maxy = path.points[0].y;

    for (int i = 1; i < path.count; i++)
    {
        if (path.points[i].x < minx) minx = path.points[i].x;
        if (path.points[i].x > maxx) maxx = path.points[i].x;
        if (path.points[i].y < miny) miny = path.points[i].y;
        if (path.points[i].y > maxy) maxy = path.points[i].y;
    }

    int dx = maxx - minx;
    int dy = maxy - miny;

    // 3. sort
    if (dx > dy)
        qsort(path.points, path.count, sizeof(Point), cmp_x); // horizontal
    else
        qsort(path.points, path.count, sizeof(Point), cmp_y); // vertical

    return path;
}
