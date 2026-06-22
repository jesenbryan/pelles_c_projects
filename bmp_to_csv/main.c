#include <stdio.h>
#include <windows.h>
#include "file_dialogs.h"
#include "bmp_io.h"

int main(void)
{
    char bmpPath[MAX_PATH];
    char csvPath[MAX_PATH];

    openFileDialogOrExit(bmpPath, MAX_PATH);
	saveFileDialogOrExit(csvPath, MAX_PATH);

	if (bmp_to_csv(bmpPath, csvPath)) {
	    printf("Success\n");
	    printf("BMP: %s\n", bmpPath);
	    printf("CSV: %s\n", csvPath);
	} else {
	    printf("Failed\n");
	}

    return 0;
}
