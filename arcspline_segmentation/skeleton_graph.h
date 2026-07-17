#pragma once

#include <stdint.h>
#include "geometry.h"

// NEW: Traces a closed loop (circle/ring) in a component.
// Returns number of points traced (0 if no loop found).
// First and last point are the same pixel, indicating closure.
int traceClosedLoop(uint8_t* compBin, int w, int h, Point* outPath, int maxPoints);

// Finds branch/junction pixels (degree >= 3) in a component - the points
// where a traced shape splits into multiple edges (a Y or T or X shape,
// e.g. a line touching a circle). A real junction is usually a small
// blob of several touching high-degree pixels rather than exactly one,
// so nearby hits are merged into a single output point per cluster.
// Returns the number of junction points written (<= maxOut), filling
// outX/outY with their pixel coordinates.
int findJunctionPixels(uint8_t* compBin, int w, int h, int outX[], int outY[], int maxOut);

// Decomposes ONE already-isolated connected skeleton component (compBin -
// same w*h size as the source image, containing just this component's
// pixels, see extractComponent() in pipeline.c) into a set of traceable
// "edges":
//
//   - a simple open stroke (no junctions) becomes exactly one edge, the
//     same result the old find_start_end_pixels + tracePath pair produced
//   - a stroke with T/X junctions (e.g. lines touching circles) becomes one
//     edge per branch between endpoint/junction nodes
//   - a pure closed loop (no loose ends anywhere) becomes one edge whose
//     first and last point are the same pixel - canvas_bridge.c's
//     sampleArcPoints() checks for that to render a full circle instead of
//     the shortest arc between two points
//
// Each edge's points are written into outPaths[i] (caller-allocated, each
// holding up to maxPointsPerPath Points) with its length in outLengths[i].
// Returns the number of edges written (<= maxEdges).
int traceComponentEdges(uint8_t* compBin, int w, int h,
                         Point* outPaths[], int outLengths[],
                         int maxEdges, int maxPointsPerPath);
