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

float perpOffsetOnAxis(Point p, Point axisStart, Point axisEnd)
{
    Point dir = { axisEnd.x - axisStart.x, axisEnd.y - axisStart.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return 0.0f; // degenerate axis -- no sideways direction to measure against

    dir.x /= len;
    dir.y /= len;

    // rotate dir by 90 degrees to get the sideways direction
    Point perp = { -dir.y, dir.x };

    Point toP = { p.x - axisStart.x, p.y - axisStart.y };
    return toP.x * perp.x + toP.y * perp.y;
}

Point recenterOnAxis(Point p, Point axisStart, Point axisEnd)
{
    Point dir = { axisEnd.x - axisStart.x, axisEnd.y - axisStart.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return p; // degenerate axis -- nothing sensible to recenter onto

    dir.x /= len;
    dir.y /= len;

    Point perp = { -dir.y, dir.x };
    float perpOffset = perpOffsetOnAxis(p, axisStart, axisEnd);

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

Point circleTowardPoint(Point center, float radius, Point target)
{
    Point d = { target.x - center.x, target.y - center.y };
    float dist = sqrtf(d.x * d.x + d.y * d.y);

    if (dist < 1e-6f)
        return (Point){ center.x + radius, center.y }; // degenerate -- target sits on the center, direction is arbitrary

    Point u = { d.x / dist, d.y / dist };
    return (Point){ center.x + u.x * radius, center.y + u.y * radius };
}

Point internalTangentPoint(Point c1, float r1, Point c2, float r2)
{
    // whichever circle has the bigger radius is the one "containing" the
    // other -- the tangent point sits on the containing circle's boundary,
    // in the direction of the smaller circle's center
    if (r1 >= r2)
        return circleTowardPoint(c1, r1, c2);
    else
        return circleTowardPoint(c2, r2, c1);
}

Fillet filletFromAttachAngle(Point c1, float r1, Point c2, float r2, float angleDeg, float minRadius, float maxRadius)
{
    float a = angleDeg * 3.1415926f / 180.0f;
    Point u = { cosf(a), sinf(a) };

    // Derivation: the fillet's center C must lie on the line through c1
    // in direction u (since C, c1, and the c1-tangent-point are always
    // colinear for tangent circles), i.e. C = c1 + k*u for some scalar k,
    // where k = r1 - R (this single relation satisfies both
    // |C - c1| == |R - r1| and the tangent point itself falling exactly
    // at c1 + r1*u, for any R -- it's just the definition of "tangent to
    // c1 at this angle"). Plugging that into the second tangency
    // constraint |C - c2| == |R - r2| and expanding, the R^2 terms cancel
    // and what's left is linear in k -- no quadratic, no iteration.
    Point V = { c1.x - c2.x, c1.y - c2.y };
    float VdotU = V.x * u.x + V.y * u.y;
    float VmagSq = V.x * V.x + V.y * V.y;

    float denom = 2.0f * (VdotU + (r1 - r2));

    float radius;

    if (fabsf(denom) < 1e-4f)
    {
        // this attach angle points straight at the exact common tangent
        // LINE between c1 and c2 -- the fillet that would pass through
        // here has infinite radius. Clamp instead of dividing by ~0.
        radius = maxRadius;
    }
    else
    {
        float k = ((r1 - r2) * (r1 - r2) - VmagSq) / denom;
        radius = r1 - k;
    }

    if (radius < minRadius) radius = minRadius;
    if (radius > maxRadius) radius = maxRadius;

    // recompute k from the (possibly clamped) radius, so the returned
    // circle is always exactly tangent to c1 at angleDeg -- even past the
    // edge of the valid range, where tangency to c2 becomes approximate
    // instead
    float k = r1 - radius;

    Fillet result;
    result.center = (Point){ c1.x + k * u.x, c1.y + k * u.y };
    result.radius = radius;
    return result;
}

SafeAngleRange filletSafeAngleRange(Point c1, float r1, Point c2, float r2, float maxRadius)
{
    Point V = { c1.x - c2.x, c1.y - c2.y };
    float dist = sqrtf(V.x * V.x + V.y * V.y);

    SafeAngleRange range;

    if (dist < 1e-6f)
    {
        // degenerate -- circles share a center, every angle is equally
        // (in)valid. Report the whole circle as safe rather than divide
        // by zero.
        range.centerDeg = 0.0f;
        range.halfWidthDeg = 180.0f;
        return range;
    }

    range.centerDeg = atan2f(V.y, V.x) * 180.0f / 3.1415926f;

    // boundary 1: where the fillet radius would blow up to infinity (the
    // true common tangent LINE between c1 and c2) -- same condition as the
    // |denom| < eps check in filletFromAttachAngle
    float ratioSing = (r2 - r1) / dist;
    if (ratioSing < -1.0f) ratioSing = -1.0f;
    if (ratioSing > 1.0f) ratioSing = 1.0f;
    float half = acosf(ratioSing) * 180.0f / 3.1415926f;

    // boundary 2 (usually the tighter one): where the radius would exactly
    // equal maxRadius. Same closed-form as filletFromAttachAngle, just
    // solved for the angle instead of the radius -- plug R = maxRadius in
    // for k, then back out what cos(angle - centerDeg) would have to be.
    float k0 = r1 - maxRadius;
    if (fabsf(k0) > 1e-6f)
    {
        float VmagSq = dist * dist;
        float rhs = ((r1 - r2) * (r1 - r2) - VmagSq) / (2.0f * k0) - (r1 - r2);
        float ratioCap = rhs / dist;

        if (ratioCap >= -1.0f && ratioCap <= 1.0f)
        {
            float halfCap = acosf(ratioCap) * 180.0f / 3.1415926f;
            if (halfCap < half)
                half = halfCap;
        }
    }

    range.halfWidthDeg = half;
    return range;
}

float clampToSafeAngleRange(float angleDeg, SafeAngleRange range, float marginDeg)
{
    float delta = angleDeg - range.centerDeg;

    while (delta > 180.0f) delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;

    float maxDelta = range.halfWidthDeg - marginDeg;
    if (maxDelta < 0.0f) maxDelta = 0.0f;

    if (delta > maxDelta) delta = maxDelta;
    if (delta < -maxDelta) delta = -maxDelta;

    return range.centerDeg + delta;
}

Point filletBulgePoint(Point c1, float r1, Point c2, float r2, float angleDeg, float minRadius, float maxRadius, Point target)
{
    Fillet f = filletFromAttachAngle(c1, r1, c2, r2, angleDeg, minRadius, maxRadius);
    return circleTowardPoint(f.center, f.radius, target);
}

Point circleAtX(Point center, float radius, float targetX, Point preferNear)
{
    float dx = targetX - center.x;
    float remSq = radius * radius - dx * dx;
    if (remSq < 0.0f) remSq = 0.0f; // targetX is outside the circle's horizontal reach -- fall back to the closest approach

    float dy = sqrtf(remSq);

    Point p1 = { targetX, center.y + dy };
    Point p2 = { targetX, center.y - dy };

    float d1 = (p1.y - preferNear.y) * (p1.y - preferNear.y);
    float d2 = (p2.y - preferNear.y) * (p2.y - preferNear.y);

    return (d1 <= d2) ? p1 : p2;
}

Point circleAtAxisMid(Point center, float radius, Point axisStart, Point axisEnd, Point preferNear)
{
    Point dir = { axisEnd.x - axisStart.x, axisEnd.y - axisStart.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return circleTowardPoint(center, radius, preferNear); // degenerate axis -- no line to intersect

    dir.x /= len;
    dir.y /= len;

    Point perp = { -dir.y, dir.x };
    Point axisMid = { axisStart.x + dir.x * (len * 0.5f), axisStart.y + dir.y * (len * 0.5f) };

    // solve |axisMid + t*perp - center|^2 == radius^2 for t (perp is a
    // unit vector, so this is a plain quadratic in t with no cross term
    // beyond the linear one)
    Point d = { axisMid.x - center.x, axisMid.y - center.y };
    float dDotPerp = d.x * perp.x + d.y * perp.y;
    float dMagSq = d.x * d.x + d.y * d.y;

    float disc = dDotPerp * dDotPerp - (dMagSq - radius * radius);
    if (disc < 0.0f) disc = 0.0f; // axis-mid line doesn't reach the circle -- fall back to closest approach

    float sq = sqrtf(disc);
    float t1 = -dDotPerp + sq;
    float t2 = -dDotPerp - sq;

    Point p1 = { axisMid.x + perp.x * t1, axisMid.y + perp.y * t1 };
    Point p2 = { axisMid.x + perp.x * t2, axisMid.y + perp.y * t2 };

    float dist1 = (p1.x - preferNear.x) * (p1.x - preferNear.x) + (p1.y - preferNear.y) * (p1.y - preferNear.y);
    float dist2 = (p2.x - preferNear.x) * (p2.x - preferNear.x) + (p2.y - preferNear.y) * (p2.y - preferNear.y);

    return (dist1 <= dist2) ? p1 : p2;
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
