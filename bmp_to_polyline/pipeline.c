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



static void runPipelineOnImage(Image* img, const char* sourceLabel, BOOL stretched)
{
    thinningZhangSuen(img);

    int sx, sy, ex, ey;
    find_start_end_pixels(img->bin, img->width, img->height, &sx, &sy, &ex, &ey);

    if (sx < 0) {
        printf("No traceable curve found (%s)\n", sourceLabel);
        free(img->data);
        free(img->bin);
        free(img);
        return;
    }

    setEndpointMarkers(img->width, img->height, sx, sy, ex, ey, stretched);

    static Point path[10000];
    int numPoints = tracePath(img->bin, img->width, img->height, sx, sy, ex, ey, path, 10000);
    debugPrintPath(path, numPoints);

    static ArcSegment segments[MAX_ARC_SEGMENTS];   // was [64]
	int segCount = buildSegments(path, numPoints, segments);
	debugPrintSegments(segments, segCount);

	setSegmentOverlay(segments, segCount, img->width, img->height, stretched);   // NEW
    debugPrintSegments(segments, segCount);                    // NEW

    free(img->data);
    free(img->bin);
    free(img);
}

void RunTracePipeline(void)
{
    Image* img = canvasToImage();
    if (!img) return;

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

    runPipelineOnImage(img, "uploaded BMP", TRUE);  // stretched to fill the view
}
