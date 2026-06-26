#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "bmp.h"
#include "file_dialogs.h"

typedef enum {
    BMP_RGB,
    BMP_BIN
} BMPMode;

Image* loadBMP_UI(const char* filename);

int saveBMP_UI(const char* filename, Image* img, uint8_t* bin, BMPMode mode);
