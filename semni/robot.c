#include "robot.h"

Point getCenter(Semni b)
{
    Point c;
    c.x = (b.leftX + b.rightX) * 0.5f;
    c.y = b.y;
    return c;
}
