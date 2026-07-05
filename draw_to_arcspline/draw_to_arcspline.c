/* draw_to_arcspline.c
 *
 * Pelles C / plain Win32 C version.
 *
 * Freehand-draw with the left mouse button. On release, the raw point
 * sequence is converted into an "arc spline": a sequence of circular arcs
 * (falling back to straight lines where needed) that approximates the
 * stroke within a pixel tolerance.
 *
 * Algorithm: recursive arc fitting.
 *   1. Try to fit a single circle through a run of points using a
 *      least-squares algebraic circle fit (Kasa method).
 *   2. Measure the max deviation of the actual points from that circle.
 *   3. If within TOLERANCE units, accept it as one arc segment.
 *   4. Otherwise split the run in half and recurse on each half.
 *   5. Too-short / collinear runs fall back to a straight line segment.
 *
 * Zoom:
 *   All captured points are stored in "world" (document) coordinates, and
 *   only converted to screen pixels at render time via a zoom + pan
 *   transform. This keeps the arc-fitting math completely independent of
 *   zoom level -- it always fits the actual stroke shape, not whatever
 *   happens to be on screen at the moment.
 *     - Mouse wheel: zoom in/out, centered on the cursor position.
 *     - '+' / '-' (or numpad +/-): zoom in/out, centered on the window.
 *     - '0': reset zoom and pan.
 *
 * BUILD IN PELLES C IDE:
 *   File -> New -> Project -> Win32 Executable (GUI)
 *   Add this .c file to the project.
 *   Pelles C auto-links user32.lib / gdi32.lib for GUI projects. If it
 *   complains about unresolved symbols, add User32.lib and Gdi32.lib
 *   under Project -> Options -> Linker -> "Additional libraries".
 *   Build (F7) and run (F5).
 *
 * BUILD FROM COMMAND LINE (Pelles C's pocc/polink):
 *   pocc draw_to_arcspline.c
 *   polink draw_to_arcspline.obj user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
 *
 * Draw with the left mouse button; gray = raw freehand stroke,
 * red = fitted arc spline. Hover over segments to highlight their circles (bright green).
 * Mouse wheel / +/-/0 to zoom. Escape to quit.
 */

#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct { double x, y; } Pt;

typedef struct {
    int    isLine;   /* nonzero => straight segment, 0 => circular arc */
    Pt     center;   /* valid only if !isLine (world coords) */
    double radius;   /* valid only if !isLine (world units) */
    Pt     start;    /* world coords */
    Pt     end;      /* world coords */
    int    sweepPositive; /* valid only if !isLine: 1 = increasing-angle sweep from
                            * start to end (our own atan2 sense), 0 = decreasing.
                            * Used only by our own manual arc drawing, not by GDI's
                            * Arc()/SetArcDirection, whose CW/CCW convention is
                            * ambiguous once y-down coordinates are involved. */
} ArcSeg;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- simple dynamic arrays (grow-by-doubling) --- */

static Pt     *g_rawPoints = NULL; /* world coordinates */
static int     g_rawCount = 0;
static int     g_rawCap = 0;

static ArcSeg *g_arcs = NULL;      /* world coordinates */
static int     g_arcCount = 0;
static int     g_arcCap = 0;

static int g_drawing = 0;

/* Track which arc segment is being hovered over (-1 if none) */
static int g_hoveredSegment = -1;

/* --- zoom / pan state ---
 * Screen = World * zoom + pan. World = (Screen - pan) / zoom.
 */
static double g_zoom = 1.0;
static double g_panX = 0.0;
static double g_panY = 0.0;
static const double ZOOM_MIN = 0.05;
static const double ZOOM_MAX = 20.0;
static const double ZOOM_STEP = 1.15;

/* Max allowed deviation (world units) between the stroke and a fitted arc. */
static const double TOLERANCE = 3.0;

/* Minimum number of points before we even attempt a circle fit. */
static const int MIN_POINTS_FOR_ARC = 5;

static Pt ScreenToWorld(int sx, int sy) {
    Pt w;
    w.x = ((double)sx - g_panX) / g_zoom;
    w.y = ((double)sy - g_panY) / g_zoom;
    return w;
}

static void WorldToScreen(Pt w, int *sx, int *sy) {
    *sx = (int)floor(w.x * g_zoom + g_panX + 0.5);
    *sy = (int)floor(w.y * g_zoom + g_panY + 0.5);
}

/* Zoom by `factor`, keeping the world point currently under (cx, cy) --
 * given in screen/client coordinates -- fixed in place on screen. */
static void ZoomAt(int cx, int cy, double factor) {
    double newZoom = g_zoom * factor;
    Pt worldUnderCursor;

    if (newZoom < ZOOM_MIN) newZoom = ZOOM_MIN;
    if (newZoom > ZOOM_MAX) newZoom = ZOOM_MAX;
    if (newZoom == g_zoom) return;

    worldUnderCursor = ScreenToWorld(cx, cy);
    g_zoom = newZoom;
    g_panX = (double)cx - worldUnderCursor.x * g_zoom;
    g_panY = (double)cy - worldUnderCursor.y * g_zoom;
}

static void RawPointsClear(void) {
    g_rawCount = 0;
}

static void RawPointsPush(Pt p) {
    if (g_rawCount >= g_rawCap) {
        g_rawCap = (g_rawCap == 0) ? 256 : g_rawCap * 2;
        g_rawPoints = (Pt *)realloc(g_rawPoints, sizeof(Pt) * g_rawCap);
    }
    g_rawPoints[g_rawCount++] = p;
}

static void ArcsClear(void) {
    g_arcCount = 0;
}

static void ArcsPush(ArcSeg seg) {
    if (g_arcCount >= g_arcCap) {
        g_arcCap = (g_arcCap == 0) ? 64 : g_arcCap * 2;
        g_arcs = (ArcSeg *)realloc(g_arcs, sizeof(ArcSeg) * g_arcCap);
    }
    g_arcs[g_arcCount++] = seg;
}

/* ---------------------------------------------------------------------
 * Least-squares circle fit (Kasa method).
 * Fits center (cx, cy) and radius r through pts[0..count-1].
 * Returns 0 if the points are (numerically) collinear.
 * ------------------------------------------------------------------- */
static int fitCircle(const Pt *pts, int count, Pt *center, double *radius) {
    double sumX = 0, sumY = 0, sumX2 = 0, sumY2 = 0, sumXY = 0;
    double sumX3 = 0, sumY3 = 0, sumXY2 = 0, sumX2Y = 0;
    double N, C, D, E, G, H, denom, a, b, rSum;
    int i;

    if (count < 3) return 0;

    for (i = 0; i < count; i++) {
        double x = pts[i].x, y = pts[i].y;
        sumX  += x;       sumY  += y;
        sumX2 += x * x;   sumY2 += y * y;
        sumXY += x * y;
        sumX3 += x * x * x;
        sumY3 += y * y * y;
        sumXY2 += x * y * y;
        sumX2Y += x * x * y;
    }

    N = (double)count;
    C = N * sumX2 - sumX * sumX;
    D = N * sumXY - sumX * sumY;
    E = N * (sumX3 + sumXY2) - (sumX2 + sumY2) * sumX;
    G = N * sumY2 - sumY * sumY;
    H = N * (sumX2Y + sumY3) - (sumX2 + sumY2) * sumY;

    denom = C * G - D * D;
    if (fabs(denom) < 1e-6) return 0; /* nearly collinear */

    a = (E * G - D * H) / denom;
    b = (C * H - D * E) / denom;

    center->x = a / 2.0;
    center->y = b / 2.0;

    rSum = 0;
    for (i = 0; i < count; i++) {
        double dx = pts[i].x - center->x, dy = pts[i].y - center->y;
        rSum += sqrt(dx * dx + dy * dy);
    }
    *radius = rSum / N;
    return 1;
}

static double maxDeviation(const Pt *pts, int count, Pt center, double radius) {
    double maxErr = 0;
    int i;
    for (i = 0; i < count; i++) {
        double dx = pts[i].x - center.x, dy = pts[i].y - center.y;
        double d = fabs(sqrt(dx * dx + dy * dy) - radius);
        if (d > maxErr) maxErr = d;
    }
    return maxErr;
}

/* Decide which rotational sense (in our own atan2 convention) sweeps from
 * `start` to `end` while passing through `mid` (an actual point from the
 * traced stroke). This is used only by our own manual arc-drawing code
 * below -- it deliberately does NOT try to map onto GDI's AD_CLOCKWISE /
 * AD_COUNTERCLOCKWISE, since that mapping is ambiguous once a y-down
 * coordinate system is involved and got the direction backwards more often
 * than not. Returns 1 if the increasing-angle sense contains mid, 0 if the
 * decreasing-angle sense does. */
static int chooseSweepSense(Pt center, Pt start, Pt mid, Pt end) {
    double a0 = atan2(start.y - center.y, start.x - center.x);
    double aM = atan2(mid.y - center.y, mid.x - center.x);
    double a1 = atan2(end.y - center.y, end.x - center.x);
    double twoPi = 2.0 * M_PI;
    double dM = fmod(aM - a0 + twoPi, twoPi);
    double dE = fmod(a1 - a0 + twoPi, twoPi);
    return (dM <= dE) ? 1 : 0;
}

/* Recursively fit arcs/lines to pts[0..count-1], appending to global arc list. */
static void recursiveArcFit(const Pt *pts, int count) {
    Pt center;
    double radius = 0;
    int ok, radiusReasonable;
    double err;
    ArcSeg seg;

    if (count < 2) return;

    if (count < MIN_POINTS_FOR_ARC) {
        seg.isLine = 1;
        seg.start = pts[0];
        seg.end = pts[count - 1];
        ArcsPush(seg);
        return;
    }

    ok = fitCircle(pts, count, &center, &radius);
    err = ok ? maxDeviation(pts, count, center, radius) : 1e18;
    radiusReasonable = ok && radius < 100000.0;

    if (ok && radiusReasonable && err <= TOLERANCE) {
        seg.isLine = 0;
        seg.center = center;
        seg.radius = radius;
        seg.start = pts[0];
        seg.end = pts[count - 1];
        /* Use an interior point (not exactly start or end) as ground truth
         * for which direction the stroke actually swept. */
        seg.sweepPositive = chooseSweepSense(center, pts[0], pts[count / 2], pts[count - 1]);
        ArcsPush(seg);
    } else if (count <= MIN_POINTS_FOR_ARC + 1) {
        seg.isLine = 1;
        seg.start = pts[0];
        seg.end = pts[count - 1];
        ArcsPush(seg);
    } else {
        int mid = count / 2;
        /* left run: pts[0..mid], right run: pts[mid..count-1] (shared joint point) */
        recursiveArcFit(pts, mid + 1);
        recursiveArcFit(pts + mid, count - mid);
    }
}

static void ConvertRawToArcs(void) {
    ArcsClear();
    if (g_rawCount < 2) return;
    recursiveArcFit(g_rawPoints, g_rawCount);
}

/* Calculate distance from a point to a line segment */
static double distanceToLineSegment(Pt p, Pt a, Pt b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double len2 = dx * dx + dy * dy;

    if (len2 < 1e-6) {
        /* Segment is essentially a point */
        double px = p.x - a.x;
        double py = p.y - a.y;
        return sqrt(px * px + py * py);
    }

    /* Project point onto line segment */
    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
    t = (t < 0) ? 0 : (t > 1) ? 1 : t;

    double projX = a.x + t * dx;
    double projY = a.y + t * dy;
    double px = p.x - projX;
    double py = p.y - projY;
    return sqrt(px * px + py * py);
}

/* Find which arc segment (if any) the cursor is hovering directly over.
 * Only detects when hovering VERY close to the actual drawn arc segment,
 * not when hovering near the ghost circle outline.
 * Returns the segment index of the closest match, or -1 if none are nearby. */
static int findSegmentAtPoint(Pt worldPt) {
    double tolerance = 10.0 / g_zoom;  /* Stricter tolerance - only actual arc */
    int i;
    int closestSegment = -1;
    double closestDist = tolerance;

    for (i = 0; i < g_arcCount; i++) {
        ArcSeg *seg = &g_arcs[i];

        if (seg->isLine) {
            /* For line segments, check distance to the line */
            double dist = distanceToLineSegment(worldPt, seg->start, seg->end);

            /* Keep track of the closest segment */
            if (dist < closestDist) {
                closestDist = dist;
                closestSegment = i;
            }
        } else {
            /* For circular arcs, we need to check distance to the actual arc polyline,
             * not just "is it on the circle somewhere in the angle range".
             * We do this by checking several sample points along the arc. */
            
            double a0 = atan2(seg->start.y - seg->center.y, seg->start.x - seg->center.x);
            double a1 = atan2(seg->end.y - seg->center.y, seg->end.x - seg->center.x);
            double twoPi = 2.0 * M_PI;
            double deltaPositive = fmod(a1 - a0 + twoPi, twoPi);
            double delta = seg->sweepPositive ? deltaPositive : (deltaPositive - twoPi);

            /* Sample points along the arc to check distance */
            int numSamples = 20;  /* Check 20 points along the arc */
            int s;
            for (s = 0; s <= numSamples; s++) {
                double t = (numSamples > 0) ? (double)s / (double)numSamples : 0.0;
                double angle = a0 + delta * t;
                
                Pt arcPoint;
                arcPoint.x = seg->center.x + seg->radius * cos(angle);
                arcPoint.y = seg->center.y + seg->radius * sin(angle);

                double dx = worldPt.x - arcPoint.x;
                double dy = worldPt.y - arcPoint.y;
                double dist = sqrt(dx * dx + dy * dy);

                /* Keep track of the closest point on any arc */
                if (dist < closestDist) {
                    closestDist = dist;
                    closestSegment = i;
                }
            }
        }
    }

    return closestSegment;
}

/* Draw one circular-arc segment as a short polyline, computing every point
 * ourselves from center/radius/angle (in world space) then mapping each
 * vertex to screen space via WorldToScreen. This avoids handing two
 * boundary points to GDI's Arc() and relying on SetArcDirection to pick
 * the right of the two possible arcs, which proved ambiguous. */
static void DrawArcSegment(HDC hdc, const ArcSeg *seg) {
    double a0 = atan2(seg->start.y - seg->center.y, seg->start.x - seg->center.x);
    double a1 = atan2(seg->end.y   - seg->center.y, seg->end.x   - seg->center.x);
    double twoPi = 2.0 * M_PI;
    double deltaPositive = fmod(a1 - a0 + twoPi, twoPi); /* sweep length going the increasing-angle way */
    double delta;
    int steps, i;

    if (seg->sweepPositive) {
        delta = deltaPositive;              /* sweep in increasing-angle direction */
    } else {
        delta = deltaPositive - twoPi;       /* sweep the other way (negative angle) */
    }

    /* Choose a step count proportional to the arc's on-screen length so
     * curves look smooth at the current zoom, bounded for tiny/huge cases. */
    steps = (int)ceil(fabs(delta) * seg->radius * g_zoom / 3.0);
    if (steps < 2) steps = 2;
    if (steps > 400) steps = 400;

    for (i = 0; i <= steps; i++) {
        int x, y;
        if (i == 0) {
            /* Snap to the literal raw endpoint rather than the
             * circle-reconstructed position, so this matches exactly
             * what the previous segment ended on (they share the same
             * underlying raw data point). */
            WorldToScreen(seg->start, &x, &y);
        } else if (i == steps) {
            WorldToScreen(seg->end, &x, &y);
        } else {
            double t = (double)i / (double)steps;
            double angle = a0 + delta * t;
            Pt worldPt;
            worldPt.x = seg->center.x + seg->radius * cos(angle);
            worldPt.y = seg->center.y + seg->radius * sin(angle);
            WorldToScreen(worldPt, &x, &y);
        }
        if (i == 0) MoveToEx(hdc, x, y, NULL);
        else        LineTo(hdc, x, y);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN: {
        int sx = (short)LOWORD(lParam);
        int sy = (short)HIWORD(lParam);
        g_drawing = 1;
        g_hoveredSegment = -1;  /* Clear hover when starting to draw */
        RawPointsClear();
        ArcsClear();
        RawPointsPush(ScreenToWorld(sx, sy));
        SetCapture(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_drawing && (wParam & MK_LBUTTON)) {
            int sx = (short)LOWORD(lParam);
            int sy = (short)HIWORD(lParam);
            Pt w = ScreenToWorld(sx, sy);
            /* Dedup threshold of ~1 screen pixel, expressed in world units,
             * so point density on screen stays consistent across zoom levels. */
            double thresh = 1.0 / g_zoom;
            if (g_rawCount == 0) {
                RawPointsPush(w);
                InvalidateRect(hwnd, NULL, FALSE);
            } else {
                double dx = w.x - g_rawPoints[g_rawCount - 1].x;
                double dy = w.y - g_rawPoints[g_rawCount - 1].y;
                if (fabs(dx) + fabs(dy) > thresh) {
                    RawPointsPush(w);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
        } else {
            /* When not drawing, check for hovered segments */
            int sx = (short)LOWORD(lParam);
            int sy = (short)HIWORD(lParam);
            Pt w = ScreenToWorld(sx, sy);
            int newHovered = findSegmentAtPoint(w);
            if (newHovered != g_hoveredSegment) {
                g_hoveredSegment = newHovered;
                InvalidateRect(hwnd, NULL, TRUE);  /* Force full redraw when hover changes */
            }
            /* Enable mouse leave tracking */
            if (!(wParam & MK_LBUTTON)) {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        /* Clear hover when mouse leaves the window */
        if (g_hoveredSegment != -1) {
            g_hoveredSegment = -1;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_drawing) {
            g_drawing = 0;
            g_hoveredSegment = -1;  /* Clear hover when finished drawing */
            ReleaseCapture();
            ConvertRawToArcs();
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = (short)HIWORD(wParam);
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        /* WM_MOUSEWHEEL gives screen coordinates; convert to client. */
        ScreenToClient(hwnd, &pt);
        ZoomAt(pt.x, pt.y, (delta > 0) ? ZOOM_STEP : (1.0 / ZOOM_STEP));
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        } else if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            ZoomAt((rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2, ZOOM_STEP);
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            ZoomAt((rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2, 1.0 / ZOOM_STEP);
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (wParam == '0') {
            g_zoom = 1.0;
            g_panX = 0.0;
            g_panY = 0.0;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc;
        HPEN grayPen, redPen, oldPen;
        int i;
        WCHAR buf[160];

        hdc = BeginPaint(hwnd, &ps);

        /* Raw freehand stroke, light gray, underneath. */
        if (g_rawCount > 1) {
            int x, y;
            grayPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
            oldPen = (HPEN)SelectObject(hdc, grayPen);
            WorldToScreen(g_rawPoints[0], &x, &y);
            MoveToEx(hdc, x, y, NULL);
            for (i = 1; i < g_rawCount; i++) {
                WorldToScreen(g_rawPoints[i], &x, &y);
                LineTo(hdc, x, y);
            }
            SelectObject(hdc, oldPen);
            DeleteObject(grayPen);
        }

        /* Faint "ghost circle" for each arc segment: the full circle that
         * segment's arc was cut from, so you can see the underlying circle
         * each piece belongs to. Drawn as a light dashed outline, no fill.
         * Hovered segments are drawn with a bright highlight. */
        {
            HPEN ghostPen = CreatePen(PS_DOT, 1, RGB(190, 190, 230));
            HPEN hoveredPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));  /* Bright green for hovered */
            HPEN oldGhostPen = (HPEN)SelectObject(hdc, ghostPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

            for (i = 0; i < g_arcCount; i++) {
                ArcSeg *seg = &g_arcs[i];
                if (!seg->isLine) {
                    Pt worldTopLeft, worldBottomRight;
                    int left, top, right, bottom;
                    worldTopLeft.x = seg->center.x - seg->radius;
                    worldTopLeft.y = seg->center.y - seg->radius;
                    worldBottomRight.x = seg->center.x + seg->radius;
                    worldBottomRight.y = seg->center.y + seg->radius;
                    WorldToScreen(worldTopLeft, &left, &top);
                    WorldToScreen(worldBottomRight, &right, &bottom);

                    /* Use bright green pen for hovered segment, faint blue for others */
                    if (i == g_hoveredSegment) {
                        SelectObject(hdc, hoveredPen);
                    } else {
                        SelectObject(hdc, ghostPen);
                    }

                    Ellipse(hdc, left, top, right, bottom);
                }
            }

            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldGhostPen);
            DeleteObject(ghostPen);
            DeleteObject(hoveredPen);
        }

        /* Fitted arc spline, red, on top. */
        redPen = CreatePen(PS_SOLID, 2, RGB(210, 0, 0));
        oldPen = (HPEN)SelectObject(hdc, redPen);

        for (i = 0; i < g_arcCount; i++) {
            ArcSeg *seg = &g_arcs[i];
            if (seg->isLine) {
                int x0, y0, x1, y1;
                WorldToScreen(seg->start, &x0, &y0);
                WorldToScreen(seg->end, &x1, &y1);
                MoveToEx(hdc, x0, y0, NULL);
                LineTo(hdc, x1, y1);
            } else {
                DrawArcSegment(hdc, seg);
            }
        }
        SelectObject(hdc, oldPen);
        DeleteObject(redPen);

        wsprintfW(buf, L"Points: %d   Segments: %d   Zoom: %d%%   Hovered: %s   "
                       L"(wheel / +/-/0 to zoom, Esc to quit)",
                  g_rawCount, g_arcCount, (int)floor(g_zoom * 100.0 + 0.5),
                  (g_hoveredSegment >= 0) ? L"Yes" : L"No");
        TextOutW(hdc, 10, 10, buf, (int)wcslen(buf));

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        free(g_rawPoints);
        free(g_arcs);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DrawToArcSplineWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    RegisterClassW(&wc);

    hwnd = CreateWindowExW(
        0, L"DrawToArcSplineWindow",
        L"Freehand -> Arc Spline (draw to sketch, hover to preview circles, wheel to zoom)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
