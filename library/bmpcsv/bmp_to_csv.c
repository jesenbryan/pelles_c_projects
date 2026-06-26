#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "bmp_io.h"

int bmp_to_csv(const char *bmpPath, const char *csvPath)
{
    FILE *bmp = fopen(bmpPath, "rb");
    if (!bmp) return 0;

    BMPFILEHEADER fileHeader;
    BMPINFOHEADER infoHeader;

    if (fread(&fileHeader, sizeof(fileHeader), 1, bmp) != 1 ||
        fread(&infoHeader, sizeof(infoHeader), 1, bmp) != 1)
    {
        fclose(bmp);
        return 0;
    }

    if (infoHeader.biBitCount != 24 || fileHeader.bfType != 0x4D42) {
        fclose(bmp);
        return 0;
    }

    int width = infoHeader.biWidth;
    int height = abs(infoHeader.biHeight);

    int rowSize = ((width * 3 + 3) / 4) * 4;

    uint8_t *row = (uint8_t*)malloc(rowSize);
    if (!row) {
        fclose(bmp);
        return 0;
    }

    FILE *csv = fopen(csvPath, "w");
    if (!csv) {
        free(row);
        fclose(bmp);
        return 0;
    }

    fprintf(csv, "x,y,r,g,b\n");

    fseek(bmp, fileHeader.bfOffBits, SEEK_SET);

    for (int y = height - 1; y >= 0; y--) {

        if (fread(row, 1, rowSize, bmp) != rowSize)
            break;

        for (int x = 0; x < width; x++) {
            uint8_t b = row[x * 3 + 0];
            uint8_t g = row[x * 3 + 1];
            uint8_t r = row[x * 3 + 2];

            if (r || g || b) {
                fprintf(csv, "%d,%d,%u,%u,%u\n", x, y, r, g, b);
            }
        }
    }

    fclose(csv);
    fclose(bmp);
    free(row);

    return 1;
}
