#include "app.h"
#include "config.h"

#include "paint.h"

void addBreak()
{
    if (app.paintCount + 2 < MAX_POINTS)
    {
        app.paintPoints[app.paintCount++] = BREAK_POINT_X;
        app.paintPoints[app.paintCount++] = BREAK_POINT_Y;
    }
}
