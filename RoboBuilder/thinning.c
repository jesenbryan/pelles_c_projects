#include "thinning.h"

void thinningZhangSuen(Image* img)
{
    int w = img->width;
    int h = img->height;
    uint8_t* bin = img->bin;

    // Allocate a temporary buffer to store deletion flags for the current iteration
    uint8_t* marker = (uint8_t*)calloc(w * h, sizeof(uint8_t));
    if (!marker) return;

    int changed = 1;
    while (changed)
    {
        changed = 0;

        // --- Step 1 ---
        // We safely step inside the canvas. If you need border pixels thinned, 
        // they must be guarded against array out-of-bounds.
        for (int y = 1; y < h - 1; y++)
        {
            for (int x = 1; x < w - 1; x++)
            {
                int idx = y * w + x;
                if (bin[idx] == 0) continue; // Skip background

                // Read 8-connected neighbors clockwise starting from North
                int p2 = bin[(y - 1) * w + x];     // N
                int p3 = bin[(y - 1) * w + (x + 1)]; // NE
                int p4 = bin[y * w + (x + 1)];     // E
                int p5 = bin[(y + 1) * w + (x + 1)]; // SE
                int p6 = bin[(y + 1) * w + x];     // S
                int p7 = bin[(y + 1) * w + (x - 1)]; // SW
                int p8 = bin[y * w + (x - 1)];     // W
                int p9 = bin[(y - 1) * w + (x - 1)]; // NW

                // Count foreground neighbors (2 <= B(P) <= 6)
                int bp = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;

                // Count 0-to-1 transitions (A(P) == 1)
                int ap = ((p2 == 0 && p3 == 1) ? 1 : 0) +
                         ((p3 == 0 && p4 == 1) ? 1 : 0) +
                         ((p4 == 0 && p5 == 1) ? 1 : 0) +
                         ((p5 == 0 && p6 == 1) ? 1 : 0) +
                         ((p6 == 0 && p7 == 1) ? 1 : 0) +
                         ((p7 == 0 && p8 == 1) ? 1 : 0) +
                         ((p8 == 0 && p9 == 1) ? 1 : 0) +
                         ((p9 == 0 && p2 == 1) ? 1 : 0);

                if (bp >= 2 && bp <= 6 && ap == 1)
                {
                    if ((p2 * p4 * p6 == 0) && (p4 * p6 * p8 == 0))
                    {
                        marker[idx] = 1;
                        changed = 1;
                    }
                }
            }
        }

        // Apply deletion flags for Step 1
        for (int i = 0; i < w * h; i++)
        {
            if (marker[i])
            {
                bin[i] = 0;
                marker[i] = 0; // Reset marker array
            }
        }

        // --- Step 2 ---
        for (int y = 1; y < h - 1; y++)
        {
            for (int x = 1; x < w - 1; x++)
            {
                int idx = y * w + x;
                if (bin[idx] == 0) continue;

                int p2 = bin[(y - 1) * w + x];
                int p3 = bin[(y - 1) * w + (x + 1)];
                int p4 = bin[y * w + (x + 1)];
                int p5 = bin[(y + 1) * w + (x + 1)];
                int p6 = bin[(y + 1) * w + x];
                int p7 = bin[(y + 1) * w + (x - 1)];
                int p8 = bin[y * w + (x - 1)];
                int p9 = bin[(y - 1) * w + (x - 1)];

                int bp = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                int ap = ((p2 == 0 && p3 == 1) ? 1 : 0) +
                         ((p3 == 0 && p4 == 1) ? 1 : 0) +
                         ((p4 == 0 && p5 == 1) ? 1 : 0) +
                         ((p5 == 0 && p6 == 1) ? 1 : 0) +
                         ((p6 == 0 && p7 == 1) ? 1 : 0) +
                         ((p7 == 0 && p8 == 1) ? 1 : 0) +
                         ((p8 == 0 && p9 == 1) ? 1 : 0) +
                         ((p9 == 0 && p2 == 1) ? 1 : 0);

                if (bp >= 2 && bp <= 6 && ap == 1)
                {
                    // Condition change for Step 2: (p2 * p4 * p8 == 0) and (p2 * p6 * p8 == 0)
                    if ((p2 * p4 * p8 == 0) && (p2 * p6 * p8 == 0))
                    {
                        marker[idx] = 1;
                        changed = 1;
                    }
                }
            }
        }

        // Apply deletion flags for Step 2
        for (int i = 0; i < w * h; i++)
        {
            if (marker[i])
            {
                bin[i] = 0;
                marker[i] = 0;
            }
        }
    }

    free(marker);
}
