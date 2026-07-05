#include "canvas.h"
#include <math.h>
#include <string.h>

HWND hWndGL = NULL;
int glWindowWidth = 800;
int glWindowHeight = 600;
GLuint fontBase = 0;

CanvasState canvas = { .zoom = 1.0f };

float segmentPointsWorld[MAX_SEGMENT_POINTS * 2];   // NEW
int   segmentStarts[MAX_ARC_SEGMENTS];              // NEW
int   segmentCounts[MAX_ARC_SEGMENTS];              // NEW

float segmentCircleCenterWorld[MAX_ARC_SEGMENTS * 2]; // NEW: ghost circle centers
float segmentCircleRadiusWorld[MAX_ARC_SEGMENTS];     // NEW: ghost circle radii

// NEW: which segment (if any) the mouse is currently hovering over.
// -1 means "none". Declared here (before ResetCanvas) since it's referenced there.
static int hoveredSegment = -1;

// NEW: state for the "hover top-right corner to reveal the UI panel" behavior
static BOOL hotZoneHighlighted = FALSE; // cursor is currently inside the corner hot zone
static BOOL uiShown            = FALSE; // panel is at least partially faded in
static int  uiAlpha             = 0;    // current fade alpha, 0 (invisible) - 255 (opaque)

void ResetCanvas(void)
{
    canvas.pointCount = 0;
    canvas.strokeCount = 0;
    canvas.hasBackgroundImage = FALSE;
    canvas.hasEndpointMarkers = FALSE;
    canvas.panX = 0.0f;
    canvas.panY = 0.0f;
    canvas.zoom = 1.0f;
    canvas.showSegments = FALSE;        // NEW
    canvas.segmentResultCount = 0;      // NEW
    hoveredSegment = -1;                // NEW: avoid a stale highlight index
	UpdateProjection();
}

GLuint canvasTexture = 0;

float bgLeft = -1.0f, bgRight = 1.0f, bgBottom = -1.0f, bgTop = 1.0f; // NEW

float markerStartX = 0.0f, markerStartY = 0.0f;
float markerEndX   = 0.0f, markerEndY   = 0.0f;

// NEW: pan state
static BOOL panning = FALSE;
static int  panLastX = 0, panLastY = 0;

float points[MAX_POINTS];
int strokeStarts[MAX_STROKES];
float strokeThickness[MAX_STROKES];
COLORREF strokeColor[MAX_STROKES];
BOOL drawing = FALSE;

static HGLRC hRC;
static HDC hDC;

static void segmentGhostColor(int index, float* r, float* g, float* b)
{
    // Cycle through a handful of distinguishable hues per segment
    static const float palette[6][3] = {
        {0.85f, 0.20f, 0.20f}, {0.20f, 0.55f, 0.85f}, {0.20f, 0.75f, 0.35f},
        {0.85f, 0.55f, 0.15f}, {0.60f, 0.30f, 0.80f}, {0.20f, 0.75f, 0.75f}
    };
    int i = index % 6;
    *r = palette[i][0];
    *g = palette[i][1];
    *b = palette[i][2];
}

static void drawMarkerDisc(float cx, float cy, float r, float red, float green, float blue)
{
    const int segments = 20;
    glColor3f(red, green, blue);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float theta = (2.0f * 3.14159265f * i) / segments;
        glVertex2f(cx + r * cosf(theta), cy + r * sinf(theta));
    }
    glEnd();
}

// Distance from a world-space point to a world-space line segment.
static float distPointToSegment(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax;
    float dy = by - ay;
    float len2 = dx * dx + dy * dy;

    if (len2 < 1e-9f) {
        float ddx = px - ax, ddy = py - ay;
        return sqrtf(ddx * ddx + ddy * ddy);
    }

    float t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float projX = ax + t * dx;
    float projY = ay + t * dy;
    float ddx = px - projX, ddy = py - projY;
    return sqrtf(ddx * ddx + ddy * ddy);
}

// Finds which segment's drawn arc strip is closest to a world-space point,
// within a small pick tolerance. Returns -1 if nothing is close enough.
static int findHoveredSegment(float wx, float wy)
{
    float tolerance = 0.05f * canvas.zoom; // pick radius, world units
    int best = -1;
    float bestDist = tolerance;

    for (int s = 0; s < canvas.segmentResultCount; s++)
    {
        int start = segmentStarts[s];
        int count = segmentCounts[s];
        if (count < 2) continue;

        for (int i = 0; i < count - 1; i++)
        {
            float ax = segmentPointsWorld[(start + i) * 2];
            float ay = segmentPointsWorld[(start + i) * 2 + 1];
            float bx = segmentPointsWorld[(start + i + 1) * 2];
            float by = segmentPointsWorld[(start + i + 1) * 2 + 1];

            float d = distPointToSegment(wx, wy, ax, ay, bx, by);
            if (d < bestDist) {
                bestDist = d;
                best = s;
            }
        }
    }

    return best;
}

void UpdateProjection(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)glWindowWidth / (float)glWindowHeight;
    if (aspect >= 1.0f) {
        glOrtho(-aspect * canvas.zoom, aspect * canvas.zoom, -1.0f * canvas.zoom, 1.0f * canvas.zoom, -1.0f, 1.0f);
    } else {
        glOrtho(-1.0f * canvas.zoom, 1.0f * canvas.zoom, -(1.0f / aspect) * canvas.zoom, (1.0f / aspect) * canvas.zoom, -1.0f, 1.0f);
    }
    glMatrixMode(GL_MODELVIEW);
}

// NEW: periodic check for the "hover top-right corner to reveal UI panel" behavior
#define UI_HOTZONE_TIMER_ID 1001
#define UI_HOTZONE_INTERVAL_MS 16    // ~60Hz - also drives the fade animation smoothness
#define UI_HOTZONE_WIDTH  48
#define UI_HOTZONE_HEIGHT 48
#define UI_FADE_STEP 18              // alpha change per tick (~14 ticks, ~230ms, to fully fade)

LRESULT CALLBACK WndProcGL(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_CREATE:
    {
        hDC = GetDC(hWnd);
        PIXELFORMATDESCRIPTOR pfd = {0};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 16;

        int pf = ChoosePixelFormat(hDC, &pfd);
        SetPixelFormat(hDC, pf, &pfd);
        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);

        HFONT hFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
        fontBase = glGenLists(96);
        wglUseFontBitmaps(hDC, 32, 96, fontBase);
        SelectObject(hDC, hOldFont);
        DeleteObject(hFont);

        UpdateProjection();
        glLoadIdentity();

        SetTimer(hWnd, UI_HOTZONE_TIMER_ID, UI_HOTZONE_INTERVAL_MS, NULL); // NEW
        return 0;
    }
    case WM_TIMER:
    {
        // NEW: reveal the UI panel when hovering the GL window's top-right
        // corner, or when the panel is already up and the cursor is over it
        // (so you don't lose it mid-click); hide it otherwise. Fades smoothly
        // instead of popping instantly.
        if (wParam == UI_HOTZONE_TIMER_ID && hWndUI)
        {
            POINT pt;
            GetCursorPos(&pt);

            // NEW: use the CLIENT area's top-right corner, not the full
            // window rect - GetWindowRect() includes the title bar, which
            // would (a) make the hot zone drift up into the title bar
            // instead of matching where the indicator is actually drawn,
            // and (b) place the panel over the title bar, covering the
            // close button.
            RECT glClientRect;
            GetClientRect(hWnd, &glClientRect);
            POINT clientTopRight = { glClientRect.right, glClientRect.top };
            ClientToScreen(hWnd, &clientTopRight);

            RECT hotZone;
            hotZone.right  = clientTopRight.x;
            hotZone.left   = clientTopRight.x - UI_HOTZONE_WIDTH;
            hotZone.top    = clientTopRight.y;
            hotZone.bottom = clientTopRight.y + UI_HOTZONE_HEIGHT;

            BOOL inHotZone = PtInRect(&hotZone, pt);

            if (inHotZone != hotZoneHighlighted)
            {
                hotZoneHighlighted = inHotZone;   // NEW: drives the corner indicator's look
                InvalidateRect(hWnd, NULL, FALSE);
            }

            BOOL inUIWindow = FALSE;
            if (uiShown)
            {
                RECT uiRect;
                GetWindowRect(hWndUI, &uiRect);
                inUIWindow = PtInRect(&uiRect, pt);
            }

            BOOL wantVisible = inHotZone || inUIWindow;

            if (wantVisible && !uiShown)
            {
                // Just entered the hot zone: position it flush against the
                // client area's top-right corner (below the title bar) and
                // start fully transparent, then fade in below.
                RECT uiRectCur;
                GetWindowRect(hWndUI, &uiRectCur);
                int uiW = uiRectCur.right - uiRectCur.left;

                uiAlpha = 0;
                SetLayeredWindowAttributes(hWndUI, 0, (BYTE)uiAlpha, LWA_ALPHA);
                SetWindowPos(hWndUI, HWND_TOPMOST,
                             clientTopRight.x - uiW, clientTopRight.y,
                             0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
                uiShown = TRUE;
            }

            if (uiShown)
            {
                int target = wantVisible ? 255 : 0;

                if (uiAlpha != target)
                {
                    if (uiAlpha < target) {
                        uiAlpha += UI_FADE_STEP;
                        if (uiAlpha > target) uiAlpha = target;
                    } else {
                        uiAlpha -= UI_FADE_STEP;
                        if (uiAlpha < target) uiAlpha = target;
                    }
                    SetLayeredWindowAttributes(hWndUI, 0, (BYTE)uiAlpha, LWA_ALPHA);
                }

                if (!wantVisible && uiAlpha <= 0)
                {
                    ShowWindow(hWndUI, SW_HIDE);
                    uiShown = FALSE;
                }
            }
        }
        return 0;
    }
    case WM_SIZE:
    {
        glWindowWidth = LOWORD(lParam);
        glWindowHeight = HIWORD(lParam);
        if (glWindowWidth == 0) glWindowWidth = 1;
        if (glWindowHeight == 0) glWindowHeight = 1;
        glViewport(0, 0, glWindowWidth, glWindowHeight);
        UpdateProjection();
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (zDelta > 0) canvas.zoom *= 0.9f;
        else            canvas.zoom *= 1.1f;
        if (canvas.zoom < 0.1f)  canvas.zoom = 0.1f;
        if (canvas.zoom > 10.0f) canvas.zoom = 10.0f;

        UpdateProjection();
        InvalidateRect(hWnd, NULL, FALSE);
        UpdateWindow(hWnd);
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        if (canvas.strokeCount >= MAX_STROKES) return 0;
        drawing = TRUE;
        strokeStarts[canvas.strokeCount] = canvas.pointCount;
        strokeColor[canvas.strokeCount] = brushColor;
        strokeThickness[canvas.strokeCount] = thickness;
        canvas.strokeCount++;

        float x = (float)LOWORD(lParam);
        float y = (float)HIWORD(lParam);
        float aspect = (float)glWindowWidth / (float)glWindowHeight;
        float nx, ny;
        if (aspect >= 1.0f) {
		    nx = ((2.0f * x / glWindowWidth) - 1.0f) * aspect * canvas.zoom;
		    ny = (1.0f - (2.0f * y / glWindowHeight)) * canvas.zoom;
		} else {
		    nx = ((2.0f * x / glWindowWidth) - 1.0f) * canvas.zoom;
		    ny = (1.0f - (2.0f * y / glWindowHeight)) * (1.0f / aspect) * canvas.zoom;
		}
		nx += canvas.panX;   // NEW
		ny += canvas.panY;   // NEW

		if (canvas.pointCount < MAX_POINTS - 1) {
		    points[canvas.pointCount++] = nx;
		    points[canvas.pointCount++] = ny;
		}
        return 0;
    }
    case WM_LBUTTONUP:
        drawing = FALSE;
        return 0;
	case WM_MBUTTONDOWN:
	{
	    panning = TRUE;
	    panLastX = LOWORD(lParam);
	    panLastY = HIWORD(lParam);
	    SetCapture(hWnd);
	    return 0;
	}
	case WM_MBUTTONUP:
	{
	    panning = FALSE;
	    ReleaseCapture();
	    return 0;
	}
    case WM_MOUSEMOVE:
	{
	    if (panning)
	    {
	        int mx = LOWORD(lParam);
	        int my = HIWORD(lParam);
	        int dx = mx - panLastX;
	        int dy = my - panLastY;

	        float aspect = (float)glWindowWidth / (float)glWindowHeight;
	        float worldPerPixelX, worldPerPixelY;

	        if (aspect >= 1.0f) {
	            worldPerPixelX = (2.0f * aspect * canvas.zoom) / glWindowWidth;
	            worldPerPixelY = (2.0f * canvas.zoom) / glWindowHeight;
	        } else {
	            worldPerPixelX = (2.0f * canvas.zoom) / glWindowWidth;
	            worldPerPixelY = (2.0f * (canvas.zoom / aspect)) / glWindowHeight;
	        }

	        canvas.panX -= dx * worldPerPixelX;
	        canvas.panY += dy * worldPerPixelY;

	        panLastX = mx;
	        panLastY = my;

	        InvalidateRect(hWnd, NULL, FALSE);
	        return 0;
	    }

	    if (!drawing || !(wParam & MK_LBUTTON))
	    {
	        // NEW: hover detection over the segment overlay when not
	        // actively drawing or panning
	        if (canvas.showSegments && canvas.segmentResultCount > 0)
	        {
	            float hx = (float)LOWORD(lParam);
	            float hy = (float)HIWORD(lParam);
	            float hAspect = (float)glWindowWidth / (float)glWindowHeight;
	            float hwx, hwy;
	            if (hAspect >= 1.0f) {
	                hwx = ((2.0f * hx / glWindowWidth) - 1.0f) * hAspect * canvas.zoom;
	                hwy = (1.0f - (2.0f * hy / glWindowHeight)) * canvas.zoom;
	            } else {
	                hwx = ((2.0f * hx / glWindowWidth) - 1.0f) * canvas.zoom;
	                hwy = (1.0f - (2.0f * hy / glWindowHeight)) * (1.0f / hAspect) * canvas.zoom;
	            }
	            hwx += canvas.panX;
	            hwy += canvas.panY;

	            int newHover = findHoveredSegment(hwx, hwy);
	            if (newHover != hoveredSegment)
	            {
	                hoveredSegment = newHover;
	                InvalidateRect(hWnd, NULL, FALSE);
	            }

	            // Needed to actually receive WM_MOUSELEAVE below
	            TRACKMOUSEEVENT tme = {0};
	            tme.cbSize = sizeof(tme);
	            tme.dwFlags = TME_LEAVE;
	            tme.hwndTrack = hWnd;
	            TrackMouseEvent(&tme);
	        }
	        else if (hoveredSegment != -1)
	        {
	            hoveredSegment = -1;
	            InvalidateRect(hWnd, NULL, FALSE);
	        }

	        return 0;
	    }

	    float x = (float)LOWORD(lParam);
	    float y = (float)HIWORD(lParam);
	    float aspect = (float)glWindowWidth / (float)glWindowHeight;
	    float nx, ny;
	    if (aspect >= 1.0f) {
	        nx = ((2.0f * x / glWindowWidth) - 1.0f) * aspect * canvas.zoom;
	        ny = (1.0f - (2.0f * y / glWindowHeight)) * canvas.zoom;
	    } else {
	        nx = ((2.0f * x / glWindowWidth) - 1.0f) * canvas.zoom;
	        ny = (1.0f - (2.0f * y / glWindowHeight)) * (1.0f / aspect) * canvas.zoom;
	    }
	    nx += canvas.panX;   // NEW: store true world coords, independent of current pan
	    ny += canvas.panY;   // NEW

	    if (canvas.pointCount < MAX_POINTS - 1) {
	        points[canvas.pointCount++] = nx;
	        points[canvas.pointCount++] = ny;
	        InvalidateRect(hWnd, NULL, FALSE);
	    }
	    return 0;
	}
	case WM_MOUSELEAVE:
	{
	    if (hoveredSegment != -1)
	    {
	        hoveredSegment = -1;
	        InvalidateRect(hWnd, NULL, FALSE);
	    }
	    return 0;
	}
	case WM_PAINT:
	{
	    PAINTSTRUCT ps;
	    BeginPaint(hWnd, &ps);
	    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	    glMatrixMode(GL_MODELVIEW);
	    glLoadIdentity();
	    glTranslatef(-canvas.panX, -canvas.panY, 0.0f);   // NEW: apply camera pan to everything below

	    if (canvas.hasBackgroundImage)
	    {
	        // FIXED bounds (computed once at upload time) — canvas.zoom now
	        // actually affects this via the ortho projection, same as strokes
	        glEnable(GL_TEXTURE_2D);
	        glBindTexture(GL_TEXTURE_2D, canvasTexture);
	        glColor3f(1.0f, 1.0f, 1.0f);

	        glBegin(GL_QUADS);
	            glTexCoord2f(0.0f, 0.0f); glVertex2f(bgLeft,  bgBottom);
	            glTexCoord2f(1.0f, 0.0f); glVertex2f(bgRight, bgBottom);
	            glTexCoord2f(1.0f, 1.0f); glVertex2f(bgRight, bgTop);
	            glTexCoord2f(0.0f, 1.0f); glVertex2f(bgLeft,  bgTop);
	        glEnd();

	        glDisable(GL_TEXTURE_2D);
	    }

	    glEnable(GL_BLEND);
	    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (int s = 0; s < canvas.strokeCount; s++)
        {
            int start = strokeStarts[s];
            int end = (s == canvas.strokeCount - 1) ? canvas.pointCount : strokeStarts[s + 1];
            int count = (end - start) / 2;
            if (count < 2) continue;

            COLORREF c = strokeColor[s];
            glColor3f(GetRValue(c)/255.0f, GetGValue(c)/255.0f, GetBValue(c)/255.0f);

            float halfW = (strokeThickness[s] * canvas.zoom) / (float)glWindowWidth;

            glBegin(GL_TRIANGLE_STRIP);
            for (int i = 0; i < count; i++)
            {
                float x = points[start + i * 2];
                float y = points[start + i * 2 + 1];
                float dx = 0.0f, dy = 0.0f;

                if (i == 0) {
                    dx = points[start + (i + 1) * 2] - x;
                    dy = points[start + (i + 1) * 2 + 1] - y;
                } else if (i == count - 1) {
                    dx = x - points[start + (i - 1) * 2];
                    dy = y - points[start + (i - 1) * 2 + 1];
                } else {
                    float dx1 = x - points[start + (i - 1) * 2];
                    float dy1 = y - points[start + (i - 1) * 2 + 1];
                    float dx2 = points[start + (i + 1) * 2] - x;
                    float dy2 = points[start + (i + 1) * 2 + 1] - y;
                    dx = dx1 + dx2;
                    dy = dy1 + dy2;
                }

                float len = sqrtf(dx * dx + dy * dy);
                if (len == 0.0f) len = 1.0f;
                float nx = -dy / len;
                float ny = dx / len;

                glVertex2f(x + nx * halfW, y + ny * halfW);
                glVertex2f(x - nx * halfW, y - ny * halfW);
            }
            glEnd();
        }
        glDisable(GL_BLEND);

		if (canvas.showSegments && canvas.segmentResultCount > 0)
		{
		    glEnable(GL_BLEND);
		    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		    float ghostHalfW = 0.01f * canvas.zoom;
		    float ghostAlpha = 0.35f;   // "ghost" = translucent, tweak to taste

		    for (int s = 0; s < canvas.segmentResultCount; s++)
		    {
		        int start = segmentStarts[s];
		        int count = segmentCounts[s];
		        if (count < 2) continue;

		        BOOL isHovered = (s == hoveredSegment);   // NEW

		        float r, g, b;
		        segmentGhostColor(s, &r, &g, &b);
		        glColor4f(r, g, b, isHovered ? 0.9f : ghostAlpha);   // NEW: brighten on hover

		        float halfW = isHovered ? ghostHalfW * 1.8f : ghostHalfW;   // NEW: thicken on hover

		        glBegin(GL_TRIANGLE_STRIP);
		        for (int i = 0; i < count; i++)
		        {
		            float x = segmentPointsWorld[(start + i) * 2];
		            float y = segmentPointsWorld[(start + i) * 2 + 1];
		            float dx = 0.0f, dy = 0.0f;

		            if (i == 0) {
		                dx = segmentPointsWorld[(start + i + 1) * 2] - x;
		                dy = segmentPointsWorld[(start + i + 1) * 2 + 1] - y;
		            } else if (i == count - 1) {
		                dx = x - segmentPointsWorld[(start + i - 1) * 2];
		                dy = y - segmentPointsWorld[(start + i - 1) * 2 + 1];
		            } else {
		                dx = segmentPointsWorld[(start + i + 1) * 2] - segmentPointsWorld[(start + i - 1) * 2];
		                dy = segmentPointsWorld[(start + i + 1) * 2 + 1] - segmentPointsWorld[(start + i - 1) * 2 + 1];
		            }

		            float len = sqrtf(dx * dx + dy * dy);
		            if (len == 0.0f) len = 1.0f;
		            float nx = -dy / len;
		            float ny = dx / len;

		            glVertex2f(x + nx * halfW, y + ny * halfW);
		            glVertex2f(x - nx * halfW, y - ny * halfW);
		        }
		        glEnd();
		    }

		    glDisable(GL_BLEND);
		}

		// NEW: ghost circles - the FULL circle each arc segment was cut from
		if (canvas.showSegments && canvas.segmentResultCount > 0)
		{
		    glEnable(GL_BLEND);
		    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		    const int circleSteps = 64;

		    for (int s = 0; s < canvas.segmentResultCount; s++)
		    {
		        float r = segmentCircleRadiusWorld[s];
		        if (r <= 0.0f) continue; // straight/degenerate segment - no circle to show

		        float cx = segmentCircleCenterWorld[s * 2];
		        float cy = segmentCircleCenterWorld[s * 2 + 1];

		        BOOL isHovered = (s == hoveredSegment);   // NEW

		        float gr, gg, gb;
		        segmentGhostColor(s, &gr, &gg, &gb);

		        if (isHovered) {
		            glDisable(GL_LINE_STIPPLE);          // solid outline when hovered
		            glLineWidth(2.5f);
		            glColor4f(gr, gg, gb, 1.0f);
		        } else {
		            glEnable(GL_LINE_STIPPLE);
		            glLineStipple(1, 0x00FF);            // dotted outline otherwise
		            glLineWidth(1.0f);
		            glColor4f(gr, gg, gb, 0.6f);
		        }

		        glBegin(GL_LINE_LOOP);
		        for (int i = 0; i < circleSteps; i++) {
		            float theta = (2.0f * 3.14159265f * i) / circleSteps;
		            glVertex2f(cx + r * cosf(theta), cy + r * sinf(theta));
		        }
		        glEnd();
		    }

		    glLineWidth(1.0f);
		    glDisable(GL_LINE_STIPPLE);
		    glDisable(GL_BLEND);
		}

	    if (canvas.hasEndpointMarkers)
	    {
	        float markerRadius = 0.02f * canvas.zoom;
	        drawMarkerDisc(markerStartX, markerStartY, markerRadius, 1.0f, 0.0f, 0.0f);
	        drawMarkerDisc(markerEndX,   markerEndY,   markerRadius, 0.0f, 0.0f, 1.0f);
	    }

        // --- BLINK-FREE UI TEXT DRAWING ---
        int zoomPercent = (int)(100.0f / canvas.zoom);
        char zoomStr[32];
        wsprintfA(zoomStr, "Zoom: %d%%", zoomPercent);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix(); glLoadIdentity();
        glOrtho(0, glWindowWidth, 0, glWindowHeight, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix(); glLoadIdentity();

        // NEW: corner hover indicator - shows exactly where to hover to
        // reveal the UI panel, and brightens while you're hovering it.
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float tabLeft   = (float)(glWindowWidth - UI_HOTZONE_WIDTH);
            float tabRight  = (float)glWindowWidth;
            float tabBottom = (float)(glWindowHeight - UI_HOTZONE_HEIGHT);
            float tabTop    = (float)glWindowHeight;

            float fillA = hotZoneHighlighted ? 0.30f : 0.10f;
            if (hotZoneHighlighted)
                glColor4f(0.25f, 0.55f, 0.95f, fillA);
            else
                glColor4f(0.4f, 0.4f, 0.4f, fillA);

            glBegin(GL_QUADS);
                glVertex2f(tabLeft,  tabBottom);
                glVertex2f(tabRight, tabBottom);
                glVertex2f(tabRight, tabTop);
                glVertex2f(tabLeft,  tabTop);
            glEnd();

            // Small drawer-handle icon: three short horizontal bars
            float shade = hotZoneHighlighted ? 0.95f : 0.55f;
            glColor4f(shade, shade, shade, 0.9f);
            float cx = (tabLeft + tabRight) * 0.5f;
            float cy = (tabBottom + tabTop) * 0.5f;
            float barHalfW = (float)UI_HOTZONE_WIDTH * 0.22f;
            for (int i = -1; i <= 1; i++)
            {
                float by = cy + i * 7.0f;
                glBegin(GL_QUADS);
                    glVertex2f(cx - barHalfW, by - 1.5f);
                    glVertex2f(cx + barHalfW, by - 1.5f);
                    glVertex2f(cx + barHalfW, by + 1.5f);
                    glVertex2f(cx - barHalfW, by + 1.5f);
                glEnd();
            }

            glDisable(GL_BLEND);
        }

        glColor3f(0.3f, 0.3f, 0.3f);
        glRasterPos2i(glWindowWidth - 90, 20);
        glPushAttrib(GL_LIST_BIT);
        glListBase(fontBase - 32);
        glCallLists((GLsizei)strlen(zoomStr), GL_UNSIGNED_BYTE, zoomStr);
        glPopAttrib();
        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW); glPopMatrix();

        SwapBuffers(hDC);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: KillTimer(hWnd, UI_HOTZONE_TIMER_ID); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
