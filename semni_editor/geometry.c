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

// Max perpendicular deviation of pts[] from the straight chord connecting
// pts[0] to pts[n-1] -- the "is this run of points already a line?" test,
// checked BEFORE ever attempting a circle fit. fitCircleToPoints is an
// unconstrained algebraic (Kasa) fit: handed a handful of pixels with
// ordinary tracing/thinning noise, it will always return SOME circle, and
// that circle's residual (maxCircleDeviation) can easily land under
// ARC_FIT_TOLERANCE even when the points are, for all practical purposes,
// straight -- silently turning one straight stroke into a string of small
// separate arcs (each carrying its own ghost-circle overlay). Testing the
// simpler straight-line explanation first means a genuinely straight run
// never reaches the circle fit at all.
static double maxLineDeviation(Point* pts, int n)
{
    double x0 = pts[0].x, y0 = pts[0].y;
    double x1 = pts[n - 1].x, y1 = pts[n - 1].y;
    double dx = x1 - x0, dy = y1 - y0;
    double len = sqrt(dx * dx + dy * dy);
    if (len < 1e-6) return 1e18;   // coincident endpoints - can't define a line, let the circle fit decide

    double maxErr = 0.0;
    for (int i = 0; i < n; i++) {
        double d = fabs(dy * (pts[i].x - x0) - dx * (pts[i].y - y0)) / len;
        if (d > maxErr) maxErr = d;
    }
    return maxErr;
}

// How far a circular arc of radius r spanning chord length `chordLen`
// bulges away from its own straight chord (the sagitta). Used to catch
// circle fits that technically pass maxCircleDeviation but describe a
// curve so shallow it's visually identical to a straight line at this same
// ARC_FIT_TOLERANCE -- e.g. a huge-radius circle quietly explaining a bit
// of S-shaped tracing noise the straight-line test above rejected. Rather
// than accept that as a "real" arc (another source of straight strokes
// rendering as strings of circles), this is checked against the SAME
// tolerance already used to accept the fit in the first place, so there's
// no new magic constant to tune.
static double arcSagitta(double chordLen, double r)
{
    double half = chordLen * 0.5;
    double inner = r * r - half * half;
    if (inner < 0.0) inner = 0.0;
    return r - sqrt(inner);
}

// --- Sharp-corner detection (run BEFORE recursiveArcFit, see buildSegments) ---
//
// A true corner (e.g. an L-shaped/right-angle bend in a hand-drawn shape)
// is neither a line nor a circle: a line can't bend, and a circle tight
// enough to explain a real corner over just a few pixels would have to be
// absurdly small. Left for recursiveArcFit's bisection to find on its own,
// the split that finally isolates it almost never lands exactly ON the
// corner pixel (bisection just cuts at n/2), so the small leaf segment
// that eventually gives up and falls back to a straight chord
// (MIN_POINTS_FOR_ARC) typically spans a few pixels on EACH side of the
// actual vertex -- cutting the corner off with a diagonal shortcut instead
// of reaching it, which is what shows up on screen as a small gap/notch
// right at a sharp corner. Detecting corners explicitly beforehand and
// forcing a hard split exactly AT the corner pixel (shared between both
// arms, same convention recursiveArcFit already uses at its own bisection
// midpoints) means neither arm's fit ever has to average across the
// vertex.
// NOTE: an earlier version of this detector tried "does neither a line NOR
// a circle fit this small window within ARC_FIT_TOLERANCE" -- it turns out
// a small enough circle (a couple pixels' radius) fits a real right-angle
// corner's own immediate neighborhood almost perfectly too (a corner IS the
// limit of a circle as its radius shrinks to zero), so that test simply
// never fired on a real corner at any window size small enough to still
// localize it precisely. What actually distinguishes a genuine corner from
// a genuine tight curve isn't the LOCAL fit residual, it's how the turning
// is distributed: a corner concentrates all of its direction change at one
// point (dead straight immediately either side of it), while a real circle
// -- however tight -- turns at close to the same steady rate everywhere
// along it. So this compares the turn angle AT a candidate point against
// the turn angle safely further along the path on either side: a large,
// ISOLATED spike is a corner; a turn angle that's just as large a bit
// further away too is ordinary (if tight) curvature.
// Values below were swept against synthetic test paths (see the buildSegments
// verification harness), not guessed: a noisy-but-straight line (independent
// +/-1..2px per-point jitter, well beyond what real tracing/thinning noise
// usually looks like) never scores above 0 with these settings, a genuine
// 90-degree corner scores 90, a 60-degree corner still scores correctly, a
// gentle 15-degree bend correctly scores below 0 (left for ordinary arc-
// fitting), and real circles from radius 10 to 80px never false-positive.
#define CORNER_PI            3.14159265358979323846
#define CORNER_HALF_WINDOW   8      // points sampled on each side, right at the candidate, to measure its own turn angle
#define CORNER_OFFSET        32     // how far away (each side) the "is this turning everywhere, or just here" comparison point sits
#define CORNER_ANGLE_MIN_DEG 55.0   // minimum turn (degrees) to even consider a point -- ordinary curve/raster noise stays well under this
#define CORNER_SPIKE_RATIO   2.0    // candidate's turn angle must exceed the neighboring comparison angle by at least this factor
#define CORNER_MERGE_WINDOW  (CORNER_HALF_WINDOW * 2)  // candidate corners this close together are the same corner
#define MAX_CORNERS          256

// Angle (degrees, 0..180) between the chord INTO pts[i] (from pts[i-hw])
// and the chord OUT OF pts[i] (to pts[i+hw]) -- 0 means dead straight
// through pts[i], larger means a sharper local turn. Returns -1 if the
// window falls outside the path.
static double turnAngleDeg(Point* pts, int n, int i, int hw)
{
    int back = i - hw, fwd = i + hw;
    if (back < 0 || fwd >= n) return -1.0;

    double ax = pts[i].x - pts[back].x, ay = pts[i].y - pts[back].y;
    double bx = pts[fwd].x - pts[i].x,  by = pts[fwd].y - pts[i].y;
    double la = sqrt(ax * ax + ay * ay);
    double lb = sqrt(bx * bx + by * by);
    if (la < 1e-6 || lb < 1e-6) return 0.0;

    double cosA = (ax * bx + ay * by) / (la * lb);
    if (cosA > 1.0) cosA = 1.0;
    if (cosA < -1.0) cosA = -1.0;
    return acos(cosA) * (180.0 / CORNER_PI);
}

// Returns pts[i]'s "corner sharpness" score (its own turn angle) if it
// passes both the minimum-angle and isolated-spike tests, or a negative
// value if it doesn't qualify as a corner at all. Higher score = sharper.
static double cornerScore(Point* pts, int n, int i)
{
    if (i - CORNER_OFFSET < 0 || i + CORNER_OFFSET >= n) return -1.0;

    double angC = turnAngleDeg(pts, n, i, CORNER_HALF_WINDOW);
    if (angC < CORNER_ANGLE_MIN_DEG) return -1.0;

    double angBefore = turnAngleDeg(pts, n, i - CORNER_OFFSET, CORNER_HALF_WINDOW);
    double angAfter  = turnAngleDeg(pts, n, i + CORNER_OFFSET, CORNER_HALF_WINDOW);
    double neighborMax = (angBefore > angAfter) ? angBefore : angAfter;
    if (neighborMax < 1e-6) neighborMax = 1e-6;

    if (angC <= neighborMax * CORNER_SPIKE_RATIO) return -1.0;
    return angC;
}

// Scans the whole path for corner candidates and collapses any cluster of
// adjacent candidates (a real corner's transition typically scores above
// threshold at several consecutive indices, not just one) down to
// whichever single index in that cluster scored sharpest -- that's the
// pixel closest to the actual vertex. Returns the count; indices are
// written in path order.
static int findCorners(Point* pts, int n, int* cornerIdx, int maxCorners)
{
    int count = 0;
    int pendingIdx = -1;
    double pendingScore = -1.0;

    for (int i = 0; i < n; i++)
    {
        double score = cornerScore(pts, n, i);
        if (score < 0.0) continue;

        if (pendingIdx >= 0 && (i - pendingIdx) <= CORNER_MERGE_WINDOW) {
            if (score > pendingScore) { pendingIdx = i; pendingScore = score; }
            continue;
        }

        if (pendingIdx >= 0 && count < maxCorners) cornerIdx[count++] = pendingIdx;
        pendingIdx = i;
        pendingScore = score;
    }

    if (pendingIdx >= 0 && count < maxCorners) cornerIdx[count++] = pendingIdx;
    return count;
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

    // Straight-line check FIRST (see maxLineDeviation) -- a run that's
    // already collinear within tolerance is reported as a line outright,
    // without ever risking a spurious circle fit.
    if (maxLineDeviation(pts, n) <= ARC_FIT_TOLERANCE) {
        pushLineSegment(pts, n, out, segCount);
        return 1;
    }

    Circle c = {0};
    int ok = fitCircleToPoints(pts, n, &c);
    int radiusReasonable = ok && c.r < 100000.0f;
    double err = (ok && radiusReasonable) ? maxCircleDeviation(pts, n, c) : 1e18;

    // A fit can pass err <= ARC_FIT_TOLERANCE yet still describe a curve so
    // shallow (sagitta below that same tolerance) it's indistinguishable
    // from straight -- reject it here too so it falls through to the
    // bisect-then-remerge path below instead of being drawn as its own arc.
    double chordLen = hypot((double)pts[n - 1].x - pts[0].x, (double)pts[n - 1].y - pts[0].y);
    int curvatureNegligible = ok && radiusReasonable &&
                               (arcSagitta(chordLen, (double)c.r) <= ARC_FIT_TOLERANCE);

    if (ok && radiusReasonable && err <= ARC_FIT_TOLERANCE && !curvatureNegligible) {
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

    // Split the path at any detected sharp corners FIRST, then run the
    // normal recursive line/arc fit independently on each corner-to-corner
    // piece (see findCorners above for why -- this is what stops a right-
    // angle bend from getting cut off by a leaf line segment that
    // straddles it). Each piece shares its boundary point with its
    // neighbor (pieceStart of one == pieceEndIdx of the previous), same
    // "shared joint" convention recursiveArcFit's own bisection uses, so
    // there's never a gap introduced at a corner split itself.
    int cornerIdx[MAX_CORNERS];
    int cornerCount = findCorners(path, n, cornerIdx, MAX_CORNERS);

    // Marks the last out[] index produced by each piece (except the very
    // last one) so the straight-line merge pass below can never fuse
    // segments from two DIFFERENT pieces back together across a real
    // corner -- doing so would draw one straight chord from the start of
    // one arm to the end of the next, cutting the actual vertex off again,
    // exactly the bug the corner split above exists to prevent.
    int noMergeAfter[MAX_ARC_SEGMENTS];
    memset(noMergeAfter, 0, sizeof(noMergeAfter));

    int pieceStart = 0;
    for (int c = 0; c <= cornerCount; c++)
    {
        int pieceEndIdx = (c < cornerCount) ? cornerIdx[c] : (n - 1);
        int pieceLen = pieceEndIdx - pieceStart + 1;

        if (pieceLen >= 2)
            recursiveArcFit(path + pieceStart, pieceLen, out, MAX_ARC_SEGMENTS, &segCount);

        if (c < cornerCount && segCount > 0 && segCount <= MAX_ARC_SEGMENTS)
            noMergeAfter[segCount - 1] = 1;

        pieceStart = pieceEndIdx;
    }

    // NEW: recursiveArcFit's bisection can chop one long straight run into
    // several small consecutive line segments - each individually correct
    // (they really are straight), just no longer combined into one. Since
    // consecutive entries in out[] are always in path order (recursiveArcFit
    // recurses left-half-then-right-half), any run of back-to-back straight
    // segments can simply be spliced into a single line segment spanning the
    // whole run. "Straight" is circle.r == 0.0f - the existing convention
    // pushLineSegment() already uses for "no meaningful circle here" (see
    // sampleArcPoints() in canvas_bridge.c) - so this never touches or
    // reinterprets any segment that was actually curve-fit; it only merges
    // segments the fitter already agreed were straight lines (and never
    // crosses a noMergeAfter corner boundary - see above).
    int mergedCount = 0;
    for (int i = 0; i < segCount; )
    {
        if (out[i].circle.r == 0.0f)
        {
            int j = i;
            while (j + 1 < segCount && out[j + 1].circle.r == 0.0f && !noMergeAfter[j]) j++;

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

// ---- Robot joint / limb geometry (float, normalized GL-space coords) ----
//
// Merged in from what used to be a separate geometry(1).c. Uses
// PointF/CircleF rather than Point/Circle -- see geometry.h's comment on
// those types for why.

PointF rotatePoint(PointF p, PointF center, float angleDeg)
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

PointF inverseRotate(PointF p, PointF center, float angleDeg)
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

PointF jointToWorld(PointF local, PointF pivot, float jointAngle, PointF center, float bodyAngle)
{
    PointF afterJoint = rotatePoint(local, pivot, jointAngle);
    return rotatePoint(afterJoint, center, bodyAngle);
}

PointF nestedJointToWorld(PointF local, PointF innerPivot, float innerAngle, PointF outerPivot, float outerAngle, PointF center, float bodyAngle)
{
    PointF afterInner = rotatePoint(local, innerPivot, innerAngle);
    return jointToWorld(afterInner, outerPivot, outerAngle, center, bodyAngle);
}

PointF constrainToAxis(PointF pivot, PointF axisRef, PointF target, float minLength)
{
    PointF dir = { axisRef.x - pivot.x, axisRef.y - pivot.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return axisRef; // degenerate axis (zero-length limb) -- nothing to project onto

    dir.x /= len;
    dir.y /= len;

    PointF toTarget = { target.x - pivot.x, target.y - pivot.y };
    float proj = toTarget.x * dir.x + toTarget.y * dir.y;

    if (proj < minLength)
        proj = minLength;

    PointF result = { pivot.x + dir.x * proj, pivot.y + dir.y * proj };
    return result;
}

float perpOffsetOnAxis(PointF p, PointF axisStart, PointF axisEnd)
{
    PointF dir = { axisEnd.x - axisStart.x, axisEnd.y - axisStart.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return 0.0f; // degenerate axis -- no sideways direction to measure against

    dir.x /= len;
    dir.y /= len;

    // rotate dir by 90 degrees to get the sideways direction
    PointF perp = { -dir.y, dir.x };

    PointF toP = { p.x - axisStart.x, p.y - axisStart.y };
    return toP.x * perp.x + toP.y * perp.y;
}

PointF recenterOnAxis(PointF p, PointF axisStart, PointF axisEnd)
{
    PointF dir = { axisEnd.x - axisStart.x, axisEnd.y - axisStart.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return p; // degenerate axis -- nothing sensible to recenter onto

    dir.x /= len;
    dir.y /= len;

    PointF perp = { -dir.y, dir.x };
    float perpOffset = perpOffsetOnAxis(p, axisStart, axisEnd);

    PointF mid = { axisStart.x + dir.x * (len * 0.5f), axisStart.y + dir.y * (len * 0.5f) };

    PointF result = { mid.x + perp.x * perpOffset, mid.y + perp.y * perpOffset };
    return result;
}

PointF circleEdge(PointF center, float radius, float angleDeg)
{
    float a = angleDeg * 3.1415926f / 180.0f;

    PointF p;
    p.x = center.x + cosf(a) * radius;
    p.y = center.y + sinf(a) * radius;

    return p;
}

int isNear(PointF a, PointF b, float radius)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return (dx*dx + dy*dy) < (radius * radius);
}

PointF circleTowardPoint(PointF center, float radius, PointF target)
{
    PointF d = { target.x - center.x, target.y - center.y };
    float dist = sqrtf(d.x * d.x + d.y * d.y);

    if (dist < 1e-6f)
        return (PointF){ center.x + radius, center.y }; // degenerate -- target sits on the center, direction is arbitrary

    PointF u = { d.x / dist, d.y / dist };
    return (PointF){ center.x + u.x * radius, center.y + u.y * radius };
}

PointF internalTangentPoint(PointF c1, float r1, PointF c2, float r2)
{
    // whichever circle has the bigger radius is the one "containing" the
    // other -- the tangent point sits on the containing circle's boundary,
    // in the direction of the smaller circle's center
    if (r1 >= r2)
        return circleTowardPoint(c1, r1, c2);
    else
        return circleTowardPoint(c2, r2, c1);
}

Fillet filletFromAttachAngle(PointF c1, float r1, PointF c2, float r2, float angleDeg, float minRadius, float maxRadius)
{
    float a = angleDeg * 3.1415926f / 180.0f;
    PointF u = { cosf(a), sinf(a) };

    PointF V = { c1.x - c2.x, c1.y - c2.y };
    float VdotU = V.x * u.x + V.y * u.y;
    float VmagSq = V.x * V.x + V.y * V.y;

    float denom = 2.0f * (VdotU + (r1 - r2));

    float radius;

    if (fabsf(denom) < 1e-4f)
    {
        radius = maxRadius;
    }
    else
    {
        float k = ((r1 - r2) * (r1 - r2) - VmagSq) / denom;
        radius = r1 - k;
    }

    if (radius < minRadius) radius = minRadius;
    if (radius > maxRadius) radius = maxRadius;

    float k = r1 - radius;

    Fillet result;
    result.center = (PointF){ c1.x + k * u.x, c1.y + k * u.y };
    result.radius = radius;
    return result;
}

Fillet filletFromAttachAngleConcave(PointF c1, float r1, PointF c2, float r2, float angleDeg, float minRadius, float maxRadius)
{
    float a = angleDeg * 3.1415926f / 180.0f;
    PointF u = { cosf(a), sinf(a) };

    PointF V = { c1.x - c2.x, c1.y - c2.y };
    float VdotU = V.x * u.x + V.y * u.y;
    float VmagSq = V.x * V.x + V.y * V.y;

    float denom = 2.0f * (VdotU + (r1 - r2));

    float radius;

    if (fabsf(denom) < 1e-4f)
    {
        radius = maxRadius;
    }
    else
    {
        float k = ((r1 - r2) * (r1 - r2) - VmagSq) / denom;
        radius = k - r1;
    }

    if (radius < minRadius) radius = minRadius;
    if (radius > maxRadius) radius = maxRadius;

    float k = r1 + radius;

    Fillet result;
    result.center = (PointF){ c1.x + k * u.x, c1.y + k * u.y };
    result.radius = radius;
    return result;
}

SafeAngleRange filletSafeAngleRange(PointF c1, float r1, PointF c2, float r2, float maxRadius)
{
    PointF V = { c1.x - c2.x, c1.y - c2.y };
    float dist = sqrtf(V.x * V.x + V.y * V.y);

    SafeAngleRange range;

    if (dist < 1e-6f)
    {
        range.centerDeg = 0.0f;
        range.halfWidthDeg = 180.0f;
        return range;
    }

    range.centerDeg = atan2f(V.y, V.x) * 180.0f / 3.1415926f;

    float ratioSing = (r2 - r1) / dist;
    if (ratioSing < -1.0f) ratioSing = -1.0f;
    if (ratioSing > 1.0f) ratioSing = 1.0f;
    float half = acosf(ratioSing) * 180.0f / 3.1415926f;

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

SafeAngleRange filletSafeAngleRangeConcave(PointF c1, float r1, PointF c2, float r2, float maxRadius)
{
    PointF V = { c1.x - c2.x, c1.y - c2.y };
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

    float ratioSing = (r2 - r1) / dist;
    if (ratioSing < -1.0f) ratioSing = -1.0f;
    if (ratioSing > 1.0f) ratioSing = 1.0f;
    float halfSingFromConvexCenter = acosf(ratioSing) * 180.0f / 3.1415926f;
    float half = 180.0f - halfSingFromConvexCenter;

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

PointF filletBulgePoint(PointF c1, float r1, PointF c2, float r2, float angleDeg, float minRadius, float maxRadius, PointF target)
{
    Fillet f = filletFromAttachAngle(c1, r1, c2, r2, angleDeg, minRadius, maxRadius);
    return circleTowardPoint(f.center, f.radius, target);
}

PointF circleAtX(PointF center, float radius, float targetX, PointF preferNear)
{
    float dx = targetX - center.x;
    float remSq = radius * radius - dx * dx;
    if (remSq < 0.0f) remSq = 0.0f;

    float dy = sqrtf(remSq);

    PointF p1 = { targetX, center.y + dy };
    PointF p2 = { targetX, center.y - dy };

    float d1 = (p1.y - preferNear.y) * (p1.y - preferNear.y);
    float d2 = (p2.y - preferNear.y) * (p2.y - preferNear.y);

    return (d1 <= d2) ? p1 : p2;
}

PointF circleAtAxisMid(PointF center, float radius, PointF axisStart, PointF axisEnd, PointF preferNear)
{
    PointF dir = { axisEnd.x - axisStart.x, axisEnd.y - axisStart.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len < 1e-6f)
        return circleTowardPoint(center, radius, preferNear);

    dir.x /= len;
    dir.y /= len;

    PointF perp = { -dir.y, dir.x };
    PointF axisMid = { axisStart.x + dir.x * (len * 0.5f), axisStart.y + dir.y * (len * 0.5f) };

    PointF d = { axisMid.x - center.x, axisMid.y - center.y };
    float dDotPerp = d.x * perp.x + d.y * perp.y;
    float dMagSq = d.x * d.x + d.y * d.y;

    float disc = dDotPerp * dDotPerp - (dMagSq - radius * radius);
    if (disc < 0.0f) disc = 0.0f;

    float sq = sqrtf(disc);
    float t1 = -dDotPerp + sq;
    float t2 = -dDotPerp - sq;

    PointF p1 = { axisMid.x + perp.x * t1, axisMid.y + perp.y * t1 };
    PointF p2 = { axisMid.x + perp.x * t2, axisMid.y + perp.y * t2 };

    float dist1 = (p1.x - preferNear.x) * (p1.x - preferNear.x) + (p1.y - preferNear.y) * (p1.y - preferNear.y);
    float dist2 = (p2.x - preferNear.x) * (p2.x - preferNear.x) + (p2.y - preferNear.y) * (p2.y - preferNear.y);

    return (dist1 <= dist2) ? p1 : p2;
}

// Finds the circle that passes through all three points.
// Used to draw an arc through p0, p1, p2 instead of a bezier curve.
CircleF circumcircle(PointF p0, PointF p1, PointF p2)
{
    CircleF c;

    float ax = p0.x, ay = p0.y;
    float bx = p1.x, by = p1.y;
    float cx = p2.x, cy = p2.y;

    float d = 2.0f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));

    if (fabsf(d) < 1e-6f)
    {
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
