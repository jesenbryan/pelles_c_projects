#pragma once

#include <stdio.h>
#include <stdint.h>

// Finds where to start (and, for an open curve, end) tracing a thinned
// binary component. Return value:
//   0 - component is empty, nothing to trace (sx/sy/ex/ey set to -1)
//   2 - open curve: two distinct endpoint pixels found (sx,sy)/(ex,ey)
//   3 - closed loop or ambiguous shape: no real second endpoint exists,
//       (sx,sy) is a single pixel to start tracing from and (ex,ey) is
//       set to (-1,-1) - a sentinel tracePath() can never actually reach,
//       so it walks all the way around until it runs out of unvisited
//       neighbors instead of stopping after one point
int find_start_end_pixels(uint8_t* bin, int w, int h, int* sx, int* sy, int* ex, int* ey);
