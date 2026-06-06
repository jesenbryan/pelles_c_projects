#include "robot.h"

Point getCenter(CapsuleBody b)
{
    Point c;
    c.x = (b.leftX + b.rightX) * 0.5f;
    c.y = b.y;
    return c;
}
