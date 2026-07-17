#include "pipeline.h"
#include "canvas_bridge.h"
#include "bmp_ui.h"
#include "binary.h"
#include "thinning.h"
#include "skeleton_graph.h"
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

// (historical) find_start_end_pixels/tracePath/buildSegments were all
// written assuming a SINGLE curve fills the whole bin buffer - with more
// than one freehand stroke (or an uploaded BMP with several curves in it,
// see 2curves.bmp), find_start_end_pixels just grabs the first two
// endpoint-looking pixels it scans past, which are almost always both on
// the first stroke, and tracePath then walks off the end of that stroke
// and stops - every other stroke is silently dropped. (find_start_end_pixels
// and tracePath, in endpoints.c/path_trace.c, are no longer called from
// here - see the UPDATE note below - but are kept around since nothing
// else in the pipeline depends on removing them.)
//
// Fix: split the thinned bin buffer into its 8-connected components first
// (one per stroke/curve, since strokes only touch other strokes if the user
// actually draws them crossing/overlapping), then run the trace/build
// pipeline independently per component and merge the resulting ArcSegments
// into one array for setSegmentOverlay.
//
// UPDATE: a single component can itself branch (a Y/T/X-shaped stroke,
// or a line touching a circle) - skeleton_graph.c's traceComponentEdges
// decomposes ONE component into several independently-traceable edges at
// its junction/endpoint nodes, so the path budget below now has to cover
// edges-across-all-components rather than one path per component. Bumped
// accordingly; each entry is just a pointer plus a malloc'd point buffer,
// so the extra headroom is cheap.
#define MAX_TRACE_COMPONENTS 64
#define MAX_EDGES_PER_COMPONENT 32
#define MAX_JUNCTIONS_PER_COMPONENT 16

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

    branchMarkerCount = 0;   // fresh trace run - drop any markers from a previous one

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

            // Branch/junction points (degree >= 3) in this component - a
            // Y/T/X-shaped stroke, or a line touching a circle. Collected
            // before tracing since traceComponentEdges below consumes/marks
            // up the component as it walks edges, but junction detection
            // itself only reads compBin.
            int jx[MAX_JUNCTIONS_PER_COMPONENT], jy[MAX_JUNCTIONS_PER_COMPONENT];
            int jCount = findJunctionPixels(compBin, w, h, jx, jy, MAX_JUNCTIONS_PER_COMPONENT);
            for (int j = 0; j < jCount; j++)
                addBranchMarker(w, h, jx[j], jy[j], stretched);

            // Decompose this component into independently-traceable edges:
            //   - a simple open stroke -> exactly one edge (same result the
            //     old find_start_end_pixels + tracePath pair produced)
            //   - a closed loop (e.g. a circle) -> one edge whose first and
            //     last point are the same pixel (sampleArcPoints in
            //     canvas_bridge.c detects that and draws a full circle)
            //   - a branching stroke -> one edge per segment between nodes
            Point* edgeBufs[MAX_EDGES_PER_COMPONENT];
            int edgeLens[MAX_EDGES_PER_COMPONENT];
            for (int e = 0; e < MAX_EDGES_PER_COMPONENT; e++)
                edgeBufs[e] = (Point*)malloc(sizeof(Point) * 10000);

            int edgeCount = traceComponentEdges(compBin, w, h, edgeBufs, edgeLens,
                                                 MAX_EDGES_PER_COMPONENT, 10000);

            if (edgeCount == 0) {
                for (int e = 0; e < MAX_EDGES_PER_COMPONENT; e++) free(edgeBufs[e]);
                continue;   // noise speck - nothing traceable
            }

            if (!haveMarkers) {
                // Red/blue start-end markers only really mean something for
                // a simple open curve - use the first edge's two ends (for
                // a closed loop these coincide, which is fine/expected).
                Point* first = edgeBufs[0];
                setEndpointMarkers(w, h, first[0].x, first[0].y,
                                   first[edgeLens[0] - 1].x, first[edgeLens[0] - 1].y, stretched);
                haveMarkers = TRUE;
            }

            for (int e = 0; e < edgeCount; e++)
            {
                if (componentPathCount >= MAX_TRACE_COMPONENTS) {
                    free(edgeBufs[e]);   // path budget exhausted - discard, avoid a leak
                    continue;
                }

                componentPaths[componentPathCount++] = edgeBufs[e];

                debugPrintPath(edgeBufs[e], edgeLens[e]);

                ArcSegment segs[MAX_ARC_SEGMENTS];
                int segCount = buildSegments(edgeBufs[e], edgeLens[e], segs);

                for (int i = 0; i < segCount && totalSegCount < MAX_ARC_SEGMENTS; i++)
                    allSegments[totalSegCount++] = segs[i];
            }

            // edgeBufs[edgeCount..MAX_EDGES_PER_COMPONENT-1] were allocated
            // up front but traceComponentEdges never wrote into them (this
            // component simply didn't have that many edges) - free them
            // here rather than leaking one 10000-Point buffer per unused slot.
            for (int e = edgeCount; e < MAX_EDGES_PER_COMPONENT; e++)
                free(edgeBufs[e]);
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
    branchMarkerCount = 0;

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
