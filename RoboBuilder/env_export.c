#include "env_export.h"
#include "ui_state.h"
#include "config.h"
#include <stdio.h>

// Writes the Environment layer's currently fitted arc segments
// (segmentPointsWorld/segmentStarts/segmentCounts, set by the last Trace/
// Compare pass -- see canvas_bridge.c's setSegmentOverlay) to a plain-text
// file, one line per segment: start.x start.y end.x end.y mid.x mid.y --
// the exact same "start, end, a THIRD POINT THAT ACTUALLY LIES ON THE
// ARC's curve between them" convention save.c's saveRockyAsRobArm uses for
// Arm.txt, so an external consumer reading both files can rely on one
// shared format. The "mid" point here is simply the segment's own
// middle SAMPLED point (index count/2) rather than a freshly-computed
// arc-midpoint angle like Arm.txt's -- segmentPointsWorld already IS a
// dense poly-line sample of the fitted arc (or straight line -- see
// canvas_bridge.c's comment on segmentCircleRadiusWorld == 0), so its own
// middle sample is already a genuine point on the curve, no extra
// geometry needed.
//
// If nothing has ever been traced (canvas.segmentResultCount == 0), still
// writes a valid (empty) file rather than skipping it, so Env.txt always
// exists alongside Env.bmp after a Save -- same "always produce every
// promised file" spirit as saveRockyAsRobArm always writing both Rob.txt
// and Arm.txt.
int saveEnvironmentSegmentsAsTxt(const char* filename)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        return 0;

    for (int s = 0; s < canvas.segmentResultCount; s++)
    {
        int start = segmentStarts[s];
        int count = segmentCounts[s];
        if (count < 2)
            continue;

        float startX = segmentPointsWorld[start * 2];
        float startY = segmentPointsWorld[start * 2 + 1];
        float endX   = segmentPointsWorld[(start + count - 1) * 2];
        float endY   = segmentPointsWorld[(start + count - 1) * 2 + 1];
        int midIdx   = start + count / 2;
        float midX   = segmentPointsWorld[midIdx * 2];
        float midY   = segmentPointsWorld[midIdx * 2 + 1];

        // ENV_EXPORT_SCALE (config.h) applied right before writing, same
        // convention save.c's saveRockyAsRobArm uses for Rob.txt/Arm.txt --
        // converts our world units into "his program"'s own units so an
        // environment traced to look right in OUR editor doesn't come out
        // a fraction of the size once loaded there.
        startX *= ENV_EXPORT_SCALE; startY *= ENV_EXPORT_SCALE;
        endX   *= ENV_EXPORT_SCALE; endY   *= ENV_EXPORT_SCALE;
        midX   *= ENV_EXPORT_SCALE; midY   *= ENV_EXPORT_SCALE;

        fprintf(f, "%.6f %.6f %.6f %.6f %.6f %.6f\n", startX, startY, endX, endY, midX, midY);
    }

    fclose(f);
    return 1;
}

// NOTE: moved out of canvas.c into its own file (env_export.c) so the
// environment editor's export logic doesn't live inside the same file as
// its drawing/input handling -- mirrors save.c owning export logic on the
// robot-editor side. No behavior change from the version above; this
// function was previously `static` (file-local to canvas.c) and is now
// given external linkage via env_export.h so canvas.c can still call it
// after the move.
