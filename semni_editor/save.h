#pragma once

#include <windows.h>
#include "app.h"

// Renders "app" into the back buffer without the draggable handles, reads
// that back, and writes it out as an uncompressed 24-bit BMP -- so the
// exported image shows the robot/paint/environment content only, not the
// editor's handle overlay. "hwnd" is just used to get the canvas size.
// Returns 1 on success, 0 on failure (bad window size, can't open the
// file, etc). What's currently displayed on screen is left untouched.
int saveCanvasAsBMP(const char* filename, HWND hwnd, AppState* app);

// Saves the robot's current pose as mathematical equations in a text file.
// Includes all geometric parameters (circle positions/radii, arc angles, etc)
// in a human-readable mathematical format.
int saveRobotAsEquations(const char* filename, AppState* app);

// Same idea as saveRobotAsEquations, for Rocky's/Stilo's own fields --
// see canvas.c's File > Save handling (ID_SAVE), which picks the filename
// and which of these three to call based on app->robotScene.activeKind.
int saveRockyAsEquations(const char* filename, AppState* app);
int saveStiloAsEquations(const char* filename, AppState* app);

// Reads back the KEY=value equations format the three save*AsEquations
// functions above write, into "out" -- same field/key mapping, so a file
// saved by one of them round-trips exactly through the matching loader
// here. Used to load a user-set custom Standing/Home pose (see input.c's
// ID_SET_STANDING_BUTTON/ID_SET_HOME_BUTTON, which save to these same
// files, and ID_STANDING_POSITION_BUTTON/ID_HOME_POSITION_BUTTON, which
// load them back). Returns 0 if the file doesn't exist (or can't be
// opened) WITHOUT touching "out" at all, so the caller can just fall back
// to its own hardcoded default in that case -- 1 on success. Unrecognized
// keys (e.g. from a hand-edited or stale file) are silently skipped rather
// than treated as an error, same forgiving spirit as the rest of this
// file's plain-text format.
int loadRobotPoseFromFile(const char* filename, Semni* out);
int loadRockyPoseFromFile(const char* filename, Rocky* out);
int loadStiloPoseFromFile(const char* filename, Stilo* out);

// Writes Rocky's body + leg as a pair of plain-numeric files, "Rob.txt"
// (rectangle) and "Arm.txt" (kneeCircle->2 shin fillets->footCircle),
// for an external mass/joint/geometry consumer -- see save.c's own
// comment just above the implementation for the full format. Reads
// app->robotScene.rocky, including its user-entered bodyWeight/legWeight
// (see input.c's hBodyWeightEdit/hLegWeightEdit). Returns 0 if either
// file can't be opened for writing, 1 on success.
int saveRockyAsRobArm(AppState* app);

// Pure geometric helpers behind saveRockyAsRobArm's approximate leg
// centroid (see save.c's own comment above each) -- exposed (not static)
// so renderer.c's live on-canvas mass-center marker
// (computeRockyMassCenterWorld) can reuse the exact same math against
// WORLD-space (rotated) points instead of duplicating it. Purely
// geometric -- neither cares whether the points passed in are Rob.txt's
// export-frame LOCAL ones or the renderer's own rotated WORLD ones.
int robArmSampleArcThroughMid(PointF center, PointF start, PointF mid, PointF end, int steps, PointF* out);
int robArmSampleArcAwayFrom(PointF center, PointF start, PointF end, PointF otherCenter, int steps, PointF* out);
PointF robArmPolygonCentroid(PointF* pts, int n);

// Same Rob.txt/Leg.txt-style export as saveRockyAsRobArm above, generalized
// to Semni's arc-based (head/butt/seam-arc) torso and its two-stage
// (hip->knee->foot) leg -- see save.c's own comment above each
// implementation for the exact file format and joint/origin conventions.
// Writes SemniExport\Rob.txt (torso) and SemniExport\Leg.txt (leg).
// Semni has no bodyWeight/legWeight fields (unlike Rocky) -- both are
// written as a flat 1.0, and each half's own geometric centroid is used
// in place of a mass-weighted combined center. Returns 0 if either file
// can't be opened for writing, 1 on success.
int saveSemniAsRobLeg(AppState* app);

// Same idea as saveSemniAsRobLeg, for Stilo -- Stilo shares Semni's own
// torso shape (so StiloExport\Rob.txt lists both legs' hip joints), but
// has TWO independent single-stage legs instead of Semni's one two-stage
// leg, so it writes StiloExport\Leg1.txt and StiloExport\Leg2.txt (one
// per leg) rather than a single Leg.txt. Same weight=1.0/geometric-
// centroid simplification as saveSemniAsRobLeg. Returns 0 if any of the
// three files can't be opened for writing, 1 on success.
int saveStiloAsRobLeg(AppState* app);
