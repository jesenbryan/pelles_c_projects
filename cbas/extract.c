#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "extract.h"


// -------------------- USER MUST DEFINE --------------------
void markVisited(int* visited, int x, int y, int width);

// ----------------------------------------------------------

int isLinePixel(Image* img, int x, int y)
{
    unsigned char pixel = img->data[y * img->width + x];
	printf("pixel value = %d\n", pixel);
    // assume black line on white background
    return pixel < 128;
}

Point findStartPixel(Image* img)
{
    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            if (isLinePixel(img, x, y))
            {
				printf("Checking pixel (%d,%d)\n", x, y);
                return (Point){ (float)x, (float)y };
            }
        }
    }

    return (Point){ -1, -1 };
}


// 8-neighborhood
int dx[8] = {1,1,0,-1,-1,-1,0,1};
int dy[8] = {0,1,1,1,0,-1,-1,-1};


// check if valid pixel
int isValid(int x, int y, int w, int h)
{
    return (x >= 0 && x < w && y >= 0 && y < h);
}


// find next connected pixel
Point findNextNeighbor(Image* img, Point current, int* visited)
{
    int x = (int)current.x;
    int y = (int)current.y;

    for (int i = 0; i < 8; i++)
    {
        int nx = x + dx[i];
        int ny = y + dy[i];

        if (isValid(nx, ny, img->width, img->height) &&
            isLinePixel(img, nx, ny) &&
            !visited[ny * img->width + nx])
        {
            return (Point){ (float)nx, (float)ny };
        }
    }

    return (Point){ -1, -1 };
}


// ---------------- MAIN EXTRACTION ----------------
Path extractPath(Image* img)
{
	printf("ENTER extractPath\n");
	

    Path path;
    path.points = malloc(sizeof(Point) * 100000);
    path.count = 0;

    int* visited = calloc(img->width * img->height, sizeof(int));

    Point current = findStartPixel(img);
	
	printf("START PIXEL: %f %f\n", current.x, current.y);

	if (current.x < 0 || current.y < 0)
	{
	    printf("No valid start pixel found\n");
	    return path;
	}

    while (1)
    {
        int cx = (int)current.x;
        int cy = (int)current.y;

		if (cx < 0 || cy < 0 || cx >= img->width || cy >= img->height)
		{
		    printf("OUT OF BOUNDS: %d %d\n", cx, cy);
		    break;
		}

        // store point
        path.points[path.count++] = current;

        // mark visited
        visited[cy * img->width + cx] = 1;

        // find next
        Point next = findNextNeighbor(img, current, visited);

        if (next.x < 0)
            break;

        current = next;
    }

    free(visited);
    return path;
}

//float dist(Point a, Point b)
//{
    //float dx = a.x - b.x;
    //float dy = a.y - b.y;
    //return sqrtf(dx*dx + dy*dy);
//}


