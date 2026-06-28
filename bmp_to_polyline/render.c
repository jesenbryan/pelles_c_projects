#include <stdio.h>
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
