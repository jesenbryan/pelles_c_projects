#include "path_trace.h"

typedef struct {
    int dx;
    int dy;
} Offset2D;

// Clockwise spatial array mapping: N, NE, E, SE, S, SW, W, NW
static const Offset2D NEIGHBORS[8] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1},
    {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
};

int tracePath(uint8_t *bin, int width, int height, int sx, int sy, int ex, int ey, Point path[], int maxPoints)
{
    // N, E, S, W, NE, SE, SW, NW

    uint8_t *visited = calloc(width * height, sizeof(uint8_t));
    if (visited == NULL)
        return 0;

    int x = sx;
    int y = sy;
    int count = 0;

    while (1)
    {
        if (count >= maxPoints)
            break;

        path[count].x = x;
        path[count].y = y;
        count++;

        visited[y * width + x] = 1;

        if (x == ex && y == ey)
            break;

        int found = 0;

        // First try 8-connected neighbors
        for (int k = 0; k < 8; k++)
        {
            int nx = x + NEIGHBORS[k].dx;
            int ny = y + NEIGHBORS[k].dy;

            if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                continue;

            int idx = ny * width + nx;

            if (bin[idx] && !visited[idx])
            {
                x = nx;
                y = ny;
                found = 1;
                break;
            }
        }

        if (found) continue;

        // NEW: If no 8-connected neighbor found, search for nearby pixels to bridge small gaps
        // (gaps created by the thinning algorithm in nearly-closed curves like circles)
        int foundBridge = 0;
        for (int dy = -3; dy <= 3 && !foundBridge; dy++) {
            for (int dx = -3; dx <= 3 && !foundBridge; dx++) {
                if (dx == 0 && dy == 0) continue;  // skip current position
                
                int nx = x + dx;
                int ny = y + dy;

                if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                    continue;

                int idx = ny * width + nx;

                if (bin[idx] && !visited[idx])
                {
                    x = nx;
                    y = ny;
                    foundBridge = 1;
                }
            }
        }

        if (!foundBridge)
            break;
    }

    free(visited);

    return count;
}
