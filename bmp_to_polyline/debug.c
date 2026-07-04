// 1. Tell the compiler we want secure standard functions (like freopen_s)
#define __STDC_WANT_LIB_EXT1__ 1

#include <stdio.h>
#include <stdlib.h>

// 2. Ensure the core Windows API header is present for AllocConsole and SetConsoleTitleA
#include <windows.h> 

#include "debug.h"

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

void OpenConsole() {
    // 1. Create the console window
    AllocConsole();
    
    // 2. Redirect standard streams to the new console
    // "CONOUT$" is the system device for the console output
    // "CONIN$" is the system device for console input
    
    // Redirect stdout to console
    freopen("CONOUT$", "w", stdout);
    
    // Redirect stderr to console
    freopen("CONOUT$", "w", stderr);
    
    // Redirect stdin to console
    freopen("CONIN$", "r", stdin);

    // 3. Disable buffering for immediate output
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    SetConsoleTitle("Debug Console");
}

void debugPrintPath(Point* path, int numPoints) {
    printf("\n[DEBUG] Path Analysis: %d points identified.\n", numPoints);

    if (numPoints <= 0 || path == NULL) {
        printf("  -> Error: Path data invalid or empty.\n\n");
        return;
    }

    // Using field width specifiers:
    // %5d: Pad to 5 spaces (index)
    // %6d: Pad to 6 spaces (x coord)
    // %6d: Pad to 6 spaces (y coord)
    printf("  Index |     X |     Y\n");
    printf("-------------------------\n");

    for (int i = 0; i < numPoints; i++) {
        // Logic check: only print start, end, or increments of 100
        if (i == 0 || (i % 100 == 0) || (i == numPoints - 1)) {
            printf("  %5d | %5d | %5d\n", i, path[i].x, path[i].y);
        }
    }

    printf("-------------------------\n");
    printf("[DEBUG] End of trace.\n\n");
}
