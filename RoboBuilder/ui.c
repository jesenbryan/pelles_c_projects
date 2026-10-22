#include "ui.h"
#include "pipeline.h"      // NEW
#include "canvas_bridge.h" // NEW: for canvasToImage()
#include "bmp_ui.h"        // NEW: for saveBMP_UI() and BMP_RGB
#include "debug.h"         // For ClearConsoleLog -- the Clear button also wipes the debug console
#include "geometry.h"      // For gArcLineBias -- the Arcs/Lines fit slider
#include <commctrl.h>
#include <stdlib.h>      // malloc/free for the anti-aliased fit icons

HWND hWndUI = NULL;
float thickness = 2.0f;
COLORREF brushColor = RGB(0, 0, 0);

static HWND hClearBtn;
static HWND hUndoBtn;       // removes the last stroke (also Ctrl+Z)
static HWND hSlider;
static HWND hColorBtn;
static HWND hThicknessLabel;
static HWND hViewSegBtn;   // NEW
static HWND hComparisonBtn;  // NEW: toggle comparison mode
static HWND hFitLabel;       // state readout above the fit slider ("balanced", "lines only", ...)
static HWND hFitSlider;      // arc-vs-line preference for tracing (gArcLineBias)
static HWND hFitArcIcon;     // owner-drawn arc symbol at the slider's left (arcs) end
static HWND hFitLineIcon;    // owner-drawn straight-lines symbol at its right (lines) end
static int controlCount = 8;   // sizing estimate (labels are shorter than buttons)

// Fit slider: FIT_SLIDER_STEPS + 1 positions in 25% steps, left = arcs,
// right = lines, middle = default. Position p maps to
// gArcLineBias = p / FIT_SLIDER_STEPS.
#define FIT_SLIDER_STEPS 4
#define FIT_ICON_SIZE    20
#define ID_FIT_ARC_ICON  901    // child ids only so WM_DRAWITEM can tell the two icons apart
#define ID_FIT_LINE_ICON 902

static void updateFitLabel(int pos)
{
    static const wchar_t* names[FIT_SLIDER_STEPS + 1] = {
        L"max arcs", L"more arcs", L"balanced", L"more lines", L"lines only"
    };
    if (pos < 0) pos = 0;
    if (pos > FIT_SLIDER_STEPS) pos = FIT_SLIDER_STEPS;
    SetWindowText(hFitLabel, names[pos]);
}

// The two end symbols (the panel's default font has no suitable glyphs, so
// they're drawn): a half circle for "arcs", a zigzag of straight strokes
// for "lines".
//
// Plain GDI draws without anti-aliasing, which made the symbols look jagged.
// So they're drawn FIT_ICON_SS times larger (black on white) into an
// off-screen bitmap, then shrunk by averaging each SSxSS block into a
// coverage value that blends the text colour over the button-face colour.
// The result is a smooth, anti-aliased symbol at the original size.
#define FIT_ICON_SS 8

static void drawFitIconShape(HDC dc, UINT id, int w, int h, int S)
{
    if (id == ID_FIT_ARC_ICON)
    {
        // Upper half of a circle, open at the bottom (Arc runs counter-
        // clockwise from the start point on the right to the end point on
        // the left, i.e. over the top).
        int R  = (w - 6) * S / 2;
        int cx = w * S / 2, cy = h * S * 3 / 4;
        Arc(dc, cx - R, cy - R, cx + R, cy + R, cx + R, cy, cx - R, cy);
    }
    else
    {
        // Three straight strokes meeting at sharp corners.
        POINT p[4] = {
            { 3 * S,             h * S * 3 / 4 },
            { w * S / 3,         h * S / 4 },
            { w * S * 2 / 3,     h * S * 3 / 4 },
            { (w - 3) * S,       h * S / 4 }
        };
        Polyline(dc, p, 4);
    }
}

static void drawFitIcon(const DRAWITEMSTRUCT* dis)
{
    HDC dc = dis->hDC;
    RECT r = dis->rcItem;
    int w = r.right - r.left, h = r.bottom - r.top;
    FillRect(dc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
    if (w <= 0 || h <= 0) return;

    const int S = FIT_ICON_SS;
    int W = w * S, H = h * S;

    // Large 32-bit top-down bitmap to draw into.
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = W;
    bi.bmiHeader.biHeight      = -H;          // negative = top-down rows
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP big = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    unsigned char* out = (unsigned char*)malloc((size_t)w * h * 4);
    if (!mem || !big || !bits || !out)
    {
        if (out) free(out);
        if (big) DeleteObject(big);
        if (mem) DeleteDC(mem);
        return;   // leave the plain background rather than draw nothing sensible
    }
    HBITMAP oldBmp = (HBITMAP)SelectObject(mem, big);

    RECT all = { 0, 0, W, H };
    FillRect(mem, &all, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // Same 2 px stroke as before (scaled up), with round ends and joins so
    // the shrunk result has soft, even line ends.
    LOGBRUSH lb = { BS_SOLID, RGB(0, 0, 0), 0 };
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
                            2 * S, &lb, 0, NULL);
    HPEN oldPen = (HPEN)SelectObject(mem, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
    drawFitIconShape(mem, dis->CtlID, w, h, S);
    SelectObject(mem, oldBrush);
    SelectObject(mem, oldPen);
    DeleteObject(pen);
    GdiFlush();   // make sure GDI has finished writing into bits

    // Shrink: ink coverage of each SSxSS block -> blend fg over bg.
    COLORREF bg = GetSysColor(COLOR_BTNFACE), fg = GetSysColor(COLOR_BTNTEXT);
    int bgB = GetBValue(bg), bgG = GetGValue(bg), bgR = GetRValue(bg);
    int fgB = GetBValue(fg), fgG = GetGValue(fg), fgR = GetRValue(fg);
    const unsigned char* src = (const unsigned char*)bits;   // BGRA rows
    int area = S * S;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int ink = 0;
            for (int sy = 0; sy < S; sy++)
            {
                const unsigned char* row = src + ((size_t)(y * S + sy) * W + (size_t)x * S) * 4;
                for (int sx = 0; sx < S; sx++)
                    ink += 255 - row[sx * 4];   // blue channel: 0 = ink, 255 = paper
            }
            int a = (ink + area / 2) / area;      // 0..255 coverage
            unsigned char* d = out + ((size_t)y * w + x) * 4;
            d[0] = (unsigned char)(bgB + (fgB - bgB) * a / 255);
            d[1] = (unsigned char)(bgG + (fgG - bgG) * a / 255);
            d[2] = (unsigned char)(bgR + (fgR - bgR) * a / 255);
            d[3] = 0;
        }
    }

    BITMAPINFO bo = bi;
    bo.bmiHeader.biWidth  = w;
    bo.bmiHeader.biHeight = -h;
    SetDIBitsToDevice(dc, r.left, r.top, (DWORD)w, (DWORD)h, 0, 0, 0, (UINT)h,
                      out, &bo, DIB_RGB_COLORS);

    SelectObject(mem, oldBmp);
    DeleteObject(big);
    DeleteDC(mem);
    free(out);
}


static int GetRequiredUIHeight(void)
{
    int btnH = 30;
    int spacing = 10;
    int contentH = (btnH * controlCount) + (spacing * (controlCount + 1));
    return contentH + 20;
}

static void AutoResizeUI(HWND hWnd)
{
    int newClientW = 220;
    int newClientH = GetRequiredUIHeight();
    RECT r = {0, 0, newClientW, newClientH};
    AdjustWindowRect(&r, GetWindowLong(hWnd, GWL_STYLE), FALSE);
    SetWindowPos(hWnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
}

static void LayoutUI(HWND hWnd)
{
    RECT r;
    GetClientRect(hWnd, &r);
    int width = r.right;
    int btnW = 180;
    int btnH = 30;
    int spacing = 10;
    int centerX = (width - btnW) / 2;
    int y = (r.bottom - ((btnH * controlCount) + (spacing * (controlCount + 1)))) / 2;
    if (y < 10) y = 10;

    MoveWindow(hClearBtn, centerX, y, btnW, btnH, TRUE);
    y += btnH + spacing;
    MoveWindow(hUndoBtn, centerX, y, btnW, btnH, TRUE);
    y += btnH + spacing;
    MoveWindow(hThicknessLabel, centerX, y, btnW, 20, TRUE);
    y += 20 + 2;
    MoveWindow(hSlider, centerX, y, btnW, btnH, TRUE);
    y += btnH + spacing;
    MoveWindow(hColorBtn, centerX, y, btnW, btnH, TRUE);
	y += btnH + spacing;
	MoveWindow(hViewSegBtn, centerX, y, btnW, btnH, TRUE);
	y += btnH + spacing;
	MoveWindow(hComparisonBtn, centerX, y, btnW, btnH, TRUE);
	y += btnH + spacing;
	MoveWindow(hFitLabel, centerX, y, btnW, 20, TRUE);
	y += 20 + 2;
	// [arc symbol] [slider] [lines symbol] on one row; the symbols sit level
	// with the slider's channel (the tick marks hang below it).
	MoveWindow(hFitArcIcon, centerX, y + 1, FIT_ICON_SIZE, FIT_ICON_SIZE, TRUE);
	MoveWindow(hFitSlider, centerX + FIT_ICON_SIZE + 2, y, btnW - 2 * (FIT_ICON_SIZE + 2), btnH, TRUE);
	MoveWindow(hFitLineIcon, centerX + btnW - FIT_ICON_SIZE, y + 1, FIT_ICON_SIZE, FIT_ICON_SIZE, TRUE);
}

LRESULT CALLBACK WndProcUI(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_CREATE:
    {
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
        InitCommonControlsEx(&icc);

        hClearBtn = CreateWindowEx(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE, 20, 20, 300, 30, hWnd, (HMENU)ID_CLEAR, GetModuleHandle(NULL), NULL);
        hUndoBtn = CreateWindowEx(0, L"BUTTON", L"Undo (Ctrl+Z)", WS_CHILD | WS_VISIBLE, 20, 20, 300, 30, hWnd, (HMENU)ID_UNDO, GetModuleHandle(NULL), NULL);
        hThicknessLabel = CreateWindowEx(0, L"STATIC", L"Thickness: 2 px", WS_CHILD | WS_VISIBLE, 20, 60, 300, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
        hSlider   = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ, 20, 80, 300, 40, hWnd, NULL, GetModuleHandle(NULL), NULL);
        hColorBtn = CreateWindowEx(0, L"BUTTON", L"Color", WS_CHILD | WS_VISIBLE, 20, 130, 300, 30, hWnd, (HMENU)ID_COLOR, GetModuleHandle(NULL), NULL);

		// BS_AUTOCHECKBOX | BS_PUSHLIKE: a checkbox drawn/behaving like a button
		// that stays visually pressed while checked - gives "View Segments" a
		// native on/off toggle look instead of a plain momentary click. (The
		// separate "Trace" button that used to sit here was removed -- View
		// Segments and Comparison Mode both already trace on demand the first
		// time they're checked with nothing traced yet, see their own
		// ID_VIEW_SEGMENTS/ID_COMPARISON handling below, so the standalone
		// button was redundant.)
		hViewSegBtn = CreateWindowEx(0, L"BUTTON", L"View Segments",
		                              WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
		                              20, 250, 300, 30, hWnd, (HMENU)ID_VIEW_SEGMENTS,
		                              GetModuleHandle(NULL), NULL);

		hComparisonBtn = CreateWindowEx(0, L"BUTTON", L"Comparison Mode",
		                                 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
		                                 20, 290, 300, 30, hWnd, (HMENU)ID_COMPARISON,
		                                 GetModuleHandle(NULL), NULL);

        // Arcs / Lines fit preference (see gArcLineBias in geometry.h):
        // left = reconstruct with arcs as much as possible, right = straight
        // lines only, middle (default) = the normal balance.
        hFitLabel  = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 330, 300, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
        hFitSlider = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ, 20, 350, 300, 40, hWnd, NULL, GetModuleHandle(NULL), NULL);
        hFitArcIcon  = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 350, FIT_ICON_SIZE, FIT_ICON_SIZE, hWnd, (HMENU)ID_FIT_ARC_ICON, GetModuleHandle(NULL), NULL);
        hFitLineIcon = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 350, FIT_ICON_SIZE, FIT_ICON_SIZE, hWnd, (HMENU)ID_FIT_LINE_ICON, GetModuleHandle(NULL), NULL);
        SendMessage(hFitSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, FIT_SLIDER_STEPS));
        SendMessage(hFitSlider, TBM_SETTICFREQ, 1, 0);
        SendMessage(hFitSlider, TBM_SETPAGESIZE, 0, 1);   // clicking the channel moves one 25% step
        {
            int pos = (int)(gArcLineBias * FIT_SLIDER_STEPS + 0.5f);
            SendMessage(hFitSlider, TBM_SETPOS, TRUE, pos);
            updateFitLabel(pos);
        }

        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(1, 10));
        SendMessage(hSlider, TBM_SETPOS, TRUE, (LPARAM)thickness);

        wchar_t buf[32];
        wsprintf(buf, L"Thickness: %d px", (int)thickness);
        SetWindowText(hThicknessLabel, buf);

        AutoResizeUI(hWnd);
        LayoutUI(hWnd);
        return 0;
    }
    case WM_HSCROLL:
    {
        if ((HWND)lParam == hSlider && hSlider != NULL)
        {
            thickness = (float)SendMessage(hSlider, TBM_GETPOS, 0, 0);
            wchar_t buf[32];
            wsprintf(buf, L"Thickness: %d px", (int)thickness);
            SetWindowText(hThicknessLabel, buf);
            if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
        }
        else if ((HWND)lParam == hFitSlider && hFitSlider != NULL)
        {
            int pos = (int)SendMessage(hFitSlider, TBM_GETPOS, 0, 0);
            float bias = (float)pos / (float)FIT_SLIDER_STEPS;
            if (bias != gArcLineBias)
            {
                gArcLineBias = bias;
                updateFitLabel(pos);
                // Re-trace right away if the reconstruction is on screen,
                // so the effect shows while dragging. Otherwise it simply
                // applies to the next trace (View Segments, Comparison
                // Mode, entering Simulation).
                if ((canvas.showSegments || canvas.comparisonMode) && canvas.strokeCount > 0)
                    RunTracePipeline();
                if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
            }
        }
        return 0;
    }
    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (dis && (dis->CtlID == ID_FIT_ARC_ICON || dis->CtlID == ID_FIT_LINE_ICON))
        {
            drawFitIcon(dis);
            return TRUE;
        }
        break;
    }
    case WM_SIZE:
        LayoutUI(hWnd);
        return 0;
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_CLEAR)
		{
		    // Clear also wipes the debug console, same as Simulation's
		    // Reset button -- a fresh drawing starts with a fresh log.
		    ClearConsoleLog();
		    ClearCanvasWithUndo();   // ResetCanvas + keeps a copy so Undo can bring it back
		    SendMessage(hViewSegBtn, BM_SETCHECK, BST_UNCHECKED, 0);
		    SendMessage(hComparisonBtn, BM_SETCHECK, BST_UNCHECKED, 0);
		    if (hWndGL) InvalidateRect(hWndGL, NULL, TRUE);
		}
        else if (LOWORD(wParam) == ID_UNDO)
        {
            UndoLastStroke();
            // Hand keyboard focus back to the canvas so Ctrl+Z keeps working.
            if (hWndGL) SetFocus(hWndGL);
        }
        else if (LOWORD(wParam) == ID_COLOR)
        {
            CHOOSECOLOR cc = {0};
            static COLORREF customColors[16];
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hWnd;
            cc.lpCustColors = customColors;
            cc.rgbResult = brushColor;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc)) brushColor = cc.rgbResult;
        }
		else if (LOWORD(wParam) == ID_VIEW_SEGMENTS)
		{
		    // BS_AUTOCHECKBOX already flipped its own check state before this
		    // notification fires, so read it back rather than tracking a
		    // separate bool - the button IS the toggle state.
		    BOOL nowChecked = (SendMessage(hViewSegBtn, BM_GETCHECK, 0, 0) == BST_CHECKED);

		    if (nowChecked)
		    {
		        // ALWAYS retrace on check, not just the first time
		        // (canvas.segmentResultCount == 0) -- the old "only if
		        // nothing traced yet" gate meant re-checking View Segments
		        // after drawing more of the contour (or editing/erasing
		        // part of it) kept showing the STALE trace from before
		        // those changes, since nothing else re-traces on its own.
		        // This also still covers the original "nothing traced
		        // yet" case for free (canvas drawing takes priority, falls
		        // back to a pending uploaded BMP), so View Segments still
		        // works standalone without a separate Trace step first.
		        RunTracePipeline();
		    }
		    canvas.showSegments = nowChecked;
		    if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
		}
		else if (LOWORD(wParam) == ID_COMPARISON)
		{
		    // Toggle between showing original strokes vs. reconstructed arc line
		    BOOL nowChecked = (SendMessage(hComparisonBtn, BM_GETCHECK, 0, 0) == BST_CHECKED);

		    if (nowChecked)
		    {
		        // Same as View Segments above: ALWAYS retrace on check
		        // (not just when nothing's been traced yet), so re-
		        // checking Comparison Mode after drawing more never shows
		        // a stale reconstruction either -- and it still covers
		        // "usable on its own, without needing View Segments
		        // pressed first" the same way it always did.
		        RunTracePipeline();
		    }
		    canvas.comparisonMode = nowChecked;
		    if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
		}
        return 0;   
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void createUIWindow(HINSTANCE hInst, HWND hWndParent)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProcUI;
    wc.hInstance = hInst;
    wc.lpszClassName = L"SemniUIPanel";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // WS_POPUP (no title bar/border -- a floating tool panel, not a real
    // top-level window) + WS_EX_LAYERED (canvas.c's WM_TIMER fades it in/
    // out via SetLayeredWindowAttributes) + WS_EX_TOOLWINDOW (keeps it off
    // the taskbar/alt-tab, matching what a hover-reveal panel should be).
    // Owned by hWndParent (not a child of it -- it needs to float outside
    // the parent's client rect and receive its own WM_COMMAND from its
    // buttons) so it stays above and closes alongside the main window.
    //
    // Starts with WS_VISIBLE omitted and alpha 0: canvas.c's WM_TIMER
    // hot-zone check is what actually reveals it (see uiShown/uiAlpha),
    // this just needs to exist first.
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        L"SemniUIPanel",
        L"",
        WS_POPUP,
        0, 0, 220, 100,
        hWndParent, NULL,
        hInst, NULL
    );

    SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);

    hWndUI = hwnd;
}

// See ui.h for why this exists instead of a raw canvas.comparisonMode
// write -- mirrors the ID_COMPARISON WM_COMMAND handler's own logic
// (trace on demand if nothing's been traced yet, keep the checkbox in
// sync) so callers outside this file get the exact same behavior a real
// click on the button would produce.
// Same idea as SetComparisonModeUI below, for View Segments: sets
// canvas.showSegments and keeps the checkbox in sync. Re-traces when
// switching on, same as clicking the checkbox.
void SetViewSegmentsUI(BOOL on)
{
    if (on)
        RunTracePipeline();

    canvas.showSegments = on;

    if (hViewSegBtn)
        SendMessage(hViewSegBtn, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);

    if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
}

void SetComparisonModeUI(BOOL on)
{
    if (on && canvas.segmentResultCount == 0)
        RunTracePipeline();

    canvas.comparisonMode = on;

    if (hComparisonBtn)
        SendMessage(hComparisonBtn, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);

    if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
}
