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
// (see input.c's hWeightRatioSlider). Returns 0 if either
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

// The two tangent points + genuine on-curve midpoint for each of a pair of
// connecting arcs between two circles, plus each arc's own fillet -- see
// save.c's own comment above computeSeamStyleTangents/
// computeLimbStyleTangents for the exact convex/concave construction each
// one uses. Exposed (not static) for the same reason robArmSampleArc*/
// robArmPolygonCentroid are above: renderer.c's Semni/Stilo mass-center
// markers (computeSemniMassCenterWorld/computeStiloMassCenterWorld) reuse
// this exact math against the robot's CURRENT pose instead of duplicating
// the fillet/tangent construction a second time.
typedef struct { PointF c1Tan1, c2Tan1, mid1, c1Tan2, c2Tan2, mid2; Fillet fillet1, fillet2; } ChainTangents;

// Torso seam-arc style (both connecting arcs convex) vs limb (thigh/shin)
// style (arc1 convex, arc2 concave) -- see save.c's own comment above each
// implementation. minR/maxR(1/2) are the same clamp constants each
// caller's own drawXxx function already uses for that pair of arcs
// (MIN_ARC_R/MAX_ARC_R for a torso seam, MIN_THIGH_ARC_R/
// MAX_SEMNI_THIGH_ARC_R/MAX_THIGH_ARC2_CONCAVE_R for a thigh stage,
// MIN_SHIN_ARC_R/MAX_SHIN_ARC_R/MAX_SHIN_ARC2_CONCAVE_R for a shin stage).
ChainTangents computeSeamStyleTangents(PointF c1, float r1, PointF c2, float r2,
                                        float arc1AngleDeg, float arc2AngleDeg,
                                        float minR, float maxR);
ChainTangents computeLimbStyleTangents(PointF c1, float r1, PointF c2, float r2,
                                        float arc1AngleDeg, float arc2AngleDeg,
                                        float minR, float maxR1, float maxR2);

// Fine polygon approximation of a two-circle-plus-two-fillets stage's
// outline (torso seam stage, a Semni thigh/shin stage, or a Stilo leg
// stage) -- see save.c's own comment above the implementation. "out" must
// have room for 4 * (ROB_ARM_ARC_SAMPLES + 1) points when called from
// save.c itself; renderer.c's own mass-center markers use a coarser
// sample count (ROCKY_MASS_CENTER_ARC_SAMPLES) for their array sizing
// instead, same "good enough for an informational dot every frame"
// reasoning as Rocky's own version.
int sampleLimbStageOutline(PointF c1, float r1, PointF c2, float r2, ChainTangents t, int steps, PointF* out);

// Same Rob.txt/Leg.txt-style export as saveRockyAsRobArm above, generalized
// to Semni's arc-based (head/butt/seam-arc) torso and its two-stage
// (hip->knee->foot) leg -- see save.c's own comment above each
// implementation for the exact file format and joint/origin conventions.
// Writes SemniExport\Rob.txt (torso) and SemniExport\Leg.txt (leg).
// Semni now has its own bodyWeight/legWeight/actualWeight fields (same
// idea as Rocky's own, see app.h's Semni comment) -- each file's weight
// number is actualWeight split by the bodyWeight/legWeight ratio, same
// "real, ratio-derived, no double-counting" design as Rocky's own
// Rob.txt/Arm.txt (saveRockyAsRobArm). Each half's own geometric centroid
// (unaffected by that ratio) is still what's written as the x/y position,
// same as Rocky's own Rob.txt. Returns 0 if either file can't be opened
// for writing, 1 on success.
//
// Unlike Rocky's own Rob.txt/Arm.txt (plain 6-float lines, no flag), every
// shape line here leads with a type flag: "0 ..." for a 6-float arc line
// (start, end, mid -- same meaning as before) and "1 cx cy r" for a FULL
// circle (just center + radius, no padding -- the two line kinds are told
// apart by the leading flag, not by a shared length) -- see
// writeArcLine/writeFullCircleLine in save.c. This lets every body circle
// (head, butt, hip, knee, foot) be written as ONE line instead of being
// split into 2 (or, for Semni's kneeCircle, 4) tangent-point pieces.
int saveSemniAsRobLeg(AppState* app);

// Same idea as saveSemniAsRobLeg, for Stilo -- Stilo shares Semni's own
// torso shape (so StiloExport\Rob.txt lists both legs' hip joints), but
// has TWO independent single-stage legs instead of Semni's one two-stage
// leg, so it writes StiloExport\Leg1.txt and StiloExport\Leg2.txt (one
// per leg) rather than a single Leg.txt. Same real, ratio-derived weight
// design as saveSemniAsRobLeg, except Stilo's legWeight is ONE combined
// value for both legs together (see app.h's own comment on Stilo's
// legWeight) -- the leg's own share of actualWeight is split evenly
// (50/50) between Leg1.txt and Leg2.txt. Returns 0 if any of the three
// files can't be opened for writing, 1 on success.
int saveStiloAsRobLeg(AppState* app);
