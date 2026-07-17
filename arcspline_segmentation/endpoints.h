#pragma once

#include <stdio.h>
#include <stdint.h>

int find_start_end_pixels(uint8_t* bin, int w, int h, int* sx, int* sy, int* ex, int* ey);
