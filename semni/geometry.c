#include "geometry.h"
#include <math.h>

Point rotatePoint(Point p, Point center, float angleDeg)
{
    float a = angleDeg * 3.1415926f / 180.0f;

    float s = sinf(a);
    float c = cosf(a);

    // translate to origin
    p.x -= center.x;
    p.y -= center.y;

    // rotate
    float xnew = p.x * c - p.y * s;
    float ynew = p.x * s + p.y * c;

    // translate back
    p.x = xnew + center.x;
    p.y = ynew + center.y;

    return p;
}

Point inverseRotate(Point p, Point center, float angleDeg)
{
    float a = -angleDeg * 3.1415926f / 180.0f;

    float s = sinf(a);
    float c = cosf(a);

    p.x -= center.x;
    p.y -= center.y;

    float xnew = p.x * c - p.y * s;
    float ynew = p.x * s + p.y * c;

    p.x = xnew + center.x;
    p.y = ynew + center.y;

    return p;
}

Point circleEdge(Point center, float radius, float angleDeg)
{
    float a = angleDeg * 3.1415926f / 180.0f;

    Point p;
    p.x = center.x + cosf(a) * radius;
    p.y = center.y + sinf(a) * radius;

    return p;
}

int isNear(Point a, Point b, float radius)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return (dx*dx + dy*dy) < (radius * radius);
}

void buildBezierSegment2(RobotSegment* seg, Point p0, Point p1, Point p2)
{
    seg->vertexCount = 0;

    for (float t = 0; t <= 1.0f; t += 0.02f)
    {
        float u = 1 - t;

        Point p;

        p.x =
            u*u*p0.x +
            2*u*t*p1.x +
            t*t*p2.x;

        p.y =
            u*u*p0.y +
            2*u*t*p1.y +
            t*t*p2.y;

        seg->vertices[seg->vertexCount++] = p;
    }
}
