#include "bmp_ui.h"

Image* loadBMP_UI(const char* filename)
{
    char path[MAX_PATH];

    if (filename == NULL || filename[0] == '\0')
    {
        if (!openFileDialog(path, MAX_PATH, 1))
        {
            printf("No file selected\n");
            return NULL;
        }
    }
    else
    {
        strncpy(path, filename, MAX_PATH);
        path[MAX_PATH - 1] = '\0';
    }

    return loadBMP(path);
}

int saveBMP_UI(const char* filename, Image* img, uint8_t* bin, BMPMode mode)
{
    char path[MAX_PATH];

    // If no filename provided → open save dialog
    if (filename == NULL || filename[0] == '\0')
    {
        if (!saveFileDialog(path, MAX_PATH, "bmp", 1))
        {
            printf("Save cancelled\n");
            return 0;
        }
        filename = path;
    }

    if (mode == BMP_RGB)
        return saveBMP_RGB(filename, img);

    if (mode == BMP_BIN)
        return saveBMP_BIN(filename, bin, img->width, img->height);

    return 0;
}
