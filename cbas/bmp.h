#pragma once

typedef struct {
    int width;
    int height;
    unsigned char* data;
} Image;

Image* loadBMP(const char* filename);
