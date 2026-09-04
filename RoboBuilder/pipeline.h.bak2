#pragma once
#include "ui_state.h"   // for BOOL

void RunTracePipeline(void);
void RunUploadPipeline(void);

// Traces + builds segments for the most recently uploaded BMP, if it hasn't
// been traced yet (upload no longer auto-traces - see RunUploadPipeline).
// Safe to call any time; it's a no-op if there's nothing pending. Returns
// TRUE if it actually ran the pipeline.
BOOL RunPendingUploadTrace(void);

// TRUE if Save's unconditional retrace (canvas.c's ID_SAVE handler) would
// actually have something to trace right now -- an already-drawn canvas
// curve, or an uploaded BMP still waiting on its first trace. Lets that
// Save handler skip calling RunTracePipeline() entirely when neither is
// true (e.g. the user only came here to save a robot and never touched the
// Environment canvas at all), instead of calling it anyway and swallowing
// the "Canvas is empty" console prints that call would otherwise produce.
BOOL CanvasHasTraceableContent(void);
