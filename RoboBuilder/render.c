#include <stdio.h>
#include <string.h>
#include <math.h>
#include "render.h"
#include "bmp_ui.h"

static void setPixelRGB(Image* img,
                        int x, int y,
                        unsigned char r,
                        unsigned char g,
                        unsigned char b)
{
    if (!img) return;

    if (x < 0 || y < 0 || x >= img->width || y >= img->height)
        return;

    int idx = (y * img->width + x) * 3;

    img->data[idx + 0] = b;
    img->data[idx + 1] = g;
    img->data[idx + 2] = r;
}

static void stampDiscForArc(Image* img, float cx, float cy, float r)
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
                setPixelRGB(img, x, y, 50, 50, 50);  // Dark grey
            }
        }
    }
}

static void stampSegmentForArc(Image* img, float x0, float y0, float x1, float y1, float r)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.001f) {
        stampDiscForArc(img, x0, y0, r);
        return;
    }

    float step = (r * 0.5f > 0.5f) ? r * 0.5f : 0.5f;
    int steps = (int)ceilf(len / step);
    if (steps < 1) steps = 1;

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        stampDiscForArc(img, x0 + dx * t, y0 + dy * t, r);
    }
}

void renderSegmentsToImage(Image* img, float* segmentPointsWorld, int* segmentStarts,
                           int* segmentCounts, float* segmentAvgRadiusPx,
                           int segmentResultCount, int imgW, int imgH)
{
    // Clear image to white
    memset(img->data, 255, (size_t)imgW * imgH * 3);

    // Convert world coordinates to pixel coordinates
    // World space: -aspect to +aspect (X), -1 to +1 (Y) at zoom=1, pan=0
    float aspect = (float)imgW / (float)imgH;

    for (int s = 0; s < segmentResultCount; s++)
    {
        int start = segmentStarts[s];
        int count = segmentCounts[s];
        if (count < 2) continue;

        // This segment's own recovered original radius, used directly as
        // this stamp's radius -- used to be one flat `thickness/2.0f` for
        // every segment regardless of how thick the original stroke it was
        // fit from actually was. See render.h's comment.
        float r = segmentAvgRadiusPx[s];
        if (r < 1.0f) r = 1.0f;

        // Convert first point from world to pixel coords
        float wx0 = segmentPointsWorld[start * 2];
        float wy0 = segmentPointsWorld[start * 2 + 1];

        float px0, py0;
        if (aspect >= 1.0f) {
            px0 = (imgW / 2.0f) * (wx0 / aspect + 1.0f);
            py0 = (imgH / 2.0f) * (1.0f - wy0);
        } else {
            px0 = (imgW / 2.0f) * (wx0 + 1.0f);
            py0 = (imgH / 2.0f) * (1.0f - (wy0 * aspect));
        }

        if (count == 1) {
            stampDiscForArc(img, px0, py0, r);
            continue;
        }

        for (int i = 1; i < count; i++)
        {
            float wx1 = segmentPointsWorld[(start + i) * 2];
            float wy1 = segmentPointsWorld[(start + i) * 2 + 1];

            float px1, py1;
            if (aspect >= 1.0f) {
                px1 = (imgW / 2.0f) * (wx1 / aspect + 1.0f);
                py1 = (imgH / 2.0f) * (1.0f - wy1);
            } else {
                px1 = (imgW / 2.0f) * (wx1 + 1.0f);
                py1 = (imgH / 2.0f) * (1.0f - (wy1 * aspect));
            }

            stampSegmentForArc(img, px0, py0, px1, py1, r);
            px0 = px1;
            py0 = py1;
        }
    }
}

void renderSegmentsToBMP(
    const char* outPath,
    Image* img,
    ArcSegment* segments,
    int segCount
)
{
    for (int s = 0; s < segCount; s++)
    {
        unsigned char r = (s * 70) % 255;
        unsigned char g = (s * 130) % 255;
        unsigned char b = (s * 200) % 255;

        ArcSegment seg = segments[s];

        for (int i = 0; i < seg.count; i++)
        {
            Point p = seg.pts[i];
            setPixelRGB(img, p.x, p.y, r, g, b);
        }
    }

    saveBMP_UI(outPath, img, NULL, BMP_RGB);
}
