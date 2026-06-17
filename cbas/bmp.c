#include <stdio.h>
#include <stdlib.h>

#include "bmp.h"

typedef struct {
    unsigned short type;
    unsigned int size;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int offset;
} BMPHeader;

typedef struct {
    unsigned int size;
    int width;
    int height;
    unsigned short planes;
    unsigned short bits;
    unsigned int compression;
    unsigned int imagesize;
    int xres;
    int yres;
    unsigned int colors;
    unsigned int importantColors;
} DIBHeader;

Image* loadBMP(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    BMPHeader bmpHeader;
    DIBHeader dibHeader;

    fread(&bmpHeader, sizeof(bmpHeader), 1, f);
    fread(&dibHeader, sizeof(dibHeader), 1, f);

    Image* img = malloc(sizeof(Image));
    img->width = dibHeader.width;
    img->height = dibHeader.height;

    int size = img->width * img->height * 3;
    img->data = malloc(size);

    fseek(f, bmpHeader.offset, SEEK_SET);
    fread(img->data, 1, size, f);

    fclose(f);
    return img;
}
