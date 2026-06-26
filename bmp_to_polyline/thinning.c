#include "thinning.h"

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
