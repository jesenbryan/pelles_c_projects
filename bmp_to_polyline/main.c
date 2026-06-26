#include <stdio.h>
#include <stdlib.h>
#include "bmp.h"
#include "extract.h"
#include "utils.h"

#define MAX_POINTS 100000

int main(int argc, char *argv[])
{
	printf("Console initialized!\n");

	Image* img = loadBMP("curve1.bmp");

	//saveBMP("load.bmp", img);

	img->bin = create_binary(img);

	//thinningZhangSuen(img->bin, img->width, img->height); // doesnt work when line is already 1 pixel wide, should do it auto in the future

	//save_binary_bmp("curve_bin.bmp", img->bin, img->width, img->height);

	Point* poly = malloc(sizeof(Point) * 100000);
	//int count;

	int sx, sy, ex, ey;
	find_start_end_pixels(img->bin, img->width, img->height, &sx, &sy, &ex, &ey);

	Point path[10000];

	int numPoints = tracePath(
	    img->bin,
	    img->width,
	    img->height,
	    sx,
	    sy,
	    ex,
	    ey,
	    path,
	    10000);

	printf("  X | Y\n");
	printf("---------\n");

	for (int i = 0; i < numPoints; i++)
	{
	    printf("%3d | %d\n", path[i].x, path[i].y);
	}

	printf("---------\n");
	printf("Number of points: %d\n", numPoints);


	savePathCSV("curve1.csv", path, numPoints);

    return 0;
}

