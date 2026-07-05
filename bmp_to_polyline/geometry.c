#include "geometry.h"
#include <string.h>  

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
            q.x = p0.x + t * (p1.x - p0.x);
            q.y = p0.y + t * (p1.y - p0.y);

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

        // 🔥 segment break condition
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

// --- Recursive "arc spline" fitting (ported from draw_to_arcspline.c) ---
//
// Instead of splitting the path wherever local curvature jumps (the old
// segmentArcs approach above), this tries to fit ONE circle through an
// entire run of points, measures how far the actual points stray from
// that circle, and only bisects the run if the fit is worse than
// ARC_FIT_TOLERANCE pixels. This tends to produce fewer, more honest
// arcs than a fixed curvature-diff threshold.

#define ARC_FIT_TOLERANCE   2.0    // max allowed deviation, in pixels
#define MIN_POINTS_FOR_ARC  5      // runs shorter than this are always lines

// r == 0 is the existing convention (see canvas_bridge.c's sampleArcPoints)
// for "no meaningful circle here, treat as a straight segment".
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

// Returns 0 once maxSegments has been hit, so the caller stops recursing.
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
        // Too short to usefully bisect further - fall back to a line.
        pushLineSegment(pts, n, out, segCount);
        return 1;
    }

    int mid = n / 2;
    // Shared joint point between the two halves, same as draw_to_arcspline.c
    if (!recursiveArcFit(pts, mid + 1, out, maxSegments, segCount)) return 0;
    if (!recursiveArcFit(pts + mid, n - mid, out, maxSegments, segCount)) return 0;
    return 1;
}

int buildSegments(Point* path, int n, ArcSegment* out)
{
    int segCount = 0;
    recursiveArcFit(path, n, out, MAX_ARC_SEGMENTS, &segCount);

    for (int i = 0; i < segCount; i++)
    {
        out[i].start = (int)(out[i].pts - path);
        out[i].end   = out[i].start + out[i].count - 1;
    }

    return segCount;
}
