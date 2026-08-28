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

// Cap on distinct node clusters findRealJunctions() tracks per call -
// generously sized for how many endpoints/junctions a single component's
// edges could plausibly produce.
#define MAX_JUNCTION_NODES 256

typedef struct { int dx, dy; } Off2D;

static const Off2D NB[8] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1},
    {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
};

// Topological degree via crossing number, NOT a raw neighbor count. A
// sharp/acute turn in a thinned skeleton routinely leaves a "thick" 2px
// elbow - e.g. both E and SE foreground at the bend pixel - which a raw
// count misreads as 3 separate neighbors (a junction) even though E and
// SE are adjacent to EACH OTHER and really represent one continuing
// direction, not two branches. Walking the 8 neighbors in ring order and
// counting background->foreground transitions instead counts distinct
// connected neighbor GROUPS, which is the actual number of directions
// leaving this pixel - the standard technique for classifying skeleton
// pixels (endpoint/pass-through/junction) without corners falsely
// registering as nodes.
static int degreeAt(uint8_t* bin, int w, int h, int x, int y)
{
    int ring[8];
    for (int k = 0; k < 8; k++) {
        int nx = x + NB[k].dx;
        int ny = y + NB[k].dy;
        ring[k] = (nx >= 0 && nx < w && ny >= 0 && ny < h && bin[ny * w + nx]) ? 1 : 0;
    }

    int crossings = 0;
    for (int k = 0; k < 8; k++) {
        int next = (k + 1) % 8;
        if (ring[k] == 0 && ring[next] == 1) crossings++;
    }
    return crossings;
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

// Shared by findRealJunctions()/findRealEndpoints(): clusters every kept
// edge's two endpoint pixels into logical nodes (nearby hits within ~4px
// are treated as the same physical point) and counts how many edge-ends
// land in each cluster. A self-closing edge (closed loop) contributes
// nothing - its "start" and "end" are the same seam pixel, not a real
// node either endpoint- or junction-wise.
static int buildEdgeNodeTable(Point* const edgePaths[], const int edgeLengths[], int edgeCount,
                               int nodeX[], int nodeY[], int nodeHits[], int maxNodes)
{
    int nodeCount = 0;

    for (int e = 0; e < edgeCount; e++)
    {
        if (edgeLengths[e] < 1) continue;

        Point a = edgePaths[e][0];
        Point b = edgePaths[e][edgeLengths[e] - 1];

        if (a.x == b.x && a.y == b.y) continue;

        Point ends[2] = { a, b };
        for (int k = 0; k < 2; k++)
        {
            int found = -1;
            for (int i = 0; i < nodeCount; i++) {
                int dx = ends[k].x - nodeX[i];
                int dy = ends[k].y - nodeY[i];
                if (dx * dx + dy * dy <= 16) { found = i; break; }   // within ~4px - same node
            }
            if (found == -1 && nodeCount < maxNodes) {
                nodeX[nodeCount] = ends[k].x;
                nodeY[nodeCount] = ends[k].y;
                nodeHits[nodeCount] = 0;
                found = nodeCount++;
            }
            if (found != -1) nodeHits[found]++;
        }
    }

    return nodeCount;
}

int findRealJunctions(Point* const edgePaths[], const int edgeLengths[], int edgeCount,
                       int outX[], int outY[], int maxOut)
{
    if (edgeCount < 2) return 0;   // nothing can branch with only 0 or 1 edge

    int nodeX[MAX_JUNCTION_NODES], nodeY[MAX_JUNCTION_NODES], nodeHits[MAX_JUNCTION_NODES];
    int nodeCount = buildEdgeNodeTable(edgePaths, edgeLengths, edgeCount,
                                        nodeX, nodeY, nodeHits, MAX_JUNCTION_NODES);

    int written = 0;
    for (int i = 0; i < nodeCount && written < maxOut; i++)
    {
        // A node touched by only 1 edge-end is a plain endpoint. Exactly 2
        // can happen where a short spurious branch got filtered out
        // elsewhere, leaving what looks like a pass-through - still not a
        // real junction. 3 or more genuinely-kept edges meeting at one
        // point is the real thing.
        if (nodeHits[i] >= 3) {
            outX[written] = nodeX[i];
            outY[written] = nodeY[i];
            written++;
        }
    }

    return written;
}

int findRealEndpoints(Point* const edgePaths[], const int edgeLengths[], int edgeCount,
                       int outX[], int outY[], int maxOut)
{
    if (edgeCount < 1) return 0;

    int nodeX[MAX_JUNCTION_NODES], nodeY[MAX_JUNCTION_NODES], nodeHits[MAX_JUNCTION_NODES];
    int nodeCount = buildEdgeNodeTable(edgePaths, edgeLengths, edgeCount,
                                        nodeX, nodeY, nodeHits, MAX_JUNCTION_NODES);

    int written = 0;
    for (int i = 0; i < nodeCount && written < maxOut; i++)
    {
        // Exactly 1 edge touching this node - a real, single-sided end.
        if (nodeHits[i] == 1) {
            outX[written] = nodeX[i];
            outY[written] = nodeY[i];
            written++;
        }
    }

    return written;
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
        // No endpoints or junctions anywhere - normally means a genuine
        // closed loop, but can also happen for an OPEN curve whose real
        // tip pixels got misclassified as pass-through (a stray thinning
        // artifact right at the tip that isn't touching the tip's one
        // real neighbor reads as two separate neighbor groups instead of
        // one). Trace from the first foreground pixel found; if that walk
        // doesn't actually close back on its own start, it wasn't a loop
        // at all - the start pixel was really just a MID-POINT of an open
        // curve - so also walk the other direction from there and stitch
        // the two halves into one full tip-to-tip path instead of
        // silently keeping only the half already traced.
        for (int y = 0; y < h; y++) {
            int done = 0;
            for (int x = 0; x < w; x++) {
                if (!compBin[y * w + x]) continue;

                Point* buf = outPaths[edgeCount];
                int n = walkLoop(compBin, w, h, x, y, buf, maxPointsPerPath);

                int closesProperly = (n >= 3 && buf[0].x == buf[n - 1].x && buf[0].y == buf[n - 1].y);

                if (!closesProperly && n >= 1)
                {
                    uint8_t* seen = (uint8_t*)calloc((size_t)w * h, 1);
                    if (seen)
                    {
                        for (int i = 0; i < n; i++) seen[buf[i].y * w + buf[i].x] = 1;

                        // Look for an unvisited neighbor of the ORIGINAL
                        // start pixel - the other direction the first
                        // walk didn't explore.
                        int fx = -1, fy = -1;
                        for (int k = 0; k < 8; k++) {
                            int ox = x + NB[k].dx, oy = y + NB[k].dy;
                            if (ox < 0 || ox >= w || oy < 0 || oy >= h) continue;
                            if (!compBin[oy * w + ox]) continue;
                            if (seen[oy * w + ox]) continue;
                            fx = ox; fy = oy;
                            break;
                        }

                        if (fx != -1)
                        {
                            Point* other = (Point*)malloc(sizeof(Point) * (size_t)maxPointsPerPath);
                            Point* merged = other ? (Point*)malloc(sizeof(Point) * (size_t)maxPointsPerPath) : NULL;

                            if (other && merged)
                            {
                                int otherN = walkEdge(compBin, seen, w, h, x, y, fx, fy, other, maxPointsPerPath);

                                // Stitch: reversed `other` (skipping its
                                // own duplicate leading (x,y) at index 0)
                                // followed by the original walk.
                                int m = 0;
                                for (int i = otherN - 1; i >= 1 && m < maxPointsPerPath; i--)
                                    merged[m++] = other[i];
                                for (int i = 0; i < n && m < maxPointsPerPath; i++)
                                    merged[m++] = buf[i];

                                memcpy(buf, merged, sizeof(Point) * (size_t)m);
                                n = m;
                            }

                            free(other);
                            free(merged);
                        }

                        free(seen);
                    }
                }

                if (n >= 3) {
                    outLengths[edgeCount] = n;
                    edgeCount++;
                }
                done = 1;
                break;   // only one shape to find in this component
            }
            if (done) break;
        }
    }

    free(visitedEdge);
    return edgeCount;
}
