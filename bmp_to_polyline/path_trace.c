#include "path_trace.h"

int tracePath(
    uint8_t *bin,
    int width,
    int height,
    int sx,
    int sy,
    int ex,
    int ey,
    Point path[],
    int maxPoints)
{
    // N, E, S, W, NE, SE, SW, NW
    const int dx[8] = { 0, 1, 0,-1, 1, 1,-1,-1 };
    const int dy[8] = {-1, 0, 1, 0,-1, 1, 1,-1 };

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

        for (int k = 0; k < 8; k++)
        {
            int nx = x + dx[k];
            int ny = y + dy[k];

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

        if (!found)
            break;
    }

    free(visited);

    return count;
}
