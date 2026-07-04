#include "canvas.h"
#include <math.h>
#include <string.h>

HWND hWndGL = NULL;
int glWindowWidth = 800;
int glWindowHeight = 600;
GLuint fontBase = 0;

void ResetCanvas(void)
{
    canvas.pointCount = 0;
    canvas.strokeCount = 0;
    canvas.hasBackgroundImage = FALSE;
    canvas.hasEndpointMarkers = FALSE;
    canvas.panX = 0.0f;
    canvas.panY = 0.0f;
    canvas.zoom = 1.0f;
}

CanvasState canvas = { .zoom = 1.0f };

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

	    if (!drawing || !(wParam & MK_LBUTTON)) return 0;

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
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
