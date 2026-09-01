#pragma once

// Writes the current traced/fitted environment contour (canvas.segmentResultCount
// segments, using the segmentStarts/segmentCounts/segmentPointsWorld arrays
// populated by the last Trace/Compare pass -- see canvas_bridge.c's
// setSegmentOverlay) out to EnvExport\Env.txt. Split out of canvas.c so the
// environment editor's export logic lives in its own file, mirroring how
// save.c owns export logic for the robot editor instead of it living inside
// input.c/renderer.c.
int saveEnvironmentSegmentsAsTxt(const char* filename);
