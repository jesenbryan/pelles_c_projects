/****************************************************************************
 *                                                                          *
 * File    : main.c                                                         *
 *                                                                          *
 * Purpose : Console mode (command line) program.                           *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

#include <stdio.h>

#include "bmp.h"
#include "extract.h"
#include "utils.h"

/****************************************************************************
 *                                                                          *
 * Function: main                                                           *
 *                                                                          *
 * Purpose : Main entry point.                                              *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

int main(int argc, char *argv[])
{
    printf("Hello, world!\n");
	printf("Console initialized!\n");

	printf("A\n");

	Image* img = loadBMP("Untitled.bmp");

	printf("B\n");

	Path path = extractPath(img);

	printf("C\n");

	printf("Points: %d\n", path.count);
    return 0;
}

