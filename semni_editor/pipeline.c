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
    BOOL haveMarkers = FALSE;   // only the first curve's endpoints get the red/blue markers

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

            if (!haveMarkers) {
                setEndpointMarkers(w, h, sx, sy, ex, ey, stretched);
                haveMarkers = TRUE;
            }

            Point* path = (Point*)malloc(sizeof(Point) * 10000);
            componentPaths[componentPathCount++] = path;

            int numPoints = tracePath(compBin, w, h, sx, sy, ex, ey, path, 10000);
            debugPrintPath(path, numPoints);

            ArcSegment segs[MAX_ARC_SEGMENTS];
            int segCount = buildSegments(path, numPoints, segs);

            // Recover each segment's original stroke width (see
            // measureLocalRadiusPx above) from the preserved pre-thinning
            // raster, before the segment (and the path buffer its pts[]
            // point into) gets copied off and this component's own
            // buildSegments locals go out of scope.
            for (int i = 0; i < segCount; i++)
            {
                double sum = 0.0;
                int n = segs[i].count;
                for (int k = 0; k < n; k++)
                {
                    sum += measureLocalRadiusPx(origBin, w, h,
                                                 segs[i].pts[k].x, segs[i].pts[k].y,
                                                 MAX_MEASURED_STROKE_RADIUS_PX);
                }
                segs[i].avgRadiusPx = (n > 0) ? (float)(sum / n) : 1.0f;
                if (segs[i].avgRadiusPx < 1.0f) segs[i].avgRadiusPx = 1.0f;
            }

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
    canvas.hasEndpointMarkers = FALSE;

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
