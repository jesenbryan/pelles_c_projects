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

Point jointToWorld(Point local, Point pivot, float jointAngle, Point center, float bodyAngle)
{
    Point afterJoint = rotatePoint(local, pivot, jointAngle);
    return rotatePoint(afterJoint, center, bodyAngle);
}

Point nestedJointToWorld(Point local, Point innerPivot, float innerAngle, Point outerPivot, float outerAngle, Point center, float bodyAngle)
{
    Point afterInner = rotatePoint(local, innerPivot, innerAngle);
    return jointToWorld(afterInner, outerPivot, outerAngle, center, bodyAngle);
}

Point constrainToAxis(Point pivot, Point axisRef, Point target, float minLength)
{
    Point dir = { axisRef.x - pivot.x, axisRef.y - pivot.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return axisRef; // degenerate axis (zero-length limb) -- nothing to project onto

    dir.x /= len;
    dir.y /= len;

    Point toTarget = { target.x - pivot.x, target.y - pivot.y };
    float proj = toTarget.x * dir.x + toTarget.y * dir.y;

    if (proj < minLength)
        proj = minLength;

    Point result = { pivot.x + dir.x * proj, pivot.y + dir.y * proj };
    return result;
}

Point recenterOnAxis(Point p, Point axisStart, Point axisEnd)
{
    Point dir = { axisEnd.x - axisStart.x, axisEnd.y - axisStart.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return p; // degenerate axis -- nothing sensible to recenter onto

    dir.x /= len;
    dir.y /= len;

    // rotate dir by 90 degrees to get the sideways direction
    Point perp = { -dir.y, dir.x };

    Point toP = { p.x - axisStart.x, p.y - axisStart.y };
    float perpOffset = toP.x * perp.x + toP.y * perp.y;

    Point mid = { axisStart.x + dir.x * (len * 0.5f), axisStart.y + dir.y * (len * 0.5f) };

    Point result = { mid.x + perp.x * perpOffset, mid.y + perp.y * perpOffset };
    return result;
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

// Finds the circle that passes through all three points.
// Used to draw an arc through p0, p1, p2 instead of a bezier curve.
Circle circumcircle(Point p0, Point p1, Point p2)
{
    Circle c;

    float ax = p0.x, ay = p0.y;
    float bx = p1.x, by = p1.y;
    float cx = p2.x, cy = p2.y;

    float d = 2.0f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));

    if (fabsf(d) < 1e-6f)
    {
        // points are (nearly) collinear -- no well-defined circle
        c.center.x = 0.0f;
        c.center.y = 0.0f;
        c.radius = 0.0f;
        c.valid = 0;
        return c;
    }

    float aSq = ax * ax + ay * ay;
    float bSq = bx * bx + by * by;
    float cSq = cx * cx + cy * cy;

    c.center.x = (aSq * (by - cy) + bSq * (cy - ay) + cSq * (ay - by)) / d;
    c.center.y = (aSq * (cx - bx) + bSq * (ax - cx) + cSq * (bx - ax)) / d;

    float dx = ax - c.center.x;
    float dy = ay - c.center.y;
    c.radius = sqrtf(dx * dx + dy * dy);
    c.valid = 1;

    return c;
}
