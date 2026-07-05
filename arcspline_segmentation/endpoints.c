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

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            if (!bin[y * w + x])
                continue;

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
	}
	else if (count_found == 0) {
	    printf("No start/end pixels found\n");

	    *sx = -1;
	    *sy = -1;
	    *ex = -1;
	    *ey = -1;
	}
    else {
        printf("Start candidates found: %d\n", count_found);
    }

    return count_found;
}





