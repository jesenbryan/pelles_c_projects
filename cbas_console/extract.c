#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "extract.h"

typedef struct {
    int dx;
    int dy;
} Dir;

static Dir dirs[8] = {
    {1, 0},   // right
    {1, 1},
    {0, 1},
    {-1, 1},
    {-1, 0},
    {-1,-1},
    {0, -1},
    {1, -1}
};


// -------------------- USER MUST DEFINE --------------------
void markVisited(int* visited, int x, int y, int width);

// ----------------------------------------------------------

int isLinePixel(Image* img, int x, int y)
{
    int rowSize = ((img->width * 3 + 3) / 4) * 4;

    unsigned char* p = &img->data[y * rowSize + x * 3];

    unsigned char b = p[0];
    unsigned char g = p[1];
    unsigned char r = p[2];

    int gray = (r + g + b) / 3;

    return r < 80 && g < 80 && b < 80;
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
    int cx = (int)current.x;
    int cy = (int)current.y;

    for (int i = 0; i < 8; i++)
    {
        int nx = cx + dirs[i].dx;
        int ny = cy + dirs[i].dy;

        if (nx < 0 || ny < 0 || nx >= img->width || ny >= img->height)
            continue;

        int idx = ny * img->width + nx;

        if (visited[idx])
            continue;

        if (isLinePixel(img, nx, ny))
        {
            Point p = { (float)nx, (float)ny };
            return p;
        }
    }

    Point invalid = { -1, -1 };
    return invalid;
}


// ---------------- MAIN EXTRACTION ----------------
Path extractPath(Image* img)
{
	printf("ENTER extractPath\n");
	

    Path path;
    path.points = malloc(sizeof(Point) * 100000);
    path.count = 0;

	printf("WIDTH=%d HEIGHT=%d\n", img->width, img->height);
	fflush(stdout);

    int* visited = calloc(img->width * img->height, sizeof(int));

    Point current = findStartPixel(img);
	
	printf("START PIXEL: %f %f\n", current.x, current.y);

	if (current.x < 0 || current.y < 0)
	{
	    printf("No valid start pixel found\n");
	    return path;
	}

	printf("BEFORE LOOP\n");
	fflush(stdout);

    while (1)
    {
        int cx = (int)current.x;
        int cy = (int)current.y;

		if (cx < 0 || cy < 0 || cx >= img->width || cy >= img->height)
		{
		    printf("OUT OF BOUNDS: %d %d\n", cx, cy);
		    break;
		}

		printf("CX CY: %d %d\n", cx, cy);
		fflush(stdout);

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


