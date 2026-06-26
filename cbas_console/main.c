#include <stdio.h>

#include "bmp.h"
#include "extract.h"
#include "utils.h"

int main(int argc, char *argv[])
{
	printf("Console initialized!\n");

	printf("A\n");

	Image* img = loadBMP("ver.bmp");

	printf("B\n");

	Path path = extractPath(img);

	printf("Points: %d\n", path.count);

	savePathCSV("ver.csv", path);

    return 0;
}

