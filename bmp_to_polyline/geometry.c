#include "geometry.h"

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
            int len = i - start;

            if (len > 2 && segCount < maxSegments)
            {
                segments[segCount].pts = &path[start];
                segments[segCount].count = len;
                segments[segCount].avgCurvature = sumK / kCount;
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
