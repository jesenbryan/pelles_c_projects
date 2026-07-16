#pragma once
#include "ui_state.h"   // for BOOL

void RunTracePipeline(void);
void RunUploadPipeline(void);

// Traces + builds segments for the most recently uploaded BMP, if it hasn't
// been traced yet (upload no longer auto-traces - see RunUploadPipeline).
// Safe to call any time; it's a no-op if there's nothing pending. Returns
// TRUE if it actually ran the pipeline.
BOOL RunPendingUploadTrace(void);
