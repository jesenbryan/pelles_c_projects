#include "pipeline.h"
#include "canvas_bridge.h"
#include "bmp_ui.h"
#include "binary.h"
#include "thinning.h"
#include "endpoints.h"
#include "path_trace.h"
#include "debug.h"
#include "bmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>   // NEW: sqrtf/floorf -- measureLocalRadiusPxDirectional's tangent handling

#include "ui_state.h"   // NEW: needed for BOOL
#include "geometry.h"   // NEW

// Holds a BMP that has been uploaded and displayed but not yet traced.
// Upload only shows the image now; tracing/segmenting is deferred until
// the user actually asks to view segments (see RunPendingUploadTrace).
static Image* s_pendingBmpImage = NULL;

static void freePendingBmpImage(void)
{
    if (!s_pendingBmpImage) return;
    free(s_pendingBmpImage->data);
    free(s_pendingBmpImage->bin);
    free(s_pendingBmpImage);
    s_pendingBmpImage = NULL;
}

// find_start_end_pixels/tracePath/buildSegments were all written assuming a
// SINGLE curve fills the whole bin buffer - with more than one freehand
// stroke (or an uploaded BMP with several curves in it, see 2curves.bmp),
// find_start_end_pixels just grabs the first two endpoint-looking pixels it
// scans past, which are almost always both on the first stroke, and
// tracePath then walks off the end of that stroke and stops - every other
// stroke is silently dropped.
//
// Fix: split the thinned bin buffer into its 8-connected components first
// (one per stroke/curve, since strokes only touch other strokes if the user
// actually draws them crossing/overlapping), then run the existing
// find/trace/build pipeline independently per component and merge the
// resulting ArcSegments into one array for setSegmentOverlay.
//
// NOTE: closed-loop (circle) and branching (Y/T/X junction) support were
// tried via skeleton_graph.c's traceComponentEdges/findRealJunctions/
// findRealEndpoints, but the edge cases around thinning-noise artifacts
// (false junctions, misplaced endpoints, half-traced curves) weren't
// reliable enough yet, so that path is disabled for now - find_start_end_
// pixels only recognizes a simple open curve with exactly two endpoints;
// closed loops and branching strokes are silently skipped, same as before
// that work started. skeleton_graph.c itself is left in place (compiled,
// unused) so this can be revisited later without redoing it.
#define MAX_TRACE_COMPONENTS 32

// Flood-fills the 8-connected component containing (startX, startY) out of
// `remaining` into `compBin` (must be pre-zeroed, same w*h size as
// `remaining`), clearing those pixels out of `remaining` so the caller's
// scan won't visit them again for a later component. Heap-based stack so it
// doesn't blow the call stack on a long, curly stroke.
static void extractComponent(uint8_t* remaining, uint8_t* compBin, int w, int h, int startX, int startY)
{
    int* stackX = (int*)malloc(sizeof(int) * (size_t)w * h);
    int* stackY = (int*)malloc(sizeof(int) * (size_t)w * h);
    int sp = 0;

    stackX[sp] = startX;
    stackY[sp] = startY;
    sp++;
    remaining[startY * w + startX] = 0;

    while (sp > 0)
    {
        sp--;
        int x = stackX[sp];
        int y = stackY[sp];
        compBin[y * w + x] = 1;

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (!remaining[ny * w + nx]) continue;

                remaining[ny * w + nx] = 0;
                stackX[sp] = nx;
                stackY[sp] = ny;
                sp++;
            }
        }
    }

    free(stackX);
    free(stackY);
}

// Approximates the local stroke half-width (radius, in source-image pixels)
// at skeleton point (x,y), by finding the smallest Chebyshev (square-ring)
// distance at which the ring around (x,y) first touches a background pixel
// or the image edge. thinningZhangSuen collapses a stroke down to its
// 1px-wide medial axis before this is ever called, and along that medial
// axis, distance-to-background approximates the ORIGINAL stroke's
// half-thickness at that point (standard property of a skeleton/medial
// axis) -- which is exactly why `bin` here has to be the PRESERVED
// pre-thinning raster (origBin below), not the thinned one (every point in
// the thinned image is background-adjacent almost immediately, since it's
// only 1px wide). Chebyshev rather than a true Euclidean distance transform
// -- cheaper, and the difference is negligible for the roughly-circular
// brush strokes this measures.
//
// Only used as a FALLBACK now (see measureLocalRadiusPxDirectional below)
// for the rare case a local tangent direction can't be worked out (a
// segment with just one point). A square ring measures distance the same
// in every direction, which is exactly its weakness for a stroke that
// ISN'T running along one of that ring's own axes: cutting straight down
// a DIAGONAL stroke this way crosses more of it than the stroke's true
// (perpendicular) width, overshooting the real radius by up to sqrt(2) --
// confirmed numerically (thickness_sim4.py) alongside the same-order
// quantization bias this has even at 0/90 degrees (below).
static float measureLocalRadiusPx(const uint8_t* bin, int w, int h, int x, int y, int maxR)
{
    for (int r = 1; r <= maxR; r++)
    {
        int yTop = y - r, yBot = y + r;
        for (int dx = -r; dx <= r; dx++)
        {
            int xTest = x + dx;
            if (xTest < 0 || xTest >= w) return (float)r;
            if (yTop < 0 || !bin[yTop * w + xTest]) return (float)r;
            if (yBot >= h || !bin[yBot * w + xTest]) return (float)r;
        }

        int xLeft = x - r, xRight = x + r;
        for (int dy = -r + 1; dy <= r - 1; dy++)
        {
            int yTest = y + dy;
            if (yTest < 0 || yTest >= h) return (float)r;
            if (xLeft < 0 || !bin[yTest * w + xLeft]) return (float)r;
            if (xRight >= w || !bin[yTest * w + xRight]) return (float)r;
        }
    }

    return (float)maxR;
}

// Same job as measureLocalRadiusPx above (approximating the local stroke
// half-width at skeleton point (x,y) from the preserved pre-thinning
// raster), but measured along the direction ACTUALLY PERPENDICULAR to the
// stroke's own local tangent (dirX, dirY -- a unit vector along the
// skeleton path at this point, see its caller below) instead of always
// along a fixed square ring. A cross-section cut anywhere off the true
// perpendicular measures MORE than the stroke's real width (you're
// slicing through it at a slant), which is exactly why the old ring-based
// version could overshoot a diagonal stroke's radius by up to sqrt(2)x --
// cutting exactly perpendicular, whatever the stroke's own angle happens
// to be, avoids that regardless of angle. Verified numerically
// (thickness_sim5.py): worst-case error across a full sweep of angles
// (0-90 degrees) and sub-pixel alignments dropped from consistently
// +-0.5px-or-worse down to +-0.5px in only the rarest cases and usually
// exact.
//
// Walks outward one integer pixel step at a time on EACH side of (x,y)
// along the perpendicular, counting how many steps stay inside the
// stroke before hitting background/the image edge, then converts that
// total run length back into a radius (run/2, +1 for the center pixel
// itself -- see the derivation in thickness_sim5.py).
static float measureLocalRadiusPxDirectional(const uint8_t* bin, int w, int h,
                                              int x, int y,
                                              float dirX, float dirY,
                                              int maxR)
{
    float pdx = -dirY;
    float pdy = dirX;

    int posSteps = 0;
    for (int t = 1; t <= maxR; t++)
    {
        int ix = (int)floorf((x + 0.5f) + pdx * (float)t);
        int iy = (int)floorf((y + 0.5f) + pdy * (float)t);
        if (ix < 0 || ix >= w || iy < 0 || iy >= h || !bin[iy * w + ix]) break;
        posSteps++;
    }

    int negSteps = 0;
    for (int t = 1; t <= maxR; t++)
    {
        int ix = (int)floorf((x + 0.5f) - pdx * (float)t);
        int iy = (int)floorf((y + 0.5f) - pdy * (float)t);
        if (ix < 0 || ix >= w || iy < 0 || iy >= h || !bin[iy * w + ix]) break;
        negSteps++;
    }

    return (float)(posSteps + negSteps + 1) * 0.5f;
}

// Generous enough for any thickness the brush slider actually allows, small
// enough that measureLocalRadiusPx's worst case (a huge solid blob with no
// nearby background at all) stays cheap.
#define MAX_MEASURED_STROKE_RADIUS_PX 40

static void runPipelineOnImage(Image* img, const char* sourceLabel, BOOL stretched)
{
    int w = img->width;
    int h = img->height;

    // Preserve the RAW (pre-thinning) binary raster -- canvasToImage/
    // img_to_bin stamp it with the actual drawn stroke width, but
    // thinningZhangSuen below immediately collapses every stroke down to a
    // 1px-wide skeleton (which the arc-fit needs), destroying that width
    // info in the process. Keeping a copy is what lets
    // measureLocalRadiusPx recover it afterward, per skeleton point.
    uint8_t* origBin = (uint8_t*)malloc((size_t)w * h);
    memcpy(origBin, img->bin, (size_t)w * h);

    thinningZhangSuen(img);

    uint8_t* remaining = (uint8_t*)malloc((size_t)w * h);
    memcpy(remaining, img->bin, (size_t)w * h);
    uint8_t* compBin = (uint8_t*)malloc((size_t)w * h);

    // ArcSegment.pts (set by buildSegments) points INTO the path buffer you
    // pass it, it doesn't copy - so each component's path buffer has to stay
    // alive until setSegmentOverlay below has consumed all the segments.
    Point* componentPaths[MAX_TRACE_COMPONENTS];
    int componentPathCount = 0;

    static ArcSegment allSegments[MAX_ARC_SEGMENTS];
    int totalSegCount = 0;

    for (int y = 0; y < h && totalSegCount < MAX_ARC_SEGMENTS && componentPathCount < MAX_TRACE_COMPONENTS; y++)
    {
        for (int x = 0; x < w && totalSegCount < MAX_ARC_SEGMENTS && componentPathCount < MAX_TRACE_COMPONENTS; x++)
        {
            if (!remaining[y * w + x]) continue;

            memset(compBin, 0, (size_t)w * h);
            extractComponent(remaining, compBin, w, h, x, y);

            int sx, sy, ex, ey;
            int found = find_start_end_pixels(compBin, w, h, &sx, &sy, &ex, &ey);
            if (found != 2) continue;   // closed loop or a noise speck - not an open curve, skip it

            Point* path = (Point*)malloc(sizeof(Point) * 10000);
            componentPaths[componentPathCount++] = path;

            int numPoints = tracePath(compBin, w, h, sx, sy, ex, ey, path, 10000);
            debugPrintPath(path, numPoints);

            ArcSegment segs[MAX_ARC_SEGMENTS];
            int segCount = buildSegments(path, numPoints, segs);

            // Recover ONE average stroke width for the WHOLE traced path
            // (not one independent measurement per arc-fit segment) from
            // the preserved pre-thinning raster, before the path buffer
            // gets copied off and this component's own buildSegments
            // locals go out of scope. The original stroke only ever has
            // ONE thickness for its entire length (strokeThickness[] is
            // set once per stroke at draw-start, see canvas.c's
            // WM_LBUTTONDOWN, never varied along it) -- measuring per
            // ARC SEGMENT instead let each piece's own subset of points
            // (sometimes just a handful, right at a buildSegments split)
            // pull its own estimate slightly away from its neighbors',
            // which showed up as a visible width "pinch" right at a
            // segment boundary -- invisible at a large overall thickness,
            // but a big fraction of the total width (and so very visible)
            // at the smallest 1px setting, even though the segments'
            // POSITIONS still joined up perfectly (that's a separate,
            // already-fixed guarantee -- see sampleArcPoints' shared-
            // endpoint snapping in canvas_bridge.c). Averaging over the
            // whole path first keeps every segment of the same stroke
            // rendered at the exact same, more representative width.
            double sum = 0.0;
            for (int k = 0; k < numPoints; k++)
            {
                // Local tangent direction at path[k], from its two
                // immediate neighbors (one-sided at either end of the
                // path) -- lets the width measurement below cut straight
                // across the stroke's TRUE perpendicular regardless of
                // which way it happens to run, instead of always
                // measuring along a fixed pair of axes (see
                // measureLocalRadiusPxDirectional's comment for why that
                // matters).
                int kPrev = (k > 0) ? k - 1 : k;
                int kNext = (k < numPoints - 1) ? k + 1 : k;

                float tdx = (float)(path[kNext].x - path[kPrev].x);
                float tdy = (float)(path[kNext].y - path[kPrev].y);
                float tlen = sqrtf(tdx * tdx + tdy * tdy);

                if (tlen > 1e-3f)
                {
                    sum += measureLocalRadiusPxDirectional(origBin, w, h,
                                                 path[k].x, path[k].y,
                                                 tdx / tlen, tdy / tlen,
                                                 MAX_MEASURED_STROKE_RADIUS_PX);
                }
                else
                {
                    // Degenerate (a single-point path, or repeated
                    // coordinates) -- no tangent to measure across, so
                    // fall back to the old direction-agnostic ring
                    // search rather than dividing by a near-zero tlen.
                    sum += measureLocalRadiusPx(origBin, w, h,
                                                 path[k].x, path[k].y,
                                                 MAX_MEASURED_STROKE_RADIUS_PX);
                }
            }
            float pathAvgRadiusPx = (numPoints > 0) ? (float)(sum / numPoints) : 1.0f;
            if (pathAvgRadiusPx < 1.0f) pathAvgRadiusPx = 1.0f;

            for (int i = 0; i < segCount; i++)
                segs[i].avgRadiusPx = pathAvgRadiusPx;

            for (int i = 0; i < segCount && totalSegCount < MAX_ARC_SEGMENTS; i++)
                allSegments[totalSegCount++] = segs[i];
        }
    }

    if (totalSegCount == 0) {
        printf("No traceable curve found (%s)\n", sourceLabel);
    }

    debugPrintSegments(allSegments, totalSegCount);
    setSegmentOverlay(allSegments, totalSegCount, w, h, stretched);
    debugPrintSegments(allSegments, totalSegCount);

    for (int i = 0; i < componentPathCount; i++) free(componentPaths[i]);
    free(remaining);
    free(compBin);
    free(origBin);

    free(img->data);
    free(img->bin);
    free(img);
}

void RunTracePipeline(void)
{
    Image* img = canvasToImage();

    if (!img) {
        // No canvas strokes - check if there's a pending uploaded BMP to trace instead
        if (RunPendingUploadTrace()) {
            return;  // Just trace, don't auto-show
        }
        printf("Canvas is empty - draw a curve or upload a BMP first\n");
        return;
    }

    freePendingBmpImage();   // a manual trace supersedes any not-yet-viewed upload
    runPipelineOnImage(img, "canvas drawing", FALSE);  // not stretched
}

void RunUploadPipeline(void)
{
    Image* img = loadBMP_UI("");
    if (!img) {
        printf("No image loaded\n");
        return;
    }

    displayImageOnCanvas(img);

    img->bin = img_to_bin(img);
    if (!img->bin) {
        printf("Failed to binarize uploaded image\n");
        free(img->data);
        free(img);
        return;
    }

    // NEW: upload just displays the image now - no auto-trace. Drop any
    // stale results/pending image from before so they don't linger on
    // screen or leak.
    freePendingBmpImage();
    canvas.showSegments = FALSE;
    canvas.segmentResultCount = 0;

    s_pendingBmpImage = img;   // traced later, on demand (see RunPendingUploadTrace)
}

BOOL RunPendingUploadTrace(void)
{
    if (!s_pendingBmpImage) return FALSE;

    Image* img = s_pendingBmpImage;
    s_pendingBmpImage = NULL;   // runPipelineOnImage frees img when done
    runPipelineOnImage(img, "uploaded BMP", TRUE);  // stretched to fill the view
    return TRUE;
}
