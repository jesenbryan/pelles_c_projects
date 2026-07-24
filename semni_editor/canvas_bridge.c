#include "canvas_bridge.h"
#include "ui_state.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ARC_PI 3.14159265358979323846

// Inverse of the pixel->world mapping used in canvas.c's mouse handlers.
// Must use the CURRENT window size/zoom, since a stroke's stored (x,y)
// is in world space and gets reprojected under whatever zoom is active now.
static void worldToPixel(float wx, float wy, int w, int h, float* px, float* py)
{
    float aspect = (float)w / (float)h;

    // undo pan to get the ortho-space coordinate currently on screen
    float orthoX = wx - canvas.panX;
    float orthoY = wy - canvas.panY;

    if (aspect >= 1.0f) {
        *px = (w / 2.0f) * (orthoX / (aspect * canvas.zoom) + 1.0f);
        *py = (h / 2.0f) * (1.0f - orthoY / canvas.zoom);
    } else {
        *px = (w / 2.0f) * (orthoX / canvas.zoom + 1.0f);
        *py = (h / 2.0f) * (1.0f - (orthoY * aspect) / canvas.zoom);
    }
}

static void pixelToWorldExact(float px, float py, int w, int h, float* wx, float* wy)
{
    float aspect = (float)w / (float)h;

    if (aspect >= 1.0f) {
        *wx = ((2.0f * px / w) - 1.0f) * aspect * canvas.zoom;
        *wy = (1.0f - (2.0f * py / h)) * canvas.zoom;
    } else {
        *wx = ((2.0f * px / w) - 1.0f) * canvas.zoom;
        *wy = (1.0f - (2.0f * py / h)) * (1.0f / aspect) * canvas.zoom;
    }

    *wx += canvas.panX;   // NEW: store as true world coords, like strokes do
    *wy += canvas.panY;   // NEW
}

// Uses the FIXED bg bounds, so no canvas.zoom/pan math needed here —
// pan/canvas.zoom are applied uniformly later by the shared render pipeline
static void pixelToWorldStretched(float px, float py, int imgW, int imgH, float* wx, float* wy)
{
    float u = px / (float)imgW;
    float v = 1.0f - (py / (float)imgH);

    *wx = bgLeft + u * (bgRight - bgLeft);
    *wy = bgBottom + v * (bgTop - bgBottom);
}

void setEndpointMarkers(int imgW, int imgH, int sx, int sy, int ex, int ey, BOOL stretched)
{
    if (stretched) {
        pixelToWorldStretched((float)sx, (float)sy, imgW, imgH, &markerStartX, &markerStartY);
        pixelToWorldStretched((float)ex, (float)ey, imgW, imgH, &markerEndX,   &markerEndY);
    } else {
        pixelToWorldExact((float)sx, (float)sy, imgW, imgH, &markerStartX, &markerStartY);
        pixelToWorldExact((float)ex, (float)ey, imgW, imgH, &markerEndX,   &markerEndY);
    }

    canvas.hasEndpointMarkers = TRUE;
    if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
}

void addBranchMarker(int imgW, int imgH, int px, int py, BOOL stretched)
{
    if (branchMarkerCount >= MAX_BRANCH_MARKERS) return;

    float wx, wy;
    if (stretched) {
        pixelToWorldStretched((float)px, (float)py, imgW, imgH, &wx, &wy);
    } else {
        pixelToWorldExact((float)px, (float)py, imgW, imgH, &wx, &wy);
    }

    branchMarkersWorld[branchMarkerCount * 2]     = wx;
    branchMarkersWorld[branchMarkerCount * 2 + 1] = wy;
    branchMarkerCount++;

    if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
}

static void setBinPixel(Image* img, int x, int y)
{
    if (x < 0 || y < 0 || x >= img->width || y >= img->height) return;

    int idx = y * img->width + x;
    img->bin[idx] = 1;

    img->data[idx * 3 + 0] = 0; // B
    img->data[idx * 3 + 1] = 0; // G
    img->data[idx * 3 + 2] = 0; // R
}

static void stampDisc(Image* img, float cx, float cy, float r)
{
    int minX = (int)floorf(cx - r);
    int maxX = (int)ceilf(cx + r);
    int minY = (int)floorf(cy - r);
    int maxY = (int)ceilf(cy + r);

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            float dx = (x + 0.5f) - cx;
            float dy = (y + 0.5f) - cy;
            if (dx * dx + dy * dy <= r * r) {
                setBinPixel(img, x, y);
            }
        }
    }
}

static void stampSegment(Image* img, float x0, float y0, float x1, float y1, float r)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.001f) {
        stampDisc(img, x0, y0, r);
        return;
    }

    float step = (r * 0.5f > 0.5f) ? r * 0.5f : 0.5f;
    int steps = (int)ceilf(len / step);
    if (steps < 1) steps = 1;

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        stampDisc(img, x0 + dx * t, y0 + dy * t, r);
    }
}

// Sub-pixel arc sample point - deliberately NOT the int-based Point type used
// for traced pixel paths. See note below.
// Named SampleF rather than PointF to avoid colliding with geometry.h's
// PointF (the unrelated robot-joint float point type, now merged into
// geometry.h alongside this pixel-domain code).
typedef struct { float x, y; } SampleF;

static void sampleArcPoints(ArcSegment* seg, SampleF* outPts, int* outCount, int maxOut)
{
    Circle c = seg->circle;

    if (c.r < 1e-3f) {
        // degenerate/near-straight segment - no meaningful circle, fall back
        // FIXED: sample evenly across the ENTIRE segment, not just the first N points
        int n = seg->count;
        int sampleCount = (n > maxOut) ? maxOut : n;
        
        for (int i = 0; i < sampleCount; i++) {
            int idx = (n > 1) ? (i * (n - 1)) / (sampleCount - 1) : 0;
            outPts[i].x = (float)seg->pts[idx].x;
            outPts[i].y = (float)seg->pts[idx].y;
        }
        *outCount = sampleCount;
        return;
    }

    Point p0 = seg->pts[0];
    Point p1 = seg->pts[seg->count - 1];

    double a0 = atan2(p0.y - c.cy, p0.x - c.cx);
    double a1 = atan2(p1.y - c.cy, p1.x - c.cx);

    double diff = a1 - a0;
    while (diff > ARC_PI)  diff -= 2.0 * ARC_PI;
    while (diff < -ARC_PI) diff += 2.0 * ARC_PI;

    int steps = (maxOut < 24) ? maxOut : 24;

    for (int i = 0; i < steps; i++) {
        double t = (double)i / (double)(steps - 1);
        double a = a0 + diff * t;
        // NEW: keep sub-pixel precision here - these come from an analytic
        // circle fit (float cx/cy/r), not from the traced pixel grid, so
        // there's no reason to snap them to whole source-image pixels.
        // Doing so used to be invisible for hand-drawn strokes (traced at
        // full window resolution, so 1px error is negligible) but very
        // visible for uploaded BMPs, where the curve can occupy only a
        // few dozen source pixels before being stretched to fill the
        // window - the same 1px snap becomes a visible staircase.
        outPts[i].x = (float)(c.cx + c.r * cos(a));
        outPts[i].y = (float)(c.cy + c.r * sin(a));
    }

    // NEW: force the two ends of this sampled arc to be the EXACT raw path
    // pixel (p0/p1), not their circle-projected position. p0/p1 are
    // themselves ordinary pixels from the single shared path buffer, and
    // buildSegments's recursive split always hands the two halves of a run
    // a literal shared boundary point (out[i].pts/out[i+1].pts overlap by
    // exactly that one Point) -- so the neighboring segment on either side
    // of this one has that exact same raw pixel as ITS p1/p0. But each
    // segment fits its OWN circle independently, and that fit is rarely
    // perfect (residual up to ARC_FIT_TOLERANCE), so projecting the shared
    // point onto two different circles yields two slightly different
    // world positions - a visible gap in the reconstructed ribbon
    // (segmentPointsWorld), since canvas.c draws each segment's ribbon
    // separately. Snapping both ends back to the one raw pixel they
    // actually share makes adjacent segments meet at a bit-identical
    // world coordinate; only the interior of the arc still follows the
    // fitted circle.
    outPts[0].x        = (float)p0.x;
    outPts[0].y        = (float)p0.y;
    outPts[steps - 1].x = (float)p1.x;
    outPts[steps - 1].y = (float)p1.y;

    *outCount = steps;
}

void setSegmentOverlay(ArcSegment* segments, int count, int imgW, int imgH, BOOL stretched)
{
    int total = 0;
    int usedSegments = 0;

    for (int s = 0; s < count && s < MAX_ARC_SEGMENTS; s++)
    {
        SampleF arcPts[24];
        int arcCount = 0;
        sampleArcPoints(&segments[s], arcPts, &arcCount, 24);

        if (arcCount < 2) continue;
        if (total + arcCount > MAX_SEGMENT_POINTS) break;

        segmentStarts[usedSegments] = total;
        segmentCounts[usedSegments] = arcCount;

        // NEW: ghost circle in world space - the FULL circle this arc was cut
        // from, not just the sampled arc points. Same pixel->world helpers
        // used for the arc points above, so it lines up exactly with them.
        //
        // NOTE: in "stretched" mode the background image is scaled by
        // different factors along x and y to fill the window (see
        // pixelToWorldStretched), so a pixel-space circle maps to an
        // ELLIPSE in world space, not a circle. Sample an offset point
        // along each axis separately and keep two radii - using a single
        // radius for both axes (the old behavior) made the ghost circle
        // drift away from the actual arc except where it happened to touch
        // the x-offset sample point.
        Circle c = segments[s].circle;
        if (c.r > 1e-3f) {
            float wcx, wcy, wrx, wry, wrxY, wryY;
            if (stretched) {
                pixelToWorldStretched(c.cx, c.cy, imgW, imgH, &wcx, &wcy);
                pixelToWorldStretched(c.cx + c.r, c.cy, imgW, imgH, &wrx, &wry);
                pixelToWorldStretched(c.cx, c.cy + c.r, imgW, imgH, &wrxY, &wryY);
            } else {
                pixelToWorldExact(c.cx, c.cy, imgW, imgH, &wcx, &wcy);
                pixelToWorldExact(c.cx + c.r, c.cy, imgW, imgH, &wrx, &wry);
                pixelToWorldExact(c.cx, c.cy + c.r, imgW, imgH, &wrxY, &wryY);
            }
            float rdx = wrx - wcx;
            float rdy = wry - wcy;
            float rdxY = wrxY - wcx;
            float rdyY = wryY - wcy;
            segmentCircleCenterWorld[usedSegments * 2]     = wcx;
            segmentCircleCenterWorld[usedSegments * 2 + 1] = wcy;
            segmentCircleRadiusWorld[usedSegments]         = sqrtf(rdx * rdx + rdy * rdy);
            segmentCircleRadiusWorldY[usedSegments]        = sqrtf(rdxY * rdxY + rdyY * rdyY);
        } else {
            segmentCircleRadiusWorld[usedSegments]  = 0.0f; // straight/degenerate - no ghost circle
            segmentCircleRadiusWorldY[usedSegments] = 0.0f;
        }

        // This segment's original stroke thickness, converted to the SAME
        // unit strokeThickness[] (canvas.c) is in -- NOT the same as
        // avgRadiusPx itself. canvasToImage stamps the raster this was
        // measured from using strokeThickness[s]/2 as the RADIUS, so
        // avgRadiusPx (measured directly off that raster) comes back as
        // roughly HALF of strokeThickness[s] -- but canvas.c's real strokes
        // render on screen using strokeThickness[s] directly, un-halved, as
        // the half-width (halfW = strokeThickness[s] * canvas.zoom /
        // glWindowWidth -- see canvas.c's stroke render loop). Plugging
        // avgRadiusPx straight into that same formula (which is what the
        // segment ghost overlay/BMP export both do) would render every
        // reconstructed line at HALF its true on-screen weight -- doubling
        // it here is what actually corrects that, not just carries the
        // measurement through.
        float srcRadiusPx = (segments[s].avgRadiusPx > 0.0f) ? segments[s].avgRadiusPx : 1.0f;
        segmentAvgRadiusPx[usedSegments] = srcRadiusPx * 2.0f;

        // Same VISUAL half-width segmentAvgRadiusPx just stored (note: the
        // DOUBLED value, not srcRadiusPx alone -- using srcRadiusPx here
        // would only convert HALF of the line's actual rendered thickness
        // into world units, leaving collision under-compensated by exactly
        // half of what's needed and the robot stopping visibly short of
        // the real surface), but converted into an actual WORLD-SPACE (EWS)
        // length instead of kept as a raw pixel count -- for Simulation's
        // ground collision (canvas.c's pointCollidesWithAnyEnvironmentStroke),
        // which works entirely in EWS units (segmentPointsWorld's own
        // space), not screen pixels. Without this, collision tests the
        // robot against this segment's bare mathematical CENTERLINE while
        // the reconstructed line is actually RENDERED with real thickness
        // around that centerline -- meaning the robot's contact point
        // tracks an invisible line instead of the visible surface it's
        // approaching. Same "offset a point by the pixel amount, see how
        // far it moved in world space" technique as the ghost circle
        // radius above.
        {
            float renderedRadiusPx = segmentAvgRadiusPx[usedSegments]; // == srcRadiusPx * 2.0f
            float refX = arcPts[0].x, refY = arcPts[0].y;
            float wpx, wpy, wrx, wry, wrxY, wryY;
            if (stretched) {
                pixelToWorldStretched(refX, refY, imgW, imgH, &wpx, &wpy);
                pixelToWorldStretched(refX + renderedRadiusPx, refY, imgW, imgH, &wrx, &wry);
                pixelToWorldStretched(refX, refY + renderedRadiusPx, imgW, imgH, &wrxY, &wryY);
            } else {
                pixelToWorldExact(refX, refY, imgW, imgH, &wpx, &wpy);
                pixelToWorldExact(refX + renderedRadiusPx, refY, imgW, imgH, &wrx, &wry);
                pixelToWorldExact(refX, refY + renderedRadiusPx, imgW, imgH, &wrxY, &wryY);
            }
            float tdx = wrx - wpx, tdy = wry - wpy;
            float tdxY = wrxY - wpx, tdyY = wryY - wpy;
            float radiusWorldX = sqrtf(tdx * tdx + tdy * tdy);
            float radiusWorldY = sqrtf(tdxY * tdxY + tdyY * tdyY);
            segmentThicknessWorld[usedSegments] = (radiusWorldX + radiusWorldY) * 0.5f;
        }

        for (int i = 0; i < arcCount; i++)
        {
            float wx, wy;
            if (stretched)
                pixelToWorldStretched((float)arcPts[i].x, (float)arcPts[i].y, imgW, imgH, &wx, &wy);
            else
                pixelToWorldExact((float)arcPts[i].x, (float)arcPts[i].y, imgW, imgH, &wx, &wy);

            segmentPointsWorld[total * 2]     = wx;
            segmentPointsWorld[total * 2 + 1] = wy;
            total++;
        }

        usedSegments++;
    }

    canvas.segmentResultCount = usedSegments;
    if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
}

Image* canvasToImage(void)
{
    if (canvas.pointCount < 1 || canvas.strokeCount < 1) {
        printf("Canvas is empty - draw a curve first\n");
        return NULL;
    }

    int w = glWindowWidth;
    int h = glWindowHeight;

    Image* img = (Image*)malloc(sizeof(Image));
    if (!img) return NULL;

    img->width  = w;
    img->height = h;
    img->data = (uint8_t*)malloc((size_t)w * h * 3);
    img->bin  = (uint8_t*)calloc((size_t)w * h, sizeof(uint8_t));

    if (!img->data || !img->bin) {
        free(img->data);
        free(img->bin);
        free(img);
        return NULL;
    }

    memset(img->data, 255, (size_t)w * h * 3); // white background

    for (int s = 0; s < canvas.strokeCount; s++)
    {
        int start = strokeStarts[s];
        int end   = (s == canvas.strokeCount - 1) ? canvas.pointCount : strokeStarts[s + 1];
        int count = (end - start) / 2;
        if (count < 1) continue;

        float r = strokeThickness[s] / 2.0f;
        if (r < 1.0f) r = 1.0f;

        float px0, py0;
        worldToPixel(points[start], points[start + 1], w, h, &px0, &py0);

        if (count == 1) {
            stampDisc(img, px0, py0, r);
            continue;
        }

        for (int i = 1; i < count; i++)
        {
            float px1, py1;
            worldToPixel(points[start + i * 2], points[start + i * 2 + 1], w, h, &px1, &py1);
            stampSegment(img, px0, py0, px1, py1, r);
            px0 = px1;
            py0 = py1;
        }
    }

    return img;
}

void displayImageOnCanvas(Image* img)
{
    int w = img->width;
    int h = img->height;

    uint8_t* rgb = (uint8_t*)malloc((size_t)w * h * 3);
    if (!rgb) return;

    for (int y = 0; y < h; y++) {
        int srcY = h - 1 - y;
        for (int x = 0; x < w; x++) {
            int srcIdx = (srcY * w + x) * 3;
            int dstIdx = (y * w + x) * 3;
            rgb[dstIdx + 0] = img->data[srcIdx + 2];
            rgb[dstIdx + 1] = img->data[srcIdx + 1];
            rgb[dstIdx + 2] = img->data[srcIdx + 0];
        }
    }

    if (canvasTexture == 0) glGenTextures(1, &canvasTexture);
    glBindTexture(GL_TEXTURE_2D, canvasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    free(rgb);

    // NEW: fixed reference bounds (canvas.zoom = 1), computed once here —
    // WM_PAINT must NOT recompute these using the live canvas.zoom value
    float aspect = (float)glWindowWidth / (float)glWindowHeight;
    if (aspect >= 1.0f) {
        bgRight = aspect;  bgLeft = -bgRight;
        bgTop   = 1.0f;    bgBottom = -bgTop;
    } else {
        bgRight = 1.0f;    bgLeft = -bgRight;
        bgTop   = 1.0f / aspect; bgBottom = -bgTop;
    }

    canvas.hasBackgroundImage = TRUE;
    if (hWndGL) InvalidateRect(hWndGL, NULL, TRUE);
}
