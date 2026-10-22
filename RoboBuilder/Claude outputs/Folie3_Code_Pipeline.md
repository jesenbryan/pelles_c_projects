# RoboBuilder – Code zu Folie 3 (Sechs Stufen vom Pixel zur Geometrie)

Wörtlich aus dem aktuellen Quellcode kopiert (22.10.2026). Dateiname, Zeilen und Änderungsdatum stehen über jedem Block.


> **Hinweis zu geometry.c (geändert am 16.10.2026):** Die Toleranzen sind keine feste Zahl mehr. `ARC_FIT_TOLERANCE` und `LINE_FIT_TOLERANCE` sind jetzt Makros für `traceArcTolerance()` und `traceLineTolerance()`, gesteuert vom Regler `gArcLineBias` im Umgebungs-Panel (0 = Bögen, 0,5 = Standard, 1 = nur Linien). In der Standardstellung sind beide 2,0 Pixel wie vorher. Die Funktionen stehen in geometry.c ab Zeile 236.

## 1 · Binär

Hochgeladene BMP: Graustufe unter 128 wird Tinte. Gezeichnete Striche werden von canvasToImage direkt ins Binärraster gestempelt.

### `img_to_bin` – binary.c, Zeilen 3–28 (Stand 28.08.2026 03:29)

```c
uint8_t* img_to_bin(Image* img)
{
    int w = img->width;
    int h = img->height;

    uint8_t* bin = (uint8_t*)malloc(w * h * sizeof(uint8_t));

    if (!bin) return NULL;

    for (int y = 0; y < h; y++) {
    	for (int x = 0; x < w; x++) {

	        int i = y * w + x;

			uint8_t b = img->data[i * 3 + 0]; // Index 0 is Blue in BMP
			uint8_t g = img->data[i * 3 + 1]; // Index 1 is Green
			uint8_t r = img->data[i * 3 + 2]; // Index 2 is Red

	        uint8_t gray = (uint8_t)(0.299*r + 0.587*g + 0.114*b);

	        bin[i] = (gray < 128) ? 1 : 0;
    	}
	}

    return bin;
}
```

### `canvasToImage` – canvas_bridge.c, Zeilen 870–953 (Stand 24.09.2026 15:23)

```c
Image* canvasToImage(void)
{
    if (canvas.pointCount < 1 || canvas.strokeCount < 1) {
        printf("Canvas is empty - draw a curve first\n");
        return NULL;
    }

    int w = glWindowWidth;
    int h = glWindowHeight;

    Image* img = (Image*)malloc(sizeof(Image));
    if (!img) return NULL;

    img->width  = w;
    img->height = h;
    img->data = (uint8_t*)malloc((size_t)w * h * 3);
    img->bin  = (uint8_t*)calloc((size_t)w * h, sizeof(uint8_t));

    // Parallel EXACT-radius raster (see bmp.h's Image.radius) -- calloc'd
    // to 0 so an unstamped pixel reads as "no known radius" rather than
    // garbage; every pixel setBinPixel actually touches below overwrites
    // its 0 with the real stamped r, which is always > 0.
    img->radius = (float*)calloc((size_t)w * h, sizeof(float));

    if (!img->data || !img->bin || !img->radius) {
        free(img->data);
        free(img->bin);
        free(img->radius);
        free(img);
        return NULL;
    }

    memset(img->data, 255, (size_t)w * h * 3); // white background

    // Fixed, zoom/pan-independent mapping for this trace -- see its own
    // comment. Must run before the stroke loop below (worldToPixel reads
    // the globals it sets) and after canvas.pointCount's own empty check
    // above (it assumes at least one point exists).
    computeTraceBounds(w, h);

    for (int s = 0; s < canvas.strokeCount; s++)
    {
        int start = strokeStarts[s];
        int end   = (s == canvas.strokeCount - 1) ? canvas.pointCount : strokeStarts[s + 1];
        int count = (end - start) / 2;
        if (count < 1) continue;

        // No floor on r here (there used to be one, clamping up to 1.0px)
        // -- stampDisc/stampSegment's own pixel-CENTER test ((x+0.5)-cx,
        // not a bare integer compare) reliably rasterizes a genuine 0.5px
        // disc to at least one real "on" pixel regardless of sub-pixel
        // alignment (verified numerically: 0 empty rasters across a full
        // sweep of offsets), so the floor wasn't actually needed to avoid
        // an invisible stroke. What it WAS doing: at thickness == 1 (the
        // slider's minimum, the only value where this floor ever
        // triggered), it silently inflated the raster's true radius from
        // 0.5 to 1.0 -- pipeline.c's width measurement then faithfully
        // measured that INFLATED 1.0, and canvas_bridge.c's own *2
        // conversion back below took it from there, so the reconstructed
        // line rendered at fully double the actual thickness=1 stroke's
        // width. Letting r go down to its true 0.5 here is what the
        // measurement needs to recover the CORRECT width instead.
        float r = strokeThickness[s] / 2.0f;

        float px0, py0;
        worldToPixel(points[start], points[start + 1], w, h, &px0, &py0);

        if (count == 1) {
            stampDisc(img, px0, py0, r);
            continue;
        }

        for (int i = 1; i < count; i++)
        {
            float px1, py1;
            worldToPixel(points[start + i * 2], points[start + i * 2 + 1], w, h, &px1, &py1);
            stampSegment(img, px0, py0, px1, py1, r);
            px0 = px1;
            py0 = py1;
        }
    }

    return img;
}
```

## 2 · Ausdünnen

Zhang-Suen mit Untergrenze B(P) >= 3 (Lu und Wang).

### `thinningZhangSuen` – thinning.c, Zeilen 3–131 (Stand 23.09.2026 15:41)

```c
// Zhang-Suen thinning with the Lu & Wang (1986) modification: a pixel may
// only be deleted when it has 3..6 foreground neighbours (B(P) in [3,6])
// instead of Zhang-Suen's original 2..6. With the original range, a line
// that is exactly 2 pixels thick along a "/" diagonal gets eroded from both
// sides in the same sub-iteration and disappears completely (seen as a whole
// diagonal of a drawn stage missing from the trace at some window sizes).
// Lu & Wang's lower bound of 3 keeps such lines one pixel wide instead.
// See pruneSkeletonSpurs (pipeline.c) for the spur clean-up that follows.
void thinningZhangSuen(Image* img)
{
    int w = img->width;
    int h = img->height;
    uint8_t* bin = img->bin;

    // Allocate a temporary buffer to store deletion flags for the current iteration
    uint8_t* marker = (uint8_t*)calloc(w * h, sizeof(uint8_t));
    if (!marker) return;

    int changed = 1;
    while (changed)
    {
        changed = 0;

        // --- Step 1 ---
        // We safely step inside the canvas. If you need border pixels thinned, 
        // they must be guarded against array out-of-bounds.
        for (int y = 1; y < h - 1; y++)
        {
            for (int x = 1; x < w - 1; x++)
            {
                int idx = y * w + x;
                if (bin[idx] == 0) continue; // Skip background

                // Read 8-connected neighbors clockwise starting from North
                int p2 = bin[(y - 1) * w + x];     // N
                int p3 = bin[(y - 1) * w + (x + 1)]; // NE
                int p4 = bin[y * w + (x + 1)];     // E
                int p5 = bin[(y + 1) * w + (x + 1)]; // SE
                int p6 = bin[(y + 1) * w + x];     // S
                int p7 = bin[(y + 1) * w + (x - 1)]; // SW
                int p8 = bin[y * w + (x - 1)];     // W
                int p9 = bin[(y - 1) * w + (x - 1)]; // NW

                // Count foreground neighbors (2 <= B(P) <= 6)
                int bp = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;

                // Count 0-to-1 transitions (A(P) == 1)
                int ap = ((p2 == 0 && p3 == 1) ? 1 : 0) +
                         ((p3 == 0 && p4 == 1) ? 1 : 0) +
                         ((p4 == 0 && p5 == 1) ? 1 : 0) +
                         ((p5 == 0 && p6 == 1) ? 1 : 0) +
                         ((p6 == 0 && p7 == 1) ? 1 : 0) +
                         ((p7 == 0 && p8 == 1) ? 1 : 0) +
                         ((p8 == 0 && p9 == 1) ? 1 : 0) +
                         ((p9 == 0 && p2 == 1) ? 1 : 0);

                if (bp >= 3 && bp <= 6 /* Lu & Wang: 3, not 2 */ && ap == 1)
                {
                    if ((p2 * p4 * p6 == 0) && (p4 * p6 * p8 == 0))
                    {
                        marker[idx] = 1;
                        changed = 1;
                    }
                }
            }
        }

        // Apply deletion flags for Step 1
        for (int i = 0; i < w * h; i++)
        {
            if (marker[i])
            {
                bin[i] = 0;
                marker[i] = 0; // Reset marker array
            }
        }

        // --- Step 2 ---
        for (int y = 1; y < h - 1; y++)
        {
            for (int x = 1; x < w - 1; x++)
            {
                int idx = y * w + x;
                if (bin[idx] == 0) continue;

                int p2 = bin[(y - 1) * w + x];
                int p3 = bin[(y - 1) * w + (x + 1)];
                int p4 = bin[y * w + (x + 1)];
                int p5 = bin[(y + 1) * w + (x + 1)];
                int p6 = bin[(y + 1) * w + x];
                int p7 = bin[(y + 1) * w + (x - 1)];
                int p8 = bin[y * w + (x - 1)];
                int p9 = bin[(y - 1) * w + (x - 1)];

                int bp = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                int ap = ((p2 == 0 && p3 == 1) ? 1 : 0) +
                         ((p3 == 0 && p4 == 1) ? 1 : 0) +
                         ((p4 == 0 && p5 == 1) ? 1 : 0) +
                         ((p5 == 0 && p6 == 1) ? 1 : 0) +
                         ((p6 == 0 && p7 == 1) ? 1 : 0) +
                         ((p7 == 0 && p8 == 1) ? 1 : 0) +
                         ((p8 == 0 && p9 == 1) ? 1 : 0) +
                         ((p9 == 0 && p2 == 1) ? 1 : 0);

                if (bp >= 3 && bp <= 6 /* Lu & Wang: 3, not 2 */ && ap == 1)
                {
                    // Condition change for Step 2: (p2 * p4 * p8 == 0) and (p2 * p6 * p8 == 0)
                    if ((p2 * p4 * p8 == 0) && (p2 * p6 * p8 == 0))
                    {
                        marker[idx] = 1;
                        changed = 1;
                    }
                }
            }
        }

        // Apply deletion flags for Step 2
        for (int i = 0; i < w * h; i++)
        {
            if (marker[i])
            {
                bin[i] = 0;
                marker[i] = 0;
            }
        }
    }

    free(marker);
}
```

## 3 · Stutzen

Entfernt kurze Seitenäste nach dem Ausdünnen.

### `crossingNumber` – pipeline.c, Zeilen 225–241 (Stand 24.09.2026 15:23)

```c
// Number of 0->1 transitions around (x,y), clockwise from N -- the same
// A(P) value Zhang-Suen uses. 1 = line end / line interior, >=3 = junction.
static int crossingNumber(const uint8_t* b, int w, int h, int x, int y)
{
    static const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    int v[8];
    for (int k = 0; k < 8; k++)
    {
        int nx = x + dx[k], ny = y + dy[k];
        v[k] = (nx >= 0 && ny >= 0 && nx < w && ny < h) ? b[ny * w + nx] : 0;
    }
    int cn = 0;
    for (int k = 0; k < 8; k++)
        if (!v[k] && v[(k + 1) % 8]) cn++;
    return cn;
}
```

### `neighborCount` – pipeline.c, Zeilen 243–254 (Stand 24.09.2026 15:23)

```c
static int neighborCount(const uint8_t* b, int w, int h, int x, int y)
{
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            if (!dx && !dy) continue;
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && ny >= 0 && nx < w && ny < h && b[ny * w + nx]) n++;
        }
    return n;
}
```

### `pruneSkeletonSpurs` – pipeline.c, Zeilen 258–311 (Stand 24.09.2026 15:23)

```c
// Removes spurs from a thinned skeleton in place. origBin (the un-thinned
// ink) supplies the local stroke half-thickness at each junction, so the
// "short" threshold scales with how thick the drawing is. Returns the number
// of pixels removed.
static int pruneSkeletonSpurs(uint8_t* skel, const uint8_t* origBin, int w, int h)
{
    int removedTotal = 0;
    int bx[SPUR_MAX_WALK + 1], by[SPUR_MAX_WALK + 1];

    for (int pass = 0; pass < 4; pass++)
    {
        int removedThisPass = 0;
        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            if (!skel[y * w + x]) continue;
            if (crossingNumber(skel, w, h, x, y) != 1 || neighborCount(skel, w, h, x, y) > 2) continue; // not an end

            // Walk from this end until a junction, the other end, or too far.
            int len = 0, cx = x, cy = y, px = -1, py = -1, reachedJunction = 0;
            while (len <= SPUR_MAX_WALK)
            {
                bx[len] = cx; by[len] = cy; len++;
                int nx = -1, ny = -1, best = 99;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        if (!dx && !dy) continue;
                        int qx = cx + dx, qy = cy + dy;
                        if (qx < 0 || qy < 0 || qx >= w || qy >= h || !skel[qy * w + qx]) continue;
                        int seen = 0;
                        for (int k = 0; k < len; k++) if (bx[k] == qx && by[k] == qy) { seen = 1; break; }
                        if (seen) continue;
                        int rank = (dx == 0 || dy == 0) ? 0 : 1; // prefer 4-neighbours
                        if (rank < best) { best = rank; nx = qx; ny = qy; }
                    }
                if (nx < 0) break;                          // dead end: isolated short piece, keep it
                if (crossingNumber(skel, w, h, nx, ny) >= 3) { reachedJunction = 1; px = nx; py = ny; break; }
                cx = nx; cy = ny;
            }
            if (!reachedJunction) continue;

            float localR = measureLocalRadiusPx(origBin, w, h, px, py, MAX_MEASURED_STROKE_RADIUS_PX);
            float maxSpurLen = 2.0f * localR + 2.0f;
            if ((float)len > maxSpurLen) continue;          // long branch: a real line, keep

            for (int k = 0; k < len; k++) skel[by[k] * w + bx[k]] = 0;
            removedThisPass += len;
        }
        removedTotal += removedThisPass;
        if (!removedThisPass) break;
    }
    return removedTotal;
}
```

## 4 · Verfolgen

Komponente herauslösen, Endpunkte suchen, gierig ablaufen.

### `extractComponent` – pipeline.c, Zeilen 64–104 (Stand 24.09.2026 15:23)

```c
// Flood-fills the 8-connected component containing (startX, startY) out of
// `remaining` into `compBin` (must be pre-zeroed, same w*h size as
// `remaining`), clearing those pixels out of `remaining` so the caller's
// scan won't visit them again for a later component. Heap-based stack so it
// doesn't blow the call stack on a long, curly stroke.
static void extractComponent(uint8_t* remaining, uint8_t* compBin, int w, int h, int startX, int startY)
{
    int* stackX = (int*)malloc(sizeof(int) * (size_t)w * h);
    int* stackY = (int*)malloc(sizeof(int) * (size_t)w * h);
    int sp = 0;

    stackX[sp] = startX;
    stackY[sp] = startY;
    sp++;
    remaining[startY * w + startX] = 0;

    while (sp > 0)
    {
        sp--;
        int x = stackX[sp];
        int y = stackY[sp];
        compBin[y * w + x] = 1;

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (!remaining[ny * w + nx]) continue;

                remaining[ny * w + nx] = 0;
                stackX[sp] = nx;
                stackY[sp] = ny;
                sp++;
            }
        }
    }

    free(stackX);
    free(stackY);
}
```

### `find_start_end_pixels` – endpoints.c, Zeilen 20–117 (Stand 04.09.2026 18:33)

```c
int find_start_end_pixels(uint8_t* bin, int w, int h, int* sx, int* sy, int* ex, int* ey)
{
    int count_found = 0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            if (!bin[y * w + x])
                continue;

            int N  = (y > 0)         && bin[(y - 1) * w + x];
            int S  = (y < h - 1)     && bin[(y + 1) * w + x];
            int W  = (x > 0)         && bin[y * w + (x - 1)];
            int E  = (x < w - 1)     && bin[y * w + (x + 1)];

            int NW = (y > 0 && x > 0)             && bin[(y - 1) * w + (x - 1)];
            int NE = (y > 0 && x < w - 1)         && bin[(y - 1) * w + (x + 1)];
            int SW = (y < h - 1 && x > 0)         && bin[(y + 1) * w + (x - 1)];
            int SE = (y < h - 1 && x < w - 1)     && bin[(y + 1) * w + (x + 1)];

            int count = N + S + W + E + NW + NE + SW + SE;

            int is_start = 0;

            if (count == 1) {
                is_start = 1;
            }

            if (count == 2) {
                if (
                    (E && NE) || (E && SE) ||
                    (W && NW) || (W && SW) ||
                    (N && NE) || (N && NW) ||
                    (S && SE) || (S && SW)
                ) {
                    is_start = 1;
                }
            }

            if (is_start) {

                if (count_found == 0) {
                    *sx = x;
                    *sy = y;
                }
                else if (count_found == 1) {
                    *ex = x;
                    *ey = y;
                }

                count_found++;
            }
        }
    }

    if (count_found > 2) {
        printf("WARNING: More than 2 start pixels detected (%d)\n", count_found);
    }

    if (count_found == 2) {

	    if (*sx > *ex) {
	        int tx = *sx;
	        int ty = *sy;

	        *sx = *ex;
	        *sy = *ey;

	        *ex = tx;
	        *ey = ty;
	    }

	    // Gated the same as debug.c's Path Analysis/Arc Segmentation
	    // dumps below (see gSuppressTraceDebugPrints' own comment) --
	    // these two lines are printed from the SAME per-component
	    // trace pass, right before debugPrintPath, so they need the
	    // same suppression or the "quiet" Simulation-entry retrace
	    // would still start with this pair every time.
	    if (!gSuppressTraceDebugPrints)
	    {
	        printf("Start (RED): (%d, %d) (left)\n", *sx, *sy);
	        printf("End  (BLUE): (%d, %d)\n", *ex, *ey);
	    }
	}
	else if (count_found == 0) {
	    printf("No start/end pixels found\n");

	    *sx = -1;
	    *sy = -1;
	    *ex = -1;
	    *ey = -1;
	}
    else {
        printf("Start candidates found: %d\n", count_found);
    }

    return count_found;
}
```

### `tracePath` – path_trace.c, Zeilen 14–95 (Stand 28.08.2026 03:29)

```c
int tracePath(uint8_t *bin, int width, int height, int sx, int sy, int ex, int ey, Point path[], int maxPoints)
{
    // N, E, S, W, NE, SE, SW, NW

    uint8_t *visited = calloc(width * height, sizeof(uint8_t));
    if (visited == NULL)
        return 0;

    int x = sx;
    int y = sy;
    int count = 0;

    while (1)
    {
        if (count >= maxPoints)
            break;

        path[count].x = x;
        path[count].y = y;
        count++;

        visited[y * width + x] = 1;

        if (x == ex && y == ey)
            break;

        int found = 0;

        // First try 8-connected neighbors
        for (int k = 0; k < 8; k++)
        {
            int nx = x + NEIGHBORS[k].dx;
            int ny = y + NEIGHBORS[k].dy;

            if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                continue;

            int idx = ny * width + nx;

            if (bin[idx] && !visited[idx])
            {
                x = nx;
                y = ny;
                found = 1;
                break;
            }
        }

        if (found) continue;

        // NEW: If no 8-connected neighbor found, search for nearby pixels to bridge small gaps
        // (gaps created by the thinning algorithm in nearly-closed curves like circles)
        int foundBridge = 0;
        for (int dy = -3; dy <= 3 && !foundBridge; dy++) {
            for (int dx = -3; dx <= 3 && !foundBridge; dx++) {
                if (dx == 0 && dy == 0) continue;  // skip current position
                
                int nx = x + dx;
                int ny = y + dy;

                if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                    continue;

                int idx = ny * width + nx;

                if (bin[idx] && !visited[idx])
                {
                    x = nx;
                    y = ny;
                    foundBridge = 1;
                }
            }
        }

        if (!foundBridge)
            break;
    }

    free(visited);

    return count;
}
```

## 5 · Anpassen

Kasa-Kreisanpassung, rekursive Linien-/Bogenanpassung, Segmente bauen.

### `fitCircleToPoints` – geometry.c, Zeilen 167–223 (Stand 16.10.2026 13:52)

```c
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
```

### `recursiveArcFit` – geometry.c, Zeilen 552–618 (Stand 16.10.2026 13:52)

```c
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
    if (maxLineDeviation(pts, n) <= LINE_FIT_TOLERANCE) {
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
                               (arcSagitta(chordLen, (double)c.r) <= LINE_FIT_TOLERANCE);

    // A piece that is really two or three STRAIGHT LINES meeting at sharp
    // corners (see polylineCornerSplit) is a corner region, not an arc --
    // even if some small circle also happens to fit it within tolerance.
    // Without this, the corners of a stage built only from straight lines
    // came back as little rounded arcs/hooks. A genuinely curved stroke
    // fails this (its pieces are still curved) unless the arc is so small
    // that 2-3 lines already describe it within ARC_FIT_TOLERANCE anyway.
    int cornerIdx = polylineCornerSplit(pts, n);
    int isTwoLineCorner = (cornerIdx >= 0);

    double arcTol = ARC_FIT_TOLERANCE;       // 0 at the slider's Lines end: no arcs at all
    if (arcTol > 0.0 && ok && radiusReasonable && err <= arcTol && !curvatureNegligible && !isTwoLineCorner) {
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

    // Split right AT the corner for a 2-3 line corner region (clean corner,
    // no short chamfer piece straddling it); otherwise at the midpoint, as
    // before -- the midpoint split keeps smooth curves (e.g. an S-curve)
    // as few, long arcs.
    int mid = isTwoLineCorner ? cornerIdx : n / 2;
    // Shared joint point between the two halves, same as draw_to_arcspline.c
    if (!recursiveArcFit(pts, mid + 1, out, maxSegments, segCount)) return 0;
    if (!recursiveArcFit(pts + mid, n - mid, out, maxSegments, segCount)) return 0;
    return 1;
}
```

### `buildSegments` – geometry.c, Zeilen 620–778 (Stand 16.10.2026 13:52)

```c
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
            // Only merge while the combined run is STILL a straight line
            // within tolerance -- two line pieces meeting at an undetected
            // corner used to be merged into one line cutting straight
            // across that corner.
            int j = i;
            while (j + 1 < segCount && out[j + 1].circle.r == 0.0f && !noMergeAfter[j])
            {
                int runCount = (int)(out[j + 1].pts - out[i].pts) + out[j + 1].count;
                if (maxLineDeviation(out[i].pts, runCount) > LINE_FIT_TOLERANCE) break;
                j++;
            }

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

    // Remove corner chamfers: thinning sometimes turns a sharp corner into
    // a tiny diagonal stub (2-6 px), which the fitter then keeps as its own
    // short straight segment -- a visible bevel at a step corner, and one
    // extra segment. A very short straight segment sitting between two
    // longer straight segments that meet at a real corner (>= 30 deg) is
    // split in half and handed to its neighbours instead; canvas_bridge's
    // line refinement then places the corner exactly where the two fitted
    // lines intersect. Short pieces between near-parallel lines (a genuine
    // tiny step) and anything next to an arc are left alone.
    {
        int w = 0;
        for (int i = 0; i < segCount; i++)
        {
            ArcSegment* cur = &out[i];
            int isChamfer = 0;
            if (w > 0 && i + 1 < segCount && cur->circle.r == 0.0f && cur->count <= CHAMFER_MAX_POINTS)
            {
                ArcSegment* prev = &out[w - 1];
                ArcSegment* next = &out[i + 1];
                if (prev->circle.r == 0.0f && next->circle.r == 0.0f
                    && prev->count >= 2 * cur->count && next->count >= 2 * cur->count
                    && prev->pts + (prev->count - 1) == cur->pts          // contiguous on one traced path
                    && cur->pts + (cur->count - 1) == next->pts)
                {
                    double ax = prev->pts[prev->count - 1].x - prev->pts[0].x;
                    double ay = prev->pts[prev->count - 1].y - prev->pts[0].y;
                    double bx = next->pts[next->count - 1].x - next->pts[0].x;
                    double by = next->pts[next->count - 1].y - next->pts[0].y;
                    double la = sqrt(ax * ax + ay * ay), lb = sqrt(bx * bx + by * by);
                    if (la > 1e-6 && lb > 1e-6)
                    {
                        double c = (ax * bx + ay * by) / (la * lb);
                        if (c > 1.0) c = 1.0;
                        if (c < -1.0) c = -1.0;
                        double turn = acos(c) * (180.0 / CORNER_PI);
                        if (turn >= CHAMFER_MIN_CORNER_DEG) isChamfer = 1;
                    }
                }
            }

            if (isChamfer)
            {
                // Give the first half of the stub to prev and the rest to next
                // (the two keep sharing one pixel, as neighbours always do).
                int half = (cur->count - 1) / 2;
                ArcSegment* prev = &out[w - 1];
                prev->count += half;
                Point* newNextStart = cur->pts + half;
                ArcSegment* next = &out[i + 1];
                next->count += (int)(next->pts - newNextStart);
                next->pts = newNextStart;
                continue;                                   // drop the stub itself
            }

            if (w != i) out[w] = out[i];
            w++;
        }
        segCount = w;
    }

    for (int i = 0; i < segCount; i++)
    {
        out[i].start = (int)(out[i].pts - path);
        out[i].end   = out[i].start + out[i].count - 1;
    }

    return segCount;
}
```

## 6 · Abtasten

Jedes Segment in bis zu 24 Gleitkomma-Punkte zerlegen.

### `sampleArcPoints` – canvas_bridge.c, Zeilen 316–436 (Stand 24.09.2026 15:23)

```c
// s0x/s0y and s1x/s1y are the ink re-centering shifts (see ArcSegment's
// inkShift fields) to apply at the first and last point -- computed by
// computeEndShifts so neighbouring segments get the same shift at the
// pixel they share and still meet exactly.
static void sampleArcPoints(ArcSegment* seg, SampleF* outPts, int* outCount, int maxOut,
                            float s0x, float s0y, float s1x, float s1y)
{
    Circle c = seg->circle;
    c.r += seg->inkShiftR;   // ink re-centering (0 for straight segments)

    if (c.r < 1e-3f) {
        // degenerate/near-straight segment - no meaningful circle to sample
        // from, so fit a straight line instead: interpolate evenly between
        // the segment's two raw endpoint pixels (p0/p1), the same way the
        // circle branch below samples evenly by ANGLE around its fitted
        // circle rather than using raw traced pixels directly.
        //
        // This used to copy raw seg->pts[idx] pixels straight through.
        // Those are the THINNED SKELETON's actual integer pixel positions,
        // which -- for anything but a perfectly axis-aligned line -- zigzag
        // in a staircase pattern (a 1px-wide raster line can only ever move
        // in whole-pixel steps). canvas.c's ribbon renderer derives each
        // vertex's extrusion NORMAL from the direction to its neighboring
        // vertex; on a short/steep segment (few raw points, so little
        // averaging-out of the zigzag between consecutive sampled points)
        // that per-step direction is often purely horizontal or purely
        // vertical instead of the line's true diagonal direction. Extruding
        // by halfW along that wrong-angle normal is never WIDER than the
        // correct perpendicular extrusion (off by a cos(theta) <= 1
        // factor), so the ribbon reads as visibly thinner than the
        // original stroke -- exactly the "still thinner, but only for
        // straight lines" symptom, since width MEASUREMENT (avgRadiusPx,
        // pipeline.c) is unbiased and the circle branch below already
        // samples smooth analytic positions instead of raw pixels.
        Point p0 = seg->pts[0];
        Point p1 = seg->pts[seg->count - 1];
        int sampleCount = (maxOut < 24) ? maxOut : 24;
        if (sampleCount < 2) sampleCount = 2;

        float ax = (float)p0.x + s0x, ay = (float)p0.y + s0y;
        float bx = (float)p1.x + s1x, by = (float)p1.y + s1y;
        for (int i = 0; i < sampleCount; i++) {
            float t = (float)i / (float)(sampleCount - 1);
            outPts[i].x = ax + (bx - ax) * t;
            outPts[i].y = ay + (by - ay) * t;
        }
        *outCount = sampleCount;
        return;
    }

    Point p0 = seg->pts[0];
    Point p1 = seg->pts[seg->count - 1];

    // Sweep between the ends as they'll actually be drawn (pixel + shift;
    // a line-arc corner can move them a few pixels, see
    // refineStraightSegmentEnds), so the arc doesn't double back.
    double a0 = atan2(p0.y + s0y - c.cy, p0.x + s0x - c.cx);
    double a1 = atan2(p1.y + s1y - c.cy, p1.x + s1x - c.cx);

    double diff = a1 - a0;
    while (diff > ARC_PI)  diff -= 2.0 * ARC_PI;
    while (diff < -ARC_PI) diff += 2.0 * ARC_PI;

    // The shortest way from a0 to a1 is only right for arcs under 180 deg.
    // For a (near-)semicircle or a bigger arc it can pick the OTHER side of
    // the circle, drawing the curve mirrored across its chord (seen on an
    // S-curve's half-circles at some window sizes). The traced pixels know
    // which side the stroke actually went: if the middle traced pixel isn't
    // inside the short sweep, take the long way round instead.
    if (seg->count >= 3)
    {
        Point pm = seg->pts[seg->count / 2];
        double am = atan2(pm.y - c.cy, pm.x - c.cx);
        double rel = am - a0;                      // angle of the midpoint, measured from a0 in diff's direction
        if (diff < 0.0) rel = -rel;
        while (rel < 0.0)            rel += 2.0 * ARC_PI;
        while (rel >= 2.0 * ARC_PI)  rel -= 2.0 * ARC_PI;
        if (rel > fabs(diff))                      // midpoint lies outside the short sweep
            diff = (diff > 0.0) ? diff - 2.0 * ARC_PI : diff + 2.0 * ARC_PI;
    }

    int steps = (maxOut < 24) ? maxOut : 24;

    for (int i = 0; i < steps; i++) {
        double t = (double)i / (double)(steps - 1);
        double a = a0 + diff * t;
        // NEW: keep sub-pixel precision here - these come from an analytic
        // circle fit (float cx/cy/r), not from the traced pixel grid, so
        // there's no reason to snap them to whole source-image pixels.
        // Doing so used to be invisible for hand-drawn strokes (traced at
        // full window resolution, so 1px error is negligible) but very
        // visible for uploaded BMPs, where the curve can occupy only a
        // few dozen source pixels before being stretched to fill the
        // window - the same 1px snap becomes a visible staircase.
        outPts[i].x = (float)(c.cx + c.r * cos(a));
        outPts[i].y = (float)(c.cy + c.r * sin(a));
    }

    // NEW: force the two ends of this sampled arc to be the EXACT raw path
    // pixel (p0/p1), not their circle-projected position. p0/p1 are
    // themselves ordinary pixels from the single shared path buffer, and
    // buildSegments's recursive split always hands the two halves of a run
    // a literal shared boundary point (out[i].pts/out[i+1].pts overlap by
    // exactly that one Point) -- so the neighboring segment on either side
    // of this one has that exact same raw pixel as ITS p1/p0. But each
    // segment fits its OWN circle independently, and that fit is rarely
    // perfect (residual up to ARC_FIT_TOLERANCE), so projecting the shared
    // point onto two different circles yields two slightly different
    // world positions - a visible gap in the reconstructed ribbon
    // (segmentPointsWorld), since canvas.c draws each segment's ribbon
    // separately. Snapping both ends back to the one raw pixel they
    // actually share makes adjacent segments meet at a bit-identical
    // world coordinate; only the interior of the arc still follows the
    // fitted circle.
    outPts[0].x        = (float)p0.x + s0x;
    outPts[0].y        = (float)p0.y + s0y;
    outPts[steps - 1].x = (float)p1.x + s1x;
    outPts[steps - 1].y = (float)p1.y + s1y;

    *outCount = steps;
}
```
