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

static void runPipelineOnImage(Image* img, const char* sourceLabel, BOOL stretched)
{
    thinningZhangSuen(img);

    int w = img->width;
    int h = img->height;

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

            for (int i = 0; i < segCount && totalSegCount < MAX_ARC_SEGMENTS; i++)
                allSegments[totalSegCount++] = segs[i];
        }
    }

    if (totalSegCount == 0) {
        printf("No traceable curve found (%s)\n", sourceLabel);
    }

    debugPrintSegments(allSegments, totalSegCount);
    setSegmentOverlay(allSegments, totalSegCount, w, h, stretched);   // NEW
    debugPrintSegments(allSegments, totalSegCount);                  // NEW

    for (int i = 0; i < componentPathCount; i++) free(componentPaths[i]);
    free(remaining);
    free(compBin);

    free(img->data);
    free(img->bin);
    free(img);
}

void RunTracePipeline(void)
{
    Image* img = canvasToImage();
    if (!img) return;

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
