#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int x, y;
} Point;

unsigned char* loadBMP(const char* filename, int* width, int* height) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    unsigned char header[54];
    fread(header, sizeof(unsigned char), 54, f);

    *width  = *(int*)&header[18];
    *height = *(int*)&header[22];

    int padding = (4 - (*width * 3) % 4) % 4;

    unsigned char* data = (unsigned char*)malloc((*width) * (*height) * 3);

    for (int y = 0; y < *height; y++) {
        fread(data + (y * (*width) * 3), 3, *width, f);
        fseek(f, padding, SEEK_CUR);
    }

    fclose(f);
    return data;
}

unsigned char** toBinary(unsigned char* bmp, int w, int h) {
    unsigned char** img = malloc(h * sizeof(unsigned char*));

    for (int y = 0; y < h; y++) {
        img[y] = malloc(w);

        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 3;

            unsigned char b = bmp[i];
            unsigned char g = bmp[i + 1];
            unsigned char r = bmp[i + 2];

            // threshold (adjust if needed)
            int gray = (r + g + b) / 3;
			img[y][x] = (gray < 128) ? 1 : 0;
        }
    }

    return img;
}

int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
int dy[8] = {0, 1, 1,  1,  0, -1,-1,-1};

int findStart(unsigned char** img, int w, int h, Point* start) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (img[y][x]) {
                start->x = x;
                start->y = y;
                return 1;
            }
        }
    }
    return 0;
}

int traceCurve(unsigned char** img, int w, int h, Point* out, int maxPts) {
    Point cur, start;

    if (!findStart(img, w, h, &start))
        return 0;

    cur = start;
    int count = 0;
    int prevDir = 0;

    while (count < maxPts) {
        out[count++] = cur;

        int found = 0;

        for (int i = 0; i < 8; i++) {
            int dir = (prevDir + i) % 8;

            int nx = cur.x + dx[dir];
            int ny = cur.y + dy[dir];

            if (nx >= 0 && ny >= 0 && nx < w && ny < h) {
                if (img[ny][nx]) {
                    cur.x = nx;
                    cur.y = ny;
                    prevDir = (dir + 5) % 8;
                    found = 1;
                    break;
                }
            }
        }

        if (!found) break;

        // stop if loop closed
        if (cur.x == start.x && cur.y == start.y)
            break;
    }

    return count;
}

int main() {
    int w, h;

    unsigned char* bmp = loadBMP("curve.bmp", &w, &h);
    if (!bmp) return 1;

    unsigned char** img = toBinary(bmp, w, h);

	int cnt = 0;
	for (int y = 0; y < h; y++)
	    for (int x = 0; x < w; x++)
	        if (img[y][x]) cnt++;

	printf("curve pixels: %d\n", cnt);

    Point* path = malloc(w * h * sizeof(Point));

    int n = traceCurve(img, w, h, path, w * h);

    for (int i = 0; i < n; i++) {
        printf("%d %d\n", path[i].x, path[i].y);
    }

    return 0;
}
