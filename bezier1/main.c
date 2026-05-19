/****************************************************************************
 *                                                                          *
 * File    : main.c                                                         *
 *                                                                          *
 * Purpose : Generic Win32 application.                                     *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

#define WIN32_LEAN_AND_MEAN  /* speed up compilations */
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <wchar.h>
#include "main.h"

#define NELEMS(a)  (sizeof(a) / sizeof((a)[0]))

/** Prototypes **************************************************************/

static LRESULT WINAPI MainWndProc(HWND, UINT, WPARAM, LPARAM);
static void Main_OnPaint(HWND);
static void Main_OnCommand(HWND, int, HWND, UINT);
static void Main_OnDestroy(HWND);
static LRESULT WINAPI AboutDlgProc(HWND, UINT, WPARAM, LPARAM);

/** Global variables ********************************************************/

static HANDLE ghInstance;

/****************************************************************************
 *                                                                          *
 * Function: wWinMain                                                       *
 *                                                                          *
 * Purpose : Initialize the application.  Register a window class,          *
 *           create and display the main window and enter the               *
 *           message loop.                                                  *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, WCHAR *pszCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc;
    WNDCLASS wc;
    HWND hwnd;
    MSG msg;

    ghInstance = hInstance;

    /* Initialize common controls. Also needed for MANIFEST's */
    /*
     * TODO: set the ICC_???_CLASSES that you need.
     */
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES /*|ICC_COOL_CLASSES|ICC_DATE_CLASSES|ICC_PAGESCROLLER_CLASS|ICC_USEREX_CLASSES|... */;
    InitCommonControlsEx(&icc);

    /* Load Rich Edit control support */
    /*
     * TODO: uncomment one of the lines below, if you are using a Rich Edit control.
     */
    // LoadLibrary(L"riched32.dll");  /* Rich Edit v1.0 */
    // LoadLibrary(L"riched20.dll");  /* Rich Edit v2.0, v3.0 */
    // LoadLibrary(L"msftedit.dll");  /* Rich Edit v4.1 */

    /*
     * TODO: uncomment line below, if you are using the Network Address control (Windows Vista+).
     */
    // InitNetworkAddressControl();

    /* Register the main window class */
    wc.lpszClassName = L"bezier1Class";
    wc.lpfnWndProc = MainWndProc;
    wc.style = CS_OWNDC|CS_VREDRAW|CS_HREDRAW;
    wc.hInstance = ghInstance;
    wc.hIcon = LoadIcon(ghInstance, MAKEINTRESOURCE(IDR_ICO_MAIN));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MNU_MAIN);
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    if (!RegisterClass(&wc))
        return 1;

    /* Create the main window */
    hwnd = CreateWindow(L"bezier1Class",
        L"bezier1 Program",
        WS_OVERLAPPEDWINDOW|WS_HSCROLL|WS_VSCROLL,
        0,
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        ghInstance,
        NULL
    );
    if (!hwnd) return 1;

    /* Show and paint the main window */
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* Pump messages until we are done */
#if 0
    /* "Politically correct" code -- SEE MICROSOFT DOCUMENTATION */
    for (;;)
    {
        BOOL fRet = GetMessage(&msg, NULL, 0, 0);
        if (fRet == -1)  /* Error */
        {
            /* TODO: handle the error from GetMessage() */
            __debugbreak();
            return -1;
        }
        else if (fRet == 0)  /* WM_QUIT */
        {
            break;
        }
        else  /* Not error or WM_QUIT */
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
#else
    /* "Commonly seen" code */
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif

    return msg.wParam;
}

/****************************************************************************
 *                                                                          *
 * Function: MainWndProc                                                    *
 *                                                                          *
 * Purpose : Process application messages.                                  *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        HANDLE_MSG(hwnd, WM_PAINT, Main_OnPaint);
        HANDLE_MSG(hwnd, WM_COMMAND, Main_OnCommand);
        HANDLE_MSG(hwnd, WM_DESTROY, Main_OnDestroy);
        /* TODO: enter more messages here */
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

/****************************************************************************
 *                                                                          *
 * Function: Main_OnPaint                                                   *
 *                                                                          *
 * Purpose : Process a WM_PAINT message.                                    *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

static void Main_OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    RECT rc;

    BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &rc);
    /* TODO: Enter any drawing code here */
    EndPaint(hwnd, &ps);
}

/****************************************************************************
 *                                                                          *
 * Function: Main_OnCommand                                                 *
 *                                                                          *
 * Purpose : Process a WM_COMMAND message.                                  *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

static void Main_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
        case IDM_ABOUT:
            DialogBox(ghInstance, MAKEINTRESOURCE(DLG_ABOUT), hwnd, (DLGPROC)AboutDlgProc);

        /* TODO: Enter more commands here */
    }
}

/****************************************************************************
 *                                                                          *
 * Function: Main_OnDestroy                                                 *
 *                                                                          *
 * Purpose : Process a WM_DESTROY message.                                  *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

static void Main_OnDestroy(HWND hwnd)
{
    PostQuitMessage(0);
}

/****************************************************************************
 *                                                                          *
 * Function: AboutDlgProc                                                   *
 *                                                                          *
 * Purpose : Process messages for the About dialog.  The dialog is          *
             shown when the user selects "About" in the "Help" menu.        *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

static LRESULT CALLBACK AboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
            /*
             * Nothing special to initialize.
             */
            return TRUE;

        case WM_COMMAND:
            switch (wParam)
            {
                case IDOK:
                case IDCANCEL:
                    /*
                     * OK or Cancel was clicked, close the dialog.
                     */
                    EndDialog(hDlg, TRUE);
                    return TRUE;
            }
            break;
    }

    return FALSE;
}
