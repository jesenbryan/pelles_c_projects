#include "binary.h"

uint8_t* img_to_bin(Image* img)
{
    int w = img->width;
    int h = img->height;

    uint8_t* bin = (uint8_t*)malloc(w * h * sizeof(uint8_t));

    if (!bin) return NULL;

    for (int y = 0; y < h; y++) {
    	for (int x = 0; x < w; x++) {

	        int i = y * w + x;

	        uint8_t r = img->data[i * 3 + 0];
	        uint8_t g = img->data[i * 3 + 1];
	        uint8_t b = img->data[i * 3 + 2];

	        uint8_t gray = (uint8_t)(0.299*r + 0.587*g + 0.114*b);

	        bin[i] = (gray < 128) ? 1 : 0;
    	}
	}

    return bin;
}
