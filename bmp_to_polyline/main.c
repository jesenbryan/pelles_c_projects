#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "bmp_ui.h"
#include "bmp.h"

#include "geometry.h"
#include "binary.h"
#include "thinning.h"
#include "endpoints.h"
#include "path_trace.h"

#include "utils.h"

#define MAX_POINTS 100000

int main(int argc, char *argv[])
{
	printf("Console initialized!\n");

	Image* img = loadBMP_UI(""); //"curve1.bmp"

	//saveBMP("load.bmp", img);

	img->bin = img_to_bin(img);

	//saveBMP_UI("", img, img->bin, BMP_BIN);

	//thinningZhangSuen(img->bin, img->width, img->height); // doesnt work when line is already 1 pixel wide, should do it auto in the future

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


	//savePathCSV("", path, numPoints);

    return 0;
}

