#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int width;
    int height;
    //unsigned char* data;
	uint8_t* data;   // RGB or grayscale depending on your loader
    uint8_t* bin;    // binary curve mask (NEW)

    // Per-pixel EXACT intended stroke half-thickness (same unit/meaning as
    // canvas_bridge.c's stampDisc/stampSegment "r" parameter), same
    // width*height footprint as bin/data -- set only where a hand-drawn
    // canvas stroke was actually stamped (canvasToImage), so
    // pipeline.c's width recovery can read the TRUE value directly
    // instead of re-measuring it off the (thinning-destroyed) binary
    // raster, which was always just an approximation of this exact
    // number. NULL for images with no such known-exact source (e.g. an
    // uploaded BMP, see loadBMP/loadBMP_UI) -- pipeline.c falls back to
    // its old measurement-based estimate whenever this is NULL.
    float* radius;
} Image;

Image* loadBMP(const char* filename);

int saveBMP_RGB(const char* filename, Image* img);

int saveBMP_BIN(const char* filename, uint8_t* bin, int w, int h);
