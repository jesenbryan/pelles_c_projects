#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "extract.h"

//static const int dx[8] = {1,1,0,-1,-1,-1,0,1};
//static const int dy[8] = {0,1,1,1,0,-1,-1,-1};

uint8_t* create_binary(Image* img)
{
    int w = img->width;
    int h = img->height;

    uint8_t* bin = (uint8_t*)malloc(w * h * sizeof(uint8_t));

    if (!bin) return NULL;

    for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {

        int i = y * w + x;

        uint8_t r = img->data[i * 3 + 0];
        uint8_t g = img->data[i * 3 + 1];
        uint8_t b = img->data[i * 3 + 2];

        uint8_t gray = (uint8_t)(0.299*r + 0.587*g + 0.114*b);

        bin[i] = (gray < 128) ? 1 : 0;
    }
}

    return bin;
}

//int find_any_curve_pixel(uint8_t* bin, int w, int h, int* sx, int* sy)
//{
    //for (int y = 0; y < h; y++) {
        //for (int x = 0; x < w; x++) {

            //if (bin[y * w + x]) {
                //*sx = x;
                //*sy = y;
                //return 1;
            //}
        //}
    //}

    //return 0;
//}

static int hasForeground8(unsigned char* img, int x, int y, int w, int h) {
    if (x < 0 || y < 0 || x >= w || y >= h) return 0;
    return img[y * w + x];
}

static int countNeighbors(unsigned char* p, int x, int y, int w, int h) {
    int n = 0;
    n += hasForeground8(p, x-1, y, w, h);
    n += hasForeground8(p, x-1, y-1, w, h);
    n += hasForeground8(p, x, y-1, w, h);
    n += hasForeground8(p, x+1, y-1, w, h);
    n += hasForeground8(p, x+1, y, w, h);
    n += hasForeground8(p, x+1, y+1, w, h);
    n += hasForeground8(p, x, y+1, w, h);
    n += hasForeground8(p, x-1, y+1, w, h);
    return n;
}

static int transitions(unsigned char* p, int x, int y, int w, int h) {
    int A[8];
    A[0] = hasForeground8(p,x,y-1,w,h);
    A[1] = hasForeground8(p,x+1,y-1,w,h);
    A[2] = hasForeground8(p,x+1,y,w,h);
    A[3] = hasForeground8(p,x+1,y+1,w,h);
    A[4] = hasForeground8(p,x,y+1,w,h);
    A[5] = hasForeground8(p,x-1,y+1,w,h);
    A[6] = hasForeground8(p,x-1,y,w,h);
    A[7] = hasForeground8(p,x-1,y-1,w,h);

    int count = 0;
    for (int i = 0; i < 7; i++)
        if (A[i] == 0 && A[i+1] == 1)
            count++;
    if (A[7] == 0 && A[0] == 1)
        count++;

    return count;
}

void thinningZhangSuen(unsigned char* img, int w, int h) {
    int changed;
    unsigned char* marker = (unsigned char*)malloc(w * h);

    do {
        changed = 0;
        memset(marker, 0, w * h);

        // Step 1
        for (int y = 1; y < h - 1; y++) {
            for (int x = 1; x < w - 1; x++) {
                int p = y * w + x;
                if (!img[p]) continue;

                int n = countNeighbors(img, x, y, w, h);
                int t = transitions(img, x, y, w, h);

                int p2 = hasForeground8(img,x,y-1,w,h);
                int p4 = hasForeground8(img,x+1,y,w,h);
                int p6 = hasForeground8(img,x,y+1,w,h);
                int p8 = hasForeground8(img,x-1,y,w,h);

                if (n >= 2 && n <= 6 &&
                    t == 1 &&
                    p2 * p4 * p6 == 0 &&
                    p4 * p6 * p8 == 0)
                {
                    marker[p] = 1;
                    changed = 1;
                }
            }
        }

        for (int i = 0; i < w * h; i++)
            if (marker[i]) img[i] = 0;

        memset(marker, 0, w * h);

        // Step 2
        for (int y = 1; y < h - 1; y++) {
            for (int x = 1; x < w - 1; x++) {
                int p = y * w + x;
                if (!img[p]) continue;

                int n = countNeighbors(img, x, y, w, h);
                int t = transitions(img, x, y, w, h);

                int p2 = hasForeground8(img,x,y-1,w,h);
                int p4 = hasForeground8(img,x+1,y,w,h);
                int p6 = hasForeground8(img,x,y+1,w,h);
                int p8 = hasForeground8(img,x-1,y,w,h);

                if (n >= 2 && n <= 6 &&
                    t == 1 &&
                    p2 * p4 * p8 == 0 &&
                    p2 * p6 * p8 == 0)
                {
                    marker[p] = 1;
                    changed = 1;
                }
            }
        }

        for (int i = 0; i < w * h; i++)
            if (marker[i]) img[i] = 0;

    } while (changed);

    free(marker);
}

int find_start_end_pixels(uint8_t* bin, int w, int h, int* sx, int* sy, int* ex, int* ey)
{
    int count_found = 0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            if (!bin[y * w + x])
                continue;

            int N  = (y > 0)         && bin[(y - 1) * w + x];
            int S  = (y < h - 1)     && bin[(y + 1) * w + x];
            int W  = (x > 0)         && bin[y * w + (x - 1)];
            int E  = (x < w - 1)     && bin[y * w + (x + 1)];

            int NW = (y > 0 && x > 0)             && bin[(y - 1) * w + (x - 1)];
            int NE = (y > 0 && x < w - 1)         && bin[(y - 1) * w + (x + 1)];
            int SW = (y < h - 1 && x > 0)         && bin[(y + 1) * w + (x - 1)];
            int SE = (y < h - 1 && x < w - 1)     && bin[(y + 1) * w + (x + 1)];

            int count = N + S + W + E + NW + NE + SW + SE;

            int is_start = 0;

            if (count == 1) {
                is_start = 1;
            }

            if (count == 2) {
                if (
                    (E && NE) || (E && SE) ||
                    (W && NW) || (W && SW) ||
                    (N && NE) || (N && NW) ||
                    (S && SE) || (S && SW)
                ) {
                    is_start = 1;
                }
            }

            if (is_start) {

                if (count_found == 0) {
                    *sx = x;
                    *sy = y;
                }
                else if (count_found == 1) {
                    *ex = x;
                    *ey = y;
                }

                count_found++;
            }
        }
    }

    if (count_found > 2) {
        printf("WARNING: More than 2 start pixels detected (%d)\n", count_found);
    }

    if (count_found == 2) {

	    if (*sx > *ex) {
	        int tx = *sx;
	        int ty = *sy;

	        *sx = *ex;
	        *sy = *ey;

	        *ex = tx;
	        *ey = ty;
	    }

	    printf("Start: (%d, %d) (left)\n", *sx, *sy);
	    printf("End  : (%d, %d)\n", *ex, *ey);
	}
    else {
        printf("Start candidates found: %d\n", count_found);
    }

    return count_found;
}

int tracePath(
    uint8_t *bin,
    int width,
    int height,
    int sx,
    int sy,
    int ex,
    int ey,
    Point path[],
    int maxPoints)
{
    // N, E, S, W, NE, SE, SW, NW
    const int dx[8] = { 0, 1, 0,-1, 1, 1,-1,-1 };
    const int dy[8] = {-1, 0, 1, 0,-1, 1, 1,-1 };

    uint8_t *visited = calloc(width * height, sizeof(uint8_t));
    if (visited == NULL)
        return 0;

    int x = sx;
    int y = sy;
    int count = 0;

    while (1)
    {
        if (count >= maxPoints)
            break;

        path[count].x = x;
        path[count].y = y;
        count++;

        visited[y * width + x] = 1;

        if (x == ex && y == ey)
            break;

        int found = 0;

        for (int k = 0; k < 8; k++)
        {
            int nx = x + dx[k];
            int ny = y + dy[k];

            if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                continue;

            int idx = ny * width + nx;

            if (bin[idx] && !visited[idx])
            {
                x = nx;
                y = ny;
                found = 1;
                break;
            }
        }

        if (!found)
            break;
    }

    free(visited);

    return count;
}
