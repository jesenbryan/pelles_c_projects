#include "skeleton_graph.h"
#include <stdlib.h>
#include <string.h>

// dedup guard for direct node-to-node adjacencies (junction pixels that
// touch each other with no degree-2 chain in between) - without this each
// such pair would be found once from each side and traced twice
#define MAX_DEDUP_PAIRS 256

// A junction in the skeleton is rarely a single pixel - it's usually a
// small 2-4 pixel cluster where several degree>=3/degree==1 pixels all
// touch each other. Since nodes aren't clustered into one logical point
// here, every pair of touching node pixels within such a cluster surfaces
// as its own near-zero-length "edge" - on a test image with several
// circles linked by lines, that was ~145 junk 2-pixel edges alongside just
// ~23 real ones. Require a minimum length so only edges that actually
// travel somewhere get kept.
#define MIN_EDGE_POINTS 4

typedef struct { int dx, dy; } Off2D;

static const Off2D NB[8] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1},
    {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
};

static int degreeAt(uint8_t* bin, int w, int h, int x, int y)
{
    int d = 0;
    for (int k = 0; k < 8; k++) {
        int nx = x + NB[k].dx;
        int ny = y + NB[k].dy;
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
        if (bin[ny * w + nx]) d++;
    }
    return d;
}

// Walks one edge starting at node pixel (sx,sy), heading first into
// neighbor (fx,fy). Consumes degree-2 pixels along the way (marks
// visitedEdge[]) so the same edge isn't retraced later from its other end.
// Stops when it reaches another node pixel (degree != 2) - which may even
// be the SAME node it started from, if the edge is a loop hanging off one
// junction - or, defensively, a dead end.
static int walkEdge(uint8_t* bin, uint8_t* visitedEdge, int w, int h,
                     int sx, int sy, int fx, int fy,
                     Point* out, int maxPoints)
{
    int count = 0;
    out[count++] = (Point){ sx, sy };

    int prevX = sx, prevY = sy;
    int x = fx, y = fy;

    while (1) {
        if (count >= maxPoints) break;
        out[count++] = (Point){ x, y };

        int d = degreeAt(bin, w, h, x, y);
        if (d != 2) break;   // reached another node (or came back to this one) - edge complete

        visitedEdge[y * w + x] = 1;

        int nx = -1, ny = -1;
        for (int k = 0; k < 8; k++) {
            int cx = x + NB[k].dx;
            int cy = y + NB[k].dy;
            if (cx < 0 || cx >= w || cy < 0 || cy >= h) continue;
            if (!bin[cy * w + cx]) continue;
            if (cx == prevX && cy == prevY) continue;
            nx = cx; ny = cy;
            break;
        }
        if (nx == -1) break;   // dead end mid-chain - shouldn't happen, guard anyway

        prevX = x; prevY = y;
        x = nx; y = ny;
    }

    return count;
}

// Traces a pure closed loop (no node pixels anywhere in the component)
// starting at (sx,sy). Walks the ring all the way around and repeats the
// start pixel as the final point, so callers can detect closure by
// comparing the first/last point.
static int walkLoop(uint8_t* bin, int w, int h, int sx, int sy, Point* out, int maxPoints)
{
    uint8_t* visited = (uint8_t*)calloc((size_t)w * h, 1);
    if (!visited) return 0;

    int count = 0;
    out[count++] = (Point){ sx, sy };
    visited[sy * w + sx] = 1;

    int prevX = -1, prevY = -1;
    int x = sx, y = sy;

    while (count < maxPoints) {
        int nx = -1, ny = -1, closing = 0;

        for (int k = 0; k < 8; k++) {
            int cx = x + NB[k].dx;
            int cy = y + NB[k].dy;
            if (cx < 0 || cx >= w || cy < 0 || cy >= h) continue;
            if (!bin[cy * w + cx]) continue;
            if (cx == prevX && cy == prevY) continue;

            if (cx == sx && cy == sy && count > 2) { nx = cx; ny = cy; closing = 1; break; }
            if (!visited[cy * w + cx]) { nx = cx; ny = cy; break; }
        }

        if (nx == -1) break;   // dead end - not actually a clean loop, best effort

        prevX = x; prevY = y;
        x = nx; y = ny;
        out[count++] = (Point){ x, y };

        if (closing) break;
        visited[y * w + x] = 1;
    }

    free(visited);
    return count;
}

// NEW: Public function to trace a closed loop in a component.
// Returns the number of points in the traced loop (or 0 if no loop found).
// First and last point will be the same pixel, indicating closure.
int traceClosedLoop(uint8_t* compBin, int w, int h, Point* outPath, int maxPoints)
{
    // Find the first foreground pixel to start the loop trace
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (compBin[y * w + x]) {
                return walkLoop(compBin, w, h, x, y, outPath, maxPoints);
            }
        }
    }
    return 0;
}

int traceComponentEdges(uint8_t* compBin, int w, int h,
                         Point* outPaths[], int outLengths[],
                         int maxEdges, int maxPointsPerPath)
{
    uint8_t* visitedEdge = (uint8_t*)calloc((size_t)w * h, 1);
    if (!visitedEdge) return 0;

    int edgeCount = 0;
    int nodePixelsFound = 0;

    int pairAX[MAX_DEDUP_PAIRS], pairAY[MAX_DEDUP_PAIRS];
    int pairBX[MAX_DEDUP_PAIRS], pairBY[MAX_DEDUP_PAIRS];
    int pairCount = 0;

    for (int y = 0; y < h && edgeCount < maxEdges; y++) {
        for (int x = 0; x < w && edgeCount < maxEdges; x++) {
            if (!compBin[y * w + x]) continue;

            int d = degreeAt(compBin, w, h, x, y);
            if (d == 2) continue;   // plain edge pixel, handled when its edge is walked
            if (d == 0) continue;   // isolated noise speck

            nodePixelsFound++;

            for (int k = 0; k < 8 && edgeCount < maxEdges; k++) {
                int nx = x + NB[k].dx;
                int ny = y + NB[k].dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (!compBin[ny * w + nx]) continue;

                int nd = degreeAt(compBin, w, h, nx, ny);

                if (nd == 2) {
                    if (visitedEdge[ny * w + nx]) continue;   // already part of a traced edge
                } else {
                    // direct node-to-node touch - dedup against the reverse pair
                    int dup = 0;
                    for (int p = 0; p < pairCount; p++) {
                        if (pairAX[p] == nx && pairAY[p] == ny && pairBX[p] == x && pairBY[p] == y) { dup = 1; break; }
                    }
                    if (dup) continue;
                    if (pairCount < MAX_DEDUP_PAIRS) {
                        pairAX[pairCount] = x;  pairAY[pairCount] = y;
                        pairBX[pairCount] = nx; pairBY[pairCount] = ny;
                        pairCount++;
                    }
                }

                Point* buf = outPaths[edgeCount];
                int n = walkEdge(compBin, visitedEdge, w, h, x, y, nx, ny, buf, maxPointsPerPath);
                if (n >= MIN_EDGE_POINTS) {
                    outLengths[edgeCount] = n;
                    edgeCount++;
                }
            }
        }
    }

    if (nodePixelsFound == 0 && edgeCount < maxEdges) {
        // No endpoints or junctions anywhere - either one clean closed loop,
        // or a stray speck too small to have a well-defined degree. Trace
        // the ring starting from the first foreground pixel found.
        for (int y = 0; y < h; y++) {
            int done = 0;
            for (int x = 0; x < w; x++) {
                if (!compBin[y * w + x]) continue;

                Point* buf = outPaths[edgeCount];
                int n = walkLoop(compBin, w, h, x, y, buf, maxPointsPerPath);
                if (n >= 3) {
                    outLengths[edgeCount] = n;
                    edgeCount++;
                }
                done = 1;
                break;   // only one loop to find in this component
            }
            if (done) break;
        }
    }

    free(visitedEdge);
    return edgeCount;
}
