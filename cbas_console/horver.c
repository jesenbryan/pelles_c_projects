#include <stdio.h>
#include <stdlib.h>
#include "bmp.h"

#pragma pack(1)

typedef struct {
    unsigned short type;
    unsigned int size;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int offset;
} BMPHeader;

typedef struct {
    unsigned int size;
    int width;
    int height;
    unsigned short planes;
    unsigned short bits;
    unsigned int compression;
    unsigned int imagesize;
    int xres;
    int yres;
    unsigned int colors;
    unsigned int important;
} DIBHeader;

typedef struct {
    int x, y;
} Point;

// ---------- compare functions ----------
int cmp_x(const void *a, const void *b) {
    return ((Point*)a)->x - ((Point*)b)->x;
}

int cmp_y(const void *a, const void *b) {
    return ((Point*)a)->y - ((Point*)b)->y;
}

int main() {
    Image* img = loadBMP("horizontal_line.bmp");

	int width = img->width;
	int height = img->height;

	Point* pts = malloc(width * height * sizeof(Point));
	int count = 0;

	for (int y = 0; y < height; y++) {
	    int realY = height - 1 - y;

	    for (int x = 0; x < width; x++) {

	        int idx = (realY * width + x) * 3;

	        unsigned char b = img->data[idx];
	        unsigned char g = img->data[idx + 1];
	        unsigned char r = img->data[idx + 2];

	        int gray = (r + g + b) / 3;

	        if (gray < 200) {
	            if (count < width * height) {
	                pts[count].x = x;
	                pts[count].y = y;
	                count++;
	            }
	        }
	    }
	}

    if (count == 0) {
        printf("No line pixels found\n");
        return 1;
    }

    // -------- detect orientation --------
    int minx = pts[0].x, maxx = pts[0].x;
    int miny = pts[0].y, maxy = pts[0].y;

    for (int i = 1; i < count; i++) {
        if (pts[i].x < minx) minx = pts[i].x;
        if (pts[i].x > maxx) maxx = pts[i].x;
        if (pts[i].y < miny) miny = pts[i].y;
        if (pts[i].y > maxy) maxy = pts[i].y;
    }

    int dx = maxx - minx;
    int dy = maxy - miny;

    // -------- sort --------
    if (dx > dy) {
        // horizontal line → sort left to right
        qsort(pts, count, sizeof(Point), cmp_x);
        printf("Detected: horizontal line\n");
    } else {
        // vertical line → sort top to bottom
        qsort(pts, count, sizeof(Point), cmp_y);
        printf("Detected: vertical line\n");
    }

    // -------- output --------
    for (int i = 0; i < count; i++) {
        printf("%d %d\n", pts[i].x, pts[i].y);
    }

    free(pts);

	FILE *out = fopen("hor.csv", "w");
	if (!out) {
	    printf("Cannot create output.csv\n");
	    return 1;
	}

	fprintf(out, "x,y\n");

	for (int i = 0; i < count; i++) {
	    fprintf(out, "%d,%d\n", pts[i].x, pts[i].y);
	}

	fclose(out);

    return 0;
}
