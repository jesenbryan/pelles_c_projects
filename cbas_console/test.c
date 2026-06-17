#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    int width;
    int height;
    unsigned char* data;
} Image;

static int rowSize(int w)
{
    return ((w * 3 + 3) / 4) * 4;
}

Image* loadBMP(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    BMPHeader bh;
    DIBHeader dh;

    fread(&bh, sizeof(bh), 1, f);
    fread(&dh, sizeof(dh), 1, f);

    Image* img = malloc(sizeof(Image));
    img->width = dh.width;
    img->height = dh.height;

    int rs = rowSize(img->width);

    img->data = malloc(rs * img->height);

    fseek(f, bh.offset, SEEK_SET);
    fread(img->data, 1, rs * img->height, f);

    fclose(f);
    return img;
}

void saveBMP(const char* filename, Image* img)
{
    FILE* f = fopen(filename, "wb");

    int rs = rowSize(img->width);
    int size = rs * img->height;

    BMPHeader bh = {0};
    DIBHeader dh = {0};

    bh.type = 0x4D42;
    bh.offset = sizeof(BMPHeader) + sizeof(DIBHeader);
    bh.size = bh.offset + size;

    dh.size = sizeof(DIBHeader);
    dh.width = img->width;
    dh.height = img->height;
    dh.planes = 1;
    dh.bits = 24;
    dh.imagesize = size;

    fwrite(&bh, sizeof(bh), 1, f);
    fwrite(&dh, sizeof(dh), 1, f);
    fwrite(img->data, 1, size, f);

    fclose(f);
}

int isLinePixel(unsigned char* data, int x, int y, int w, int rs)
{
    unsigned char* p = &data[y * rs + x * 3];

    int b = p[0];
    int g = p[1];
    int r = p[2];

    int gray = (r + g + b) / 3;

    return gray < 128;
}

void toBinary(unsigned char* data, unsigned char* bin, int w, int h)
{
    int rs = rowSize(w);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            bin[y * w + x] = isLinePixel(data, x, y, w, rs);
        }
    }
}

int countNeighbors(unsigned char* img, int x, int y, int w, int h)
{
    int c = 0;

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0) continue;

            int nx = x + dx;
            int ny = y + dy;

            if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                continue;

            if (img[ny * w + nx])
                c++;
        }
    }

    return c;
}

void skeletonize(unsigned char* img, int w, int h)
{
    int changed;

    do
    {
        changed = 0;

        unsigned char* copy = malloc(w * h);
        memcpy(copy, img, w * h);

        for (int y = 1; y < h - 1; y++)
        {
            for (int x = 1; x < w - 1; x++)
            {
                if (!img[y * w + x]) continue;

                int n = countNeighbors(img, x, y, w, h);

                if (n <= 1 || n >= 7)
                {
                    copy[y * w + x] = 0;
                    changed = 1;
                }
            }
        }

        memcpy(img, copy, w * h);
        free(copy);

    } while (changed);
}

Image* createOutput(int w, int h, unsigned char* bin)
{
    Image* out = malloc(sizeof(Image));
    out->width = w;
    out->height = h;

    int rs = rowSize(w);
    out->data = calloc(rs * h, 1);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            if (bin[y * w + x])
            {
                unsigned char* p = &out->data[y * rs + x * 3];
                p[0] = p[1] = p[2] = 0;
            }
            else
            {
                unsigned char* p = &out->data[y * rs + x * 3];
                p[0] = p[1] = p[2] = 255;
            }
        }
    }

    return out;
}

int main()
{
    Image* img = loadBMP("Untitled.bmp");
    if (!img)
    {
        printf("Failed to load BMP\n");
        return 0;
    }

    int w = img->width;
    int h = img->height;

    unsigned char* bin = malloc(w * h);

    toBinary(img->data, bin, w, h);

    skeletonize(bin, w, h);

    Image* out = createOutput(w, h, bin);

    saveBMP("skeleton.bmp", out);

    printf("Saved: skeleton.bmp\n");

    free(bin);
    free(img->data);
    free(img);
    free(out->data);
    free(out);

    return 0;
}
