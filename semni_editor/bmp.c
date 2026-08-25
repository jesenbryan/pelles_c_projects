#include "bmp.h"

#pragma pack(push, 1)
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
#pragma pack(pop)

Image* loadBMP(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    BMPHeader bmpHeader;
    DIBHeader dibHeader;

    if (fread(&bmpHeader, sizeof(bmpHeader), 1, f) != 1 ||
        fread(&dibHeader, sizeof(dibHeader), 1, f) != 1)
    {
        printf("Failed to read BMP headers\n");
        fclose(f);
        return NULL;
    }

    if (bmpHeader.type != 0x4D42) {
        printf("Not a valid BMP file\n");
        fclose(f);
        return NULL;
    }

    if (dibHeader.bits != 24) {
        printf("Only 24-bit BMP files are supported (got %d-bit)\n", dibHeader.bits);
        fclose(f);
        return NULL;
    }

    Image* img = malloc(sizeof(Image));

    img->width = dibHeader.width;
    img->height = dibHeader.height;

    img->data = malloc(img->width * img->height * 3);

    // No known-exact per-pixel stroke thickness for a loaded BMP (unlike
    // canvasToImage's own hand-drawn strokes) -- explicitly NULL, same
    // convention as img->bin below (left for the caller to fill in, or
    // in this case never filled in at all), so pipeline.c's radius
    // lookup correctly falls back to measuring it instead of reading
    // uninitialized memory.
    img->radius = NULL;

    int rowSize = (img->width * 3 + 3) & ~3;

    uint8_t* row = malloc(rowSize);

    fseek(f, bmpHeader.offset, SEEK_SET);

    for (int y = 0; y < img->height; y++)
    {
        fread(row, 1, rowSize, f);

        int dstY = img->height - 1 - y;  // flip BMP

        memcpy(
            img->data + dstY * img->width * 3,
            row,
            img->width * 3
        );
    }

    free(row);
    fclose(f);

    return img;
}

int saveBMP_RGB(const char* filename, Image* img)
{
    FILE* f = fopen(filename, "wb");
    if (!f) return 0;

    BMPHeader bmpHeader;
    DIBHeader dibHeader;

    int rowSize = (img->width * 3 + 3) & ~3;
    int imageSize = rowSize * img->height;

    bmpHeader.type = 0x4D42;
    bmpHeader.offset = sizeof(BMPHeader) + sizeof(DIBHeader);
    bmpHeader.size = bmpHeader.offset + imageSize;
    bmpHeader.reserved1 = 0;
    bmpHeader.reserved2 = 0;

    dibHeader.size = sizeof(DIBHeader);
    dibHeader.width = img->width;
    dibHeader.height = img->height;
    dibHeader.planes = 1;
    dibHeader.bits = 24;
    dibHeader.compression = 0;
    dibHeader.imagesize = imageSize;
    dibHeader.xres = 0;
    dibHeader.yres = 0;
    dibHeader.colors = 0;
    dibHeader.importantColors = 0;

    fwrite(&bmpHeader, sizeof(bmpHeader), 1, f);
    fwrite(&dibHeader, sizeof(dibHeader), 1, f);

    uint8_t padding[3] = {0,0,0};
    int padSize = rowSize - img->width * 3;

    for (int y = img->height - 1; y >= 0; y--)
    {
        fwrite(img->data + y * img->width * 3,
               1,
               img->width * 3,
               f);

        fwrite(padding, 1, padSize, f);
    }

    fclose(f);
    return 1;
}

int saveBMP_BIN(const char* filename, uint8_t* bin, int w, int h)
{
    FILE* f = fopen(filename, "wb");
    if (!f) return 0;

    int rowSize = (w + 3) & ~3;  // 4-byte alignment
    int imageSize = rowSize * h;

    BMPHeader fileHeader;
    DIBHeader infoHeader;

    fileHeader.type = 0x4D42; // "BM"
    fileHeader.offset = sizeof(BMPHeader) + sizeof(DIBHeader) + 256 * 4;
    fileHeader.size = fileHeader.offset + imageSize;
    fileHeader.reserved1 = 0;
    fileHeader.reserved2 = 0;

    infoHeader.size = sizeof(DIBHeader);
    infoHeader.width = w;
    infoHeader.height = h;
    infoHeader.planes = 1;
    infoHeader.bits = 8;
    infoHeader.compression = 0;
    infoHeader.imagesize = imageSize;
    infoHeader.xres = 2835;
    infoHeader.yres = 2835;
    infoHeader.colors = 256;
    infoHeader.importantColors = 0;

    fwrite(&fileHeader, sizeof(fileHeader), 1, f);
    fwrite(&infoHeader, sizeof(infoHeader), 1, f);

    // grayscale palette
    for (int i = 0; i < 256; i++) {
        uint8_t c[4] = {(uint8_t)i, (uint8_t)i, (uint8_t)i, 0};
        fwrite(c, 4, 1, f);
    }

    uint8_t* row = (uint8_t*)calloc(rowSize, 1);

    // BMP is bottom-up
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            row[x] = bin[y * w + x] ? 255 : 0;
        }
        fwrite(row, rowSize, 1, f);
    }

    free(row);
    fclose(f);

    return 1;
}
