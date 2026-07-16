#include "geometry.h"
#include <math.h>
#include <string.h>

// ============================================================================
// SEMNI ROBOT GEOMETRY FUNCTIONS
// ============================================================================

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

Fillet filletFromAttachAngleConcave(Point c1, float r1, Point c2, float r2, float angleDeg, float minRadius, float maxRadius)
{
    float a = angleDeg * 3.1415926f / 180.0f;
    Point u = { cosf(a), sinf(a) };

    // same tangent-point-fixes-the-center setup as filletFromAttachAngle
    // (C = c1 + k*u), but external tangency to c1 means k = r1 + R instead
    // of k = r1 - R. Plugging that into the second tangency constraint
    // (|C - c2| == R + r2, also external) and expanding turns out to give
    // the EXACT same k formula as the convex/internal case -- only the
    // k-to-radius relationship at the end differs.
    Point V = { c1.x - c2.x, c1.y - c2.y };
    float VdotU = V.x * u.x + V.y * u.y;
    float VmagSq = V.x * V.x + V.y * V.y;

    float denom = 2.0f * (VdotU + (r1 - r2));

    float radius;

    if (fabsf(denom) < 1e-4f)
    {
        // same singularity as the convex case -- this attach angle points
        // at the common tangent LINE, where any fillet (concave or
        // convex) has infinite radius. Clamp instead of dividing by ~0.
        radius = maxRadius;
    }
    else
    {
        float k = ((r1 - r2) * (r1 - r2) - VmagSq) / denom;
        radius = r1 + k; // only difference from convex case
    }

    if (radius < minRadius) radius = minRadius;
    if (radius > maxRadius) radius = maxRadius;

    // recompute k from the (possibly clamped) radius
    float k = radius - r1; // mirror of the convex case

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

SafeAngleRange filletSafeAngleRangeConcave(Point c1, float r1, Point c2, float r2, float maxRadius)
{
    Point V = { c1.x - c2.x, c1.y - c2.y };
    float dist = sqrtf(V.x * V.x + V.y * V.y);

    SafeAngleRange range;

    if (dist < 1e-6f)
    {
        range.centerDeg = 0.0f;
        range.halfWidthDeg = 180.0f;
        return range;
    }

    float convexCenterDeg = atan2f(V.y, V.x) * 180.0f / 3.1415926f;

    range.centerDeg = convexCenterDeg + 180.0f;
    while (range.centerDeg > 180.0f) range.centerDeg -= 360.0f;
    while (range.centerDeg < -180.0f) range.centerDeg += 360.0f;

    // boundary 1: same common-tangent-line singularity as
    // filletSafeAngleRange, just re-measured from the concave center
    float ratioSing = (r2 - r1) / dist;
    if (ratioSing < -1.0f) ratioSing = -1.0f;
    if (ratioSing > 1.0f) ratioSing = 1.0f;
    float halfSingFromConvexCenter = acosf(ratioSing) * 180.0f / 3.1415926f;
    float half = 180.0f - halfSingFromConvexCenter;

    // boundary 2 (usually the tighter one): where the concave radius
    // would exactly equal maxRadius. Same closed-form as
    // filletFromAttachAngleConcave, solved for the angle instead of the
    // radius -- k0 here is r1 + maxRadius (the concave k-to-radius
    // relation), the mirror image of filletSafeAngleRange's k0.
    float k0 = r1 + maxRadius;
    if (fabsf(k0) > 1e-6f)
    {
        float VmagSq = dist * dist;
        float rhs = ((r1 - r2) * (r1 - r2) - VmagSq) / (2.0f * k0) - (r1 - r2);
        float ratioCap = rhs / dist;

        if (ratioCap >= -1.0f && ratioCap <= 1.0f)
        {
            float halfCapFromConvexCenter = acosf(ratioCap) * 180.0f / 3.1415926f;
            float halfCap = 180.0f - halfCapFromConvexCenter;
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

Circle_Semni circumcircle(Point p0, Point p1, Point p2)
{
    Circle_Semni c;

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

// ============================================================================
// ARC-SPLINE SEGMENTATION FUNCTIONS
// ============================================================================

// 2D cross product (AB x BC)
static double cross(Point a, Point b, Point c)
{
    double abx = b.x - a.x;
    double aby = b.y - a.y;

    double bcx = c.x - b.x;
    double bcy = c.y - b.y;

    return abx * bcy - aby * bcx;
}

// distance between points
static double dist(Point p1, Point p2)
{
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    return sqrt(dx*dx + dy*dy);
}

// curvature from 3 points
double curvature(Point a, Point b, Point c)
{
    double ab = dist(a, b);
    double bc = dist(b, c);
    double ca = dist(c, a);

    double denom = ab * bc * ca;

    if (denom == 0.0)
        return 0.0;

    double area2 = cross(a, b, c); // actually 2x signed triangle area

    return (2.0 * fabs(area2)) / denom;
}

int resamplePath(Point* in, int n, Point* out, double step)
{
    if (n < 2) return 0;

    int m = 0;

    double accDist = 0.0;
    out[m++] = in[0];

    double target = step;

    for (int i = 1; i < n; i++)
    {
        Point p0 = in[i - 1];
        Point p1 = in[i];

        double segLen = dist(p0, p1);

        while (accDist + segLen >= target)
        {
            double t = (target - accDist) / segLen;

            Point q;
            q.x = (int)round(p0.x + t * (p1.x - p0.x));
            q.y = (int)round(p0.y + t * (p1.y - p0.y));

            out[m++] = q;

            target += step;
        }

        accDist += segLen;
    }

    return m;
}

int segmentPath(Point* path, int n, int* segmentId, double threshold)
{
    if (n < 3) return 0;

    int seg = 0;

    double prevKappa = curvature(path[0], path[1], path[2]);
    segmentId[1] = seg;

    for (int i = 2; i < n - 1; i++)
    {
        double kappa = curvature(path[i-1], path[i], path[i+1]);

        double diff = fabs(kappa - prevKappa);

        if (diff > threshold)
        {
            seg++; // new arc segment
        }

        segmentId[i] = seg;
        prevKappa = kappa;
    }

    return seg + 1;
}

int segmentArcs(Point* path, int n, double tolerance, ArcSegment* segments, int maxSegments)
{
    int segCount = 0;
    int start = 0;
    double prevK = -1.0;
    double sumK = 0.0;
    int kCount = 0;

    for (int i = 1; i < n - 1; i++)
    {
        double k = curvature(path[i-1], path[i], path[i+1]);

        if (isnan(k))
            continue;

        if (prevK < 0)
        {
            prevK = k;
            sumK = k;
            kCount = 1;
            continue;
        }

        double diff = fabs(k - prevK);

        // segment break condition
        if (diff > tolerance)
        {
            int len = n - start;
            if (len > 2 && segCount < maxSegments) {
                segments[segCount].pts = &path[start];
                segments[segCount].count = len;
                segments[segCount].avgCurvature = (kCount > 0) ? sumK / kCount : 0.0;
                segCount++;
            }

            // start new segment
            start = i;
            sumK = k;
            kCount = 1;
        }
        else
        {
            sumK += k;
            kCount++;
        }

        prevK = k;
    }

    return segCount;
}

#define ARC_TOLERANCE 0.02

static double det3(double m[3][3])
{
    return m[0][0]*(m[1][1]*m[2][2] - m[1][2]*m[2][1])
         - m[0][1]*(m[1][0]*m[2][2] - m[1][2]*m[2][0])
         + m[0][2]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]);
}

// Algebraic (Kasa) least-squares circle fit.
// Fits x^2 + y^2 + B1*x + B2*y + B3 = 0, then recovers center/radius.
// Returns 0 if the points are (near) collinear - no valid circle.
int fitCircleToPoints(Point* pts, int n, Circle* out)
{
    if (n < 3) return 0;

    double Sx = 0, Sy = 0, Sxx = 0, Syy = 0, Sxy = 0;
    double Sxxx = 0, Syyy = 0, Sxyy = 0, Sxxy = 0;

    for (int i = 0; i < n; i++) {
        double x = pts[i].x;
        double y = pts[i].y;
        Sx += x;  Sy += y;
        Sxx += x * x;  Syy += y * y;  Sxy += x * y;
        Sxxx += x * x * x;  Syyy += y * y * y;
        Sxyy += x * y * y;  Sxxy += x * x * y;
    }

    double M[3][3] = {
        { Sxx, Sxy, Sx },
        { Sxy, Syy, Sy },
        { Sx,  Sy,  (double)n }
    };
    double rhs[3] = {
        -(Sxxx + Sxyy),
        -(Sxxy + Syyy),
        -(Sxx + Syy)
    };

    double detM = det3(M);
    if (fabs(detM) < 1e-9) return 0; // degenerate / collinear

    double M1[3][3], M2[3][3], M3[3][3];
    memcpy(M1, M, sizeof(M));
    memcpy(M2, M, sizeof(M));
    memcpy(M3, M, sizeof(M));

    for (int i = 0; i < 3; i++) M1[i][0] = rhs[i];
    for (int i = 0; i < 3; i++) M2[i][1] = rhs[i];
    for (int i = 0; i < 3; i++) M3[i][2] = rhs[i];

    double B1 = det3(M1) / detM;
    double B2 = det3(M2) / detM;
    double B3 = det3(M3) / detM;

    double cx = -B1 / 2.0;
    double cy = -B2 / 2.0;
    double r2 = cx * cx + cy * cy - B3;

    if (r2 < 0) return 0;

    out->cx = (float)cx;
    out->cy = (float)cy;
    out->r  = (float)sqrt(r2);
    return 1;
}

#define ARC_FIT_TOLERANCE   2.0    // max allowed deviation, in pixels
#define MIN_POINTS_FOR_ARC  5      // runs shorter than this are always lines

static void pushLineSegment(Point* pts, int n, ArcSegment* out, int* segCount)
{
    out[*segCount].pts          = pts;
    out[*segCount].count        = n;
    out[*segCount].circle.cx    = 0.0f;
    out[*segCount].circle.cy    = 0.0f;
    out[*segCount].circle.r     = 0.0f;
    out[*segCount].avgCurvature = 0.0;
    (*segCount)++;
}

static double maxCircleDeviation(Point* pts, int n, Circle c)
{
    double maxErr = 0.0;
    for (int i = 0; i < n; i++) {
        double dx = pts[i].x - c.cx;
        double dy = pts[i].y - c.cy;
        double d = fabs(sqrt(dx * dx + dy * dy) - c.r);
        if (d > maxErr) maxErr = d;
    }
    return maxErr;
}

static int recursiveArcFit(Point* pts, int n, ArcSegment* out, int maxSegments, int* segCount)
{
    if (n < 2) return 1;
    if (*segCount >= maxSegments) return 0;

    if (n < MIN_POINTS_FOR_ARC) {
        pushLineSegment(pts, n, out, segCount);
        return 1;
    }

    Circle c = {0};
    int ok = fitCircleToPoints(pts, n, &c);
    int radiusReasonable = ok && c.r < 100000.0f;
    double err = (ok && radiusReasonable) ? maxCircleDeviation(pts, n, c) : 1e18;

    if (ok && radiusReasonable && err <= ARC_FIT_TOLERANCE) {
        out[*segCount].pts          = pts;
        out[*segCount].count        = n;
        out[*segCount].circle       = c;
        out[*segCount].avgCurvature = (c.r > 1e-6f) ? (1.0 / c.r) : 0.0;
        (*segCount)++;
        return 1;
    }

    if (n <= MIN_POINTS_FOR_ARC + 1) {
        pushLineSegment(pts, n, out, segCount);
        return 1;
    }

    int mid = n / 2;
    if (!recursiveArcFit(pts, mid + 1, out, maxSegments, segCount)) return 0;
    if (!recursiveArcFit(pts + mid, n - mid, out, maxSegments, segCount)) return 0;
    return 1;
}

int buildSegments(Point* path, int n, ArcSegment* out)
{
    int segCount = 0;
    recursiveArcFit(path, n, out, MAX_ARC_SEGMENTS, &segCount);

    // Merge consecutive straight segments
    int mergedCount = 0;
    for (int i = 0; i < segCount; )
    {
        if (out[i].circle.r == 0.0f)
        {
            int j = i;
            while (j + 1 < segCount && out[j + 1].circle.r == 0.0f) j++;

            if (j > i)
            {
                Point* mergedPts   = out[i].pts;
                int    mergedPtCount = (int)(out[j].pts - out[i].pts) + out[j].count;

                out[mergedCount].pts          = mergedPts;
                out[mergedCount].count        = mergedPtCount;
                out[mergedCount].circle.cx    = 0.0f;
                out[mergedCount].circle.cy    = 0.0f;
                out[mergedCount].circle.r     = 0.0f;
                out[mergedCount].avgCurvature = 0.0;
                mergedCount++;
                i = j + 1;
                continue;
            }
        }

        if (mergedCount != i) out[mergedCount] = out[i];
        mergedCount++;
        i++;
    }
    segCount = mergedCount;

    for (int i = 0; i < segCount; i++)
    {
        out[i].start = (int)(out[i].pts - path);
        out[i].end   = out[i].start + out[i].count - 1;
    }

    return segCount;
}
