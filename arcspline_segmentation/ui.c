#include "ui.h"
#include "pipeline.h"      // NEW
#include <commctrl.h>

HWND hWndUI = NULL;
float thickness = 2.0f;
COLORREF brushColor = RGB(0, 0, 0);

static HWND hClearBtn;
static HWND hSlider;
static HWND hColorBtn;
static HWND hTraceBtn;
static HWND hUploadBtn;      // NEW
static HWND hThicknessLabel;
static HWND hViewSegBtn;   // NEW
static int controlCount = 7; // was 6


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
    MoveWindow(hThicknessLabel, centerX, y, btnW, 20, TRUE);
    y += 20 + 2;
    MoveWindow(hSlider, centerX, y, btnW, btnH, TRUE);
    y += btnH + spacing;
    MoveWindow(hColorBtn, centerX, y, btnW, btnH, TRUE);
	y += btnH + spacing;
	MoveWindow(hTraceBtn, centerX, y, btnW, btnH, TRUE);
	y += btnH + spacing;
	MoveWindow(hViewSegBtn, centerX, y, btnW, btnH, TRUE);
	y += btnH + spacing;
	MoveWindow(hUploadBtn, centerX, y, btnW, btnH, TRUE);
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
        hThicknessLabel = CreateWindowEx(0, L"STATIC", L"Thickness: 2 px", WS_CHILD | WS_VISIBLE, 20, 60, 300, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
        hSlider   = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ, 20, 80, 300, 40, hWnd, NULL, GetModuleHandle(NULL), NULL);
        hColorBtn = CreateWindowEx(0, L"BUTTON", L"Color", WS_CHILD | WS_VISIBLE, 20, 130, 300, 30, hWnd, (HMENU)ID_COLOR, GetModuleHandle(NULL), NULL);
		
		hTraceBtn = CreateWindowEx(0, L"BUTTON", L"Trace", WS_CHILD | WS_VISIBLE,
                            20, 170, 300, 30, hWnd, (HMENU)ID_TRACE,
                            GetModuleHandle(NULL), NULL);

		// BS_AUTOCHECKBOX | BS_PUSHLIKE: a checkbox drawn/behaving like a button
		// that stays visually pressed while checked - gives "View Segments" a
		// native on/off toggle look instead of a plain momentary click.
		hViewSegBtn = CreateWindowEx(0, L"BUTTON", L"View Segments",
		                              WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
		                              20, 250, 300, 30, hWnd, (HMENU)ID_VIEW_SEGMENTS,
		                              GetModuleHandle(NULL), NULL);

		hUploadBtn = CreateWindowEx(0, L"BUTTON", L"Upload BMP...", WS_CHILD | WS_VISIBLE,
		                             20, 210, 300, 30, hWnd, (HMENU)ID_UPLOAD,
		                             GetModuleHandle(NULL), NULL);

        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));
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
        return 0;
    }
    case WM_SIZE:
        LayoutUI(hWnd);
        return 0;
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_CLEAR)
		{
		    ResetCanvas();
		    SendMessage(hViewSegBtn, BM_SETCHECK, BST_UNCHECKED, 0);   // NEW: keep toggle in sync
		    if (hWndGL) InvalidateRect(hWndGL, NULL, TRUE);
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
		else if (LOWORD(wParam) == ID_TRACE)
		{
		    RunTracePipeline();
		    if (hWndGL) InvalidateRect(hWndGL, NULL, FALSE);
		}
		else if (LOWORD(wParam) == ID_UPLOAD)
		{
		    RunUploadPipeline();
		    SendMessage(hViewSegBtn, BM_SETCHECK, BST_UNCHECKED, 0);   // NEW: new image needs its own trace
		}
		else if (LOWORD(wParam) == ID_VIEW_SEGMENTS)
		{
		    // BS_AUTOCHECKBOX already flipped its own check state before this
		    // notification fires, so read it back rather than tracking a
		    // separate bool - the button IS the toggle state.
		    BOOL nowChecked = (SendMessage(hViewSegBtn, BM_GETCHECK, 0, 0) == BST_CHECKED);

		    if (nowChecked)
		    {
		        RunPendingUploadTrace();   // trace/segment the uploaded BMP on demand,
		                                   // no-op if already traced or nothing was uploaded
		    }
		    canvas.showSegments = nowChecked;
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
