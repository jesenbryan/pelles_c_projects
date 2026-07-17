#pragma once

#include <stdint.h>
#include "geometry.h"

// NEW: Traces a closed loop (circle/ring) in a component.
// Returns number of points traced (0 if no loop found).
// First and last point are the same pixel, indicating closure.
int traceClosedLoop(uint8_t* compBin, int w, int h, Point* outPath, int maxPoints);

// Finds real branch/junction points among edges already produced by
// traceComponentEdges() for one component - a node is a genuine junction
// only if 3 or more of those edges actually end there.
//
// Deliberately does NOT look at raw pixel degree (checking for pixels
// with 3+ foreground neighbors): Zhang-Suen thinning routinely leaves
// short 1-3px "spur" artifacts on an otherwise perfectly simple curve,
// each of which reads as a local degree-3 pixel even though nothing
// actually branches there. traceComponentEdges already filters those
// spurs out (via MIN_EDGE_POINTS) when building its edge list, so
// counting edge ENDPOINTS instead of raw pixel degree automatically
// inherits that same filtering - a spur that got dropped as too short
// can't contribute a false junction here.
//
// edgePaths[e]/edgeLengths[e] for e in [0, edgeCount) must be the exact
// output of a traceComponentEdges() call on the same component. A
// self-closing edge (closed loop, first point == last point) is treated
// as a seam, not a junction, and contributes nothing. Nearby node hits
// (within a few pixels, same physical point) are merged before counting.
// Returns the number of junction points written (<= maxOut).
int findRealJunctions(Point* const edgePaths[], const int edgeLengths[], int edgeCount,
                       int outX[], int outY[], int maxOut);

// Companion to findRealJunctions(): finds true endpoint nodes (touched by
// exactly 1 kept edge) among the same edge set. Same reasoning applies in
// reverse - a spurious mid-curve thinning artifact can split one visually
// continuous stroke into two or more kept edges (each individually long
// enough to survive MIN_EDGE_POINTS), so picking markers off just the
// first edge's own two ends can land the "far" marker at that artificial
// split instead of the curve's true far end. This looks at every edge's
// endpoints instead, so it finds the real ones regardless of how many
// pieces the curve got split into internally.
// Returns the number of endpoint nodes written (<= maxOut).
int findRealEndpoints(Point* const edgePaths[], const int edgeLengths[], int edgeCount,
                       int outX[], int outY[], int maxOut);

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
