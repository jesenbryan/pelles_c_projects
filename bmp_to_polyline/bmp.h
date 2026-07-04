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
} Image;

Image* loadBMP(const char* filename);

int saveBMP_RGB(const char* filename, Image* img);

int saveBMP_BIN(const char* filename, uint8_t* bin, int w, int h);
