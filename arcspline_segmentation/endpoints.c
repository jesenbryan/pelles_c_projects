#include "endpoints.h"

//int find_any_curve_pixel(uint8_t* bin, int w, int h, int* sx, int* sy)
//{
    //for (int y = 0; y < h; y++) {
        //for (int x = 0; x < w; x++) {

            //if (bin[y * w + x]) {
                //*sx = x;
                //*sy = y;
                //return 1;
            //}
        //}
    //}

    //return 0;
//}

int find_start_end_pixels(uint8_t* bin, int w, int h, int* sx, int* sy, int* ex, int* ey)
{
    int count_found = 0;

    // Remembers the first foreground pixel seen, regardless of degree -
    // the fallback start point for a closed loop (a circle's skeleton has
    // no degree-1 pixel anywhere, so the is_start heuristic below never
    // fires for it, and count_found stays 0 even though the shape is
    // perfectly traceable).
    int anyX = -1, anyY = -1;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            if (!bin[y * w + x])
                continue;

            if (anyX == -1) { anyX = x; anyY = y; }

            int N  = (y > 0)         && bin[(y - 1) * w + x];
            int S  = (y < h - 1)     && bin[(y + 1) * w + x];
            int W  = (x > 0)         && bin[y * w + (x - 1)];
            int E  = (x < w - 1)     && bin[y * w + (x + 1)];

            int NW = (y > 0 && x > 0)             && bin[(y - 1) * w + (x - 1)];
            int NE = (y > 0 && x < w - 1)         && bin[(y - 1) * w + (x + 1)];
            int SW = (y < h - 1 && x > 0)         && bin[(y + 1) * w + (x - 1)];
            int SE = (y < h - 1 && x < w - 1)     && bin[(y + 1) * w + (x + 1)];

            int count = N + S + W + E + NW + NE + SW + SE;

            int is_start = 0;

            if (count == 1) {
                is_start = 1;
            }

            if (count == 2) {
                if (
                    (E && NE) || (E && SE) ||
                    (W && NW) || (W && SW) ||
                    (N && NE) || (N && NW) ||
                    (S && SE) || (S && SW)
                ) {
                    is_start = 1;
                }
            }

            if (is_start) {

                if (count_found == 0) {
                    *sx = x;
                    *sy = y;
                }
                else if (count_found == 1) {
                    *ex = x;
                    *ey = y;
                }

                count_found++;
            }
        }
    }

    if (count_found > 2) {
        printf("WARNING: More than 2 start pixels detected (%d)\n", count_found);
    }

    if (count_found == 2) {

	    if (*sx > *ex) {
	        int tx = *sx;
	        int ty = *sy;

	        *sx = *ex;
	        *sy = *ey;

	        *ex = tx;
	        *ey = ty;
	    }

	    printf("Start (RED): (%d, %d) (left)\n", *sx, *sy);
	    printf("End  (BLUE): (%d, %d)\n", *ex, *ey);

	    return 2;
	}

	if (count_found == 0 && anyX == -1) {
	    // Truly empty component - nothing to trace.
	    printf("No start/end pixels found\n");

	    *sx = -1;
	    *sy = -1;
	    *ex = -1;
	    *ey = -1;

	    return 0;
	}

	// count_found is 0 or 1 here, but the component isn't empty: either a
	// closed loop (no natural break point at all, count_found == 0) or a
	// noisy/ambiguous shape with just one candidate (count_found == 1).
	// Either way, there's no real second endpoint to find - fall back to
	// tracing from a single pixel. tracePath() already stops on its own
	// once it runs out of unvisited neighbors, so passing an end point
	// that's never a real pixel (-1, -1 is always out of bounds) makes it
	// walk the whole ring instead of stopping after one point.
	if (count_found == 0) {
	    // No candidate at all - use the first foreground pixel seen.
	    *sx = anyX;
	    *sy = anyY;
	}
	// else count_found == 1: *sx/*sy already hold the one real candidate
	// found during the scan above - keep it rather than an arbitrary pixel.
	*ex = -1;
	*ey = -1;
	printf("Closed loop detected (no distinct endpoints) - tracing from (%d, %d)\n", *sx, *sy);

	return 3;
}





