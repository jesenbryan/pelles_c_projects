#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "debug.h"
#include "bmp_ui.h"
#include "bmp.h"
#include "geometry.h"
#include "binary.h"
#include "thinning.h"
#include "endpoints.h"
#include "path_trace.h"
#include "utils.h"

#include "render.h"

#define MAX_POINTS 100000


int main(int argc, char *argv[])
{
	printf("Console initialized!\n");

	Image* img = loadBMP_UI("circl.bmp"); //"curve1.bmp"

	img->bin = img_to_bin(img);

	//saveBMP_UI("", img, img->bin, BMP_BIN);

	//thinningZhangSuen(img->bin, img->width, img->height); // doesnt work when line is already 1 pixel wide, should do it auto in the future

	int sx, sy, ex, ey;
	find_start_end_pixels(img->bin, img->width, img->height, &sx, &sy, &ex, &ey);

	Point path[10000];

	int numPoints = tracePath(img->bin, img->width, img->height, sx, sy, ex, ey, path, 10000);

	//debugPrintPath(path, numPoints);

	//savePathCSV("", path, numPoints);

	//printf("--------- CURVATURE ---------\n");

	//for (int i = 1; i < numPoints - 1; i++)
	//{
	    //double k = curvature(
	        //path[i - 1],
	        //path[i],
	        //path[i + 1]
	    //);

	    //printf("%d: k = %f\n", i, k);
	//}

	//ArcSegment segments[1000];

	//int segCount = segmentArcs(path, numPoints, 0.02, segments, 1000);

	//renderSegmentsToBMP("curve1_seg.bmp", img, segments, segCount);

	//printf("Segments: %d\n", segCount);

	//int segmentId[10000];

	//double threshold = 0.01; // you will tune this

	//int numSegments = segmentPath(path, numPoints, segmentId, threshold);

	//printf("Segments found: %d\n", numSegments);

	//for (int i = 0; i < numPoints; i++)
	    //{
	        //printf("%d %d seg=%d\n",
	            //(int)path[i].x,
	            //(int)path[i].y,
	            //segmentId[i]);
	    //}

	//renderSegmentsToBMP("curve1_seg.bmp", img, segments, segCount);

	int n = sizeof(path) / sizeof(path[0]);

	Point smooth[20000];

	int m = resamplePath(path, n, smooth, 1.0); // 1 pixel spacing

	printf("Resampled points: %d\n", m);

    for (int i = 10; i < m - 10; i++)
    {
        double kappa = curvature(smooth[i-10], smooth[i], smooth[i+10]);
        printf("%d %.6f\n", i, kappa);
    }

    //for (int i = 3; i < 100 && i < n - 1; i++)
	//{
	    //Point a = path[i - 2];
	    //Point b = path[i];
	    //Point c = path[i + 2];

	    //double kappa = curvature(a, b, c);

	    ////printf("i=%d | A(%.0f,%.0f) B(%.0f,%.0f) C(%.0f,%.0f) | curvature=%.6f\n",
	           ////i,
	           ////a.x, a.y,
	           ////b.x, b.y,
	           ////c.x, c.y,
	           ////kappa);

		//printf("i=%d | A(%d,%d) B(%d,%d) C(%d,%d) | k=%.6f\n",
		       //i,
		       //(int)a.x, (int)a.y,
		       //(int)b.x, (int)b.y,
		       //(int)c.x, (int)c.y,
		       //kappa);
	//}

    return 0;
}

