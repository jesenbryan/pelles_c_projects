#include "help_dialog.h"
#include <stddef.h>

// ---- content ----
//
// One row per shortcut, grouped into sections (sectionHeader != NULL starts
// a new section right before that row, with its own header + separator
// line). Kept as a single flat table instead of two separate arrays so
// adding/reordering shortcuts later is a one-line change, not a layout
// change.
typedef struct {
    const wchar_t* sectionHeader; // NULL unless this row starts a new section
    const wchar_t* key;
    const wchar_t* desc;
} HelpRow;

static const HelpRow kHelpRows[] = {
    { L"General",                        L"Scroll Wheel",        L"Zoom in / out" },
    { NULL,                              L"Middle-click + Drag", L"Pan the view" },
    { NULL,                              L"Ctrl + Numpad 0",     L"Reset zoom and panning" },

    { L"Drawing (Design > Environment)", L"Left-click + Drag",   L"Draw a freehand stroke" },
    { NULL,                              L"Hold Shift",          L"Constrain the stroke to a straight line" },

    { L"Robot Editing (Design > Robot)", L"Left-click + Drag",   L"Move, resize, or reshape a handle" },
    { NULL,                              L"Scroll Wheel (handle)", L"Resize the hovered joint's circle" },
    { NULL,                              L"Shift + Scroll (handle)", L"Rotate the hovered joint" },
    { NULL,                              L"Left / Right Arrow",  L"Rotate the whole robot" },
    { NULL,                              L"Up / Down Arrow",     L"Move the whole robot (hold Ctrl for finer steps)" },
    { NULL,                              L"Esc",                 L"Return to Environment view" },

    { L"Simulation (Work in Progress)",  L"Left-click + Drag",   L"Move the robot" },
    { NULL,                              L"Shift + Scroll",      L"Rotate the hovered joint" },
    { NULL,                              L"Left / Right Arrow",  L"Rotate the whole robot" },
    { NULL,                              L"G",                   L"Nudge the robot down" },
    { NULL,                              L"Shift + G",           L"Toggle auto gravity" },
    { NULL,                              L"Shift + W",           L"Toggle Walk" },
};
#define HELP_ROW_COUNT (sizeof(kHelpRows) / sizeof(kHelpRows[0]))

#define ID_HELP_DLG_OK 5001

// ---- layout constants ----
#define HD_MARGIN        20
#define HD_HEADER_H      22
#define HD_SEP_H         2
#define HD_SEP_GAP       6
#define HD_ROW_H         22
#define HD_SECTION_GAP   16
#define HD_KEY_COL_W     190
#define HD_DESC_COL_W    380
#define HD_BUTTON_W      90
#define HD_BUTTON_H      26

static const wchar_t* kHelpWndClass = L"RobotControlsHelpDialog";
static BOOL g_helpClassRegistered = FALSE;
static HFONT g_helpHeaderFont = NULL;
static HFONT g_helpKeyFont = NULL;
static HFONT g_helpDescFont = NULL;
static BOOL g_helpDialogOpen = FALSE; // guards the local message loop below

static void ensureHelpFonts(void)
{
    if (g_helpHeaderFont) return;

    g_helpHeaderFont = CreateFont(
        -16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // 600 == FW_SEMIBOLD -- used as a plain number rather than the named
    // constant since not every compiler's headers declare it (Pelles C's
    // own wingdi.h has been missing MS-specific symbols before, see
    // save.c's _wtof/wcstod history), while FW_NORMAL/FW_BOLD above are
    // universally available.
    g_helpKeyFont = CreateFont(
        -14, 0, 0, 0, 600, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_helpDescFont = CreateFont(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

// Plain white "sheet" look (not the gray COL_BTNFACE panels the rest of
// the app's own control panel uses) -- reads closer to a real Windows
// settings/help page, and keeps every child STATIC control transparent
// against it via WM_CTLCOLORSTATIC below instead of showing a mismatched
// gray box behind each line of text.
static HBRUSH g_helpWhiteBrush = NULL;

static LRESULT CALLBACK helpDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        ensureHelpFonts();
        if (!g_helpWhiteBrush)
            g_helpWhiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

        int y = HD_MARGIN;
        int keyX = HD_MARGIN;
        int descX = HD_MARGIN + HD_KEY_COL_W;

        for (size_t i = 0; i < HELP_ROW_COUNT; i++)
        {
            const HelpRow* row = &kHelpRows[i];

            if (row->sectionHeader)
            {
                if (i != 0)
                    y += HD_SECTION_GAP;

                HWND hHeader = CreateWindow(L"STATIC", row->sectionHeader,
                    WS_VISIBLE | WS_CHILD | SS_LEFT,
                    HD_MARGIN, y, HD_KEY_COL_W + HD_DESC_COL_W, HD_HEADER_H,
                    hwnd, NULL, NULL, NULL);
                SendMessage(hHeader, WM_SETFONT, (WPARAM)g_helpHeaderFont, TRUE);
                y += HD_HEADER_H + HD_SEP_GAP;

                // Thin horizontal rule under the header -- SS_ETCHEDHORZ
                // is a 1px system-drawn line, no bitmap/owner-draw needed.
                CreateWindow(L"STATIC", L"",
                    WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
                    HD_MARGIN, y, HD_KEY_COL_W + HD_DESC_COL_W, HD_SEP_H,
                    hwnd, NULL, NULL, NULL);
                y += HD_SEP_H + HD_SEP_GAP;
            }

            HWND hKey = CreateWindow(L"STATIC", row->key,
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                keyX, y, HD_KEY_COL_W, HD_ROW_H,
                hwnd, NULL, NULL, NULL);
            SendMessage(hKey, WM_SETFONT, (WPARAM)g_helpKeyFont, TRUE);

            HWND hDesc = CreateWindow(L"STATIC", row->desc,
                WS_VISIBLE | WS_CHILD | SS_LEFT,
                descX, y, HD_DESC_COL_W, HD_ROW_H,
                hwnd, NULL, NULL, NULL);
            SendMessage(hDesc, WM_SETFONT, (WPARAM)g_helpDescFont, TRUE);

            y += HD_ROW_H;
        }

        y += HD_SECTION_GAP;

        RECT client;
        GetClientRect(hwnd, &client);
        HWND hOk = CreateWindow(L"BUTTON", L"OK",
            WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
            client.right - HD_MARGIN - HD_BUTTON_W, y, HD_BUTTON_W, HD_BUTTON_H,
            hwnd, (HMENU)ID_HELP_DLG_OK, NULL, NULL);
        SendMessage(hOk, WM_SETFONT, (WPARAM)g_helpDescFont, TRUE);
        SetFocus(hOk);

        return 0;
    }

    case WM_CTLCOLORSTATIC:
        // Transparent text over the window's own white background,
        // instead of each STATIC painting its own (mismatched gray)
        // default background square.
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (LRESULT)g_helpWhiteBrush;

    case WM_COMMAND:
        // IDCANCEL (2): IsDialogMessage's own modal loop (see
        // showControlsHelpDialog below) synthesizes a WM_COMMAND(IDCANCEL)
        // straight from a VK_ESCAPE keydown -- it never reaches this
        // window as a raw WM_KEYDOWN at all, so handling Esc there
        // wouldn't actually work; this is the real place to catch it.
        if (LOWORD(wParam) == ID_HELP_DLG_OK || LOWORD(wParam) == IDCANCEL)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_helpDialogOpen = FALSE;
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void ensureHelpClassRegistered(void)
{
    if (g_helpClassRegistered) return;

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = helpDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = kHelpWndClass;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    g_helpClassRegistered = TRUE;
}

void showControlsHelpDialog(HWND owner)
{
    if (g_helpDialogOpen) return; // already open -- don't stack a second one

    ensureHelpClassRegistered();
    ensureHelpFonts();

    // Total content height, same accumulation the WM_CREATE layout above
    // does, so the window is sized to fit exactly (no leftover blank
    // space, no clipping) without hardcoding a number that'd silently go
    // stale if a row is ever added/removed from kHelpRows.
    int contentH = HD_MARGIN;
    for (size_t i = 0; i < HELP_ROW_COUNT; i++)
    {
        if (kHelpRows[i].sectionHeader)
        {
            if (i != 0) contentH += HD_SECTION_GAP;
            contentH += HD_HEADER_H + HD_SEP_GAP + HD_SEP_H + HD_SEP_GAP;
        }
        contentH += HD_ROW_H;
    }
    contentH += HD_SECTION_GAP + HD_BUTTON_H + HD_MARGIN;

    int clientW = HD_MARGIN * 2 + HD_KEY_COL_W + HD_DESC_COL_W;

    RECT wr = { 0, 0, clientW, contentH };
    AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int winW = wr.right - wr.left;
    int winH = wr.bottom - wr.top;

    // Centered over "owner" (falls back to the screen's own center if
    // owner has no valid rect for some reason).
    int x = 100, y = 100;
    RECT ownerRect;
    if (owner && GetWindowRect(owner, &ownerRect))
    {
        x = ownerRect.left + ((ownerRect.right - ownerRect.left) - winW) / 2;
        y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - winH) / 2;
    }

    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        kHelpWndClass, L"Controls",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, winW, winH,
        owner, NULL, GetModuleHandle(NULL), NULL);

    if (!hDlg) return;

    g_helpDialogOpen = TRUE;

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    // Hand-rolled modal loop -- this app has no DialogBox-based modal
    // anywhere else (every other window, including the Robot editor's own
    // control panel, is a plain CreateWindow-built child of the shared
    // main window), so there's no existing modal machinery to hook into.
    // IsDialogMessage gives Tab/Enter/Esc the usual dialog-navigation
    // behavior even though hDlg is a plain WS_POPUP, not a real dialog
    // resource.
    MSG msg;
    while (g_helpDialogOpen && GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (owner)
    {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
        SetFocus(owner);
    }
}
