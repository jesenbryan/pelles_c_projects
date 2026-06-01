#define UNICODE
#define _UNICODE

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT 0

//#pragma warn(disable: 2018)
//#pragma warn(disable: 2154)
//#pragma warn(disable: 2215)

#include <windows.h>
#include <gl/GL.h>
#include <commctrl.h>
#include <math.h>
#include <stdio.h>
#include "stb_image_write.h"
#include "stb_image.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

#define MAX_POINTS 200000
#define MAX_STROKES 1000
#define ID_COLOR 2

unsigned char *bgImage = NULL;
int bgW = 0;
int bgH = 0;

float points[MAX_POINTS];
int pointCount = 0;

int strokeStarts[MAX_STROKES];
int strokeCount = 0;

BOOL drawing = FALSE;

HGLRC hRC;
HDC hDC;

HWND hWndGL;
HWND hWndUI;

HWND hClearBtn;
HWND hSlider;
HWND hColorBtn;
HWND hSaveBtn;
HWND hLoadBtn;
#define ID_CLEAR 1
#define ID_SAVE 3
#define ID_LOAD 4

int controlCount = 5; // slider + clear + color, save

float thickness = 2.0f;
float strokeThickness[MAX_STROKES];

COLORREF brushColor = RGB(0, 0, 0);
COLORREF strokeColor[MAX_STROKES];

LRESULT CALLBACK WndProcGL(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcUI(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void LayoutUI(HWND hWnd)
{
    RECT r;
    GetClientRect(hWnd, &r);

    int width = r.right;

    int btnW = 300;
    int btnH = 30;
    int spacing = 10;

    int totalH = (btnH * controlCount) + (spacing * (controlCount + 1));

    int startY = (r.bottom - totalH) / 2;

    if (startY < 10) startY = 10; 

    int centerX = (width - btnW) / 2;

    int y = startY;

	MoveWindow(hClearBtn, centerX, y, btnW, btnH, TRUE);
    y += btnH + spacing;

    MoveWindow(hSlider, centerX, y, btnW, btnH, TRUE);
    y += btnH + spacing;

    MoveWindow(hColorBtn, centerX, y, 75, btnH, TRUE);
	y += btnH + spacing;

	if (hSaveBtn)
	    MoveWindow(hSaveBtn, centerX, y, 75, btnH, TRUE);

	y += btnH + spacing;

	if (hLoadBtn)
	    MoveWindow(hLoadBtn, centerX, y, 75, btnH, TRUE);

}

int GetRequiredUIHeight()
{
    int btnH = 30;
    int spacing = 10;

    int controls = controlCount;

    int contentH = (btnH * controls) + (spacing * (controls + 1));

    int paddingBottom = 20; // 👈 IMPORTANT FIX

    return contentH + paddingBottom;
}

void AutoResizeUI(HWND hWnd)
{
    int newClientW = 360;
    int newClientH = GetRequiredUIHeight();

    RECT r = {0, 0, newClientW, newClientH};

    AdjustWindowRect(&r, GetWindowLong(hWnd, GWL_STYLE), FALSE);

    SetWindowPos(
        hWnd,
        NULL,
        0, 0,
        r.right - r.left,
        r.bottom - r.top,
        SWP_NOMOVE | SWP_NOZORDER
    );
}

void SavePNG(HWND hWndGL)
{
    RECT r;
    GetClientRect(hWndGL, &r);

    int width = r.right;
    int height = r.bottom;

    unsigned char *pixels = (unsigned char*)malloc(width * height * 3);

    glReadPixels(
        0, 0,
        width, height,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        pixels
    );

    // flip vertically (OpenGL is bottom-up)
    unsigned char *flipped = (unsigned char*)malloc(width * height * 3);

    for (int y = 0; y < height; y++)
    {
        memcpy(
            flipped + (y * width * 3),
            pixels + ((height - 1 - y) * width * 3),
            width * 3
        );
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    char filename[256];
    wsprintfA(filename, "drawing_%04d%02d%02d_%02d%02d%02d.png",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);

    stbi_write_png(filename, width, height, 3, flipped, width * 3);

    free(pixels);
    free(flipped);

    char msg[300];
    wsprintfA(msg, "Saved PNG: %s\n", filename);
    OutputDebugStringA(msg);
} 

void LoadPNG(HWND hWndGL)
{
    OPENFILENAME ofn;
    wchar_t fileName[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWndGL;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;

    ofn.lpstrFilter = L"PNG Files\0*.png\0All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileName(&ofn))
        return;

    int w, h, c;

    char pathA[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, fileName, -1, pathA, MAX_PATH, NULL, NULL);

    unsigned char *img = stbi_load(pathA, &w, &h, &c, 3);

    if (!img)
    {
        OutputDebugStringA("Load failed\n");
        return;
    }

    bgImage = stbi_load(pathA, &bgW, &bgH, NULL, 3);

    stbi_image_free(img);

    SwapBuffers(hDC);
}

//
// ---------------- OPENGL WINDOW ----------------
//
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

        int pf = ChoosePixelFormat(hDC, &pfd);
        SetPixelFormat(hDC, pf, &pfd);

        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_LINE_SMOOTH);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        return 0;
    }

	case WM_LBUTTONDOWN:
	{
	    drawing = TRUE;

	    strokeStarts[strokeCount] = pointCount;

	    strokeThickness[strokeCount] = thickness;
		strokeColor[strokeCount] = brushColor;

	    strokeCount++;

	    return 0;
	}

    case WM_LBUTTONUP:
        drawing = FALSE;
        return 0;

    case WM_MOUSEMOVE:
    {
        if (!drawing) return 0;

        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        RECT r;
        GetClientRect(hWnd, &r);

        float glx = (x / (float)r.right) * 2.0f - 1.0f;
        float gly = 1.0f - (y / (float)r.bottom) * 2.0f;

        points[pointCount++] = glx;
        points[pointCount++] = gly;

        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);

        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
		
		if (bgImage)
		{
		    glDrawPixels(bgW, bgH, GL_RGB, GL_UNSIGNED_BYTE, bgImage);
		}

        for (int s = 0; s < strokeCount; s++)
		{
		    int start = strokeStarts[s];
		    int end = (s == strokeCount - 1) ? pointCount : strokeStarts[s + 1];

		    COLORREF c = strokeColor[s];

		    glColor3f(
		        GetRValue(c) / 255.0f,
		        GetGValue(c) / 255.0f,
		        GetBValue(c) / 255.0f
		    );

		    glLineWidth(strokeThickness[s]);

		    glBegin(GL_LINE_STRIP);

		    for (int i = start; i < end; i += 2)
		        glVertex2f(points[i], points[i + 1]);

		    glEnd();
		}

        SwapBuffers(hDC);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

//
// ---------------- UI WINDOW ----------------
//
LRESULT CALLBACK WndProcUI(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_CREATE:
    {
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
        InitCommonControlsEx(&icc);

		hClearBtn = CreateWindowEx(
		    0,
		    L"BUTTON",
		    L"Clear",
		    WS_CHILD | WS_VISIBLE,
		    20, 20, 300, 30,
		    hWnd,
		    (HMENU)ID_CLEAR,
		    GetModuleHandle(NULL),
		    NULL
		);

		hSlider = CreateWindowEx(
		    0,
		    TRACKBAR_CLASS,
		    L"",
		    WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
		    20, 70, 300, 40,
		    hWnd,
		    NULL,
		    GetModuleHandle(NULL),
		    NULL
		);

		hColorBtn = CreateWindowEx(
		    0,
		    L"BUTTON",
		    L"Color",
		    WS_CHILD | WS_VISIBLE,
		    20, 120, 300, 30,
		    hWnd,
		    (HMENU)ID_COLOR,
		    GetModuleHandle(NULL),
		    NULL
		);

		hSaveBtn = CreateWindowEx(
		    0,
		    L"BUTTON",
		    L"Save",
		    WS_CHILD | WS_VISIBLE,
		    20, 160, 300, 30,
		    hWnd,
		    (HMENU)ID_SAVE,
		    GetModuleHandle(NULL),
		    NULL
		);

		hLoadBtn = CreateWindowEx(
		    0,
		    L"BUTTON",
		    L"Load",
		    WS_CHILD | WS_VISIBLE,
		    20, 200, 300, 30,
		    hWnd,
		    (HMENU)ID_LOAD,
		    GetModuleHandle(NULL),
		    NULL
		);

        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));
        SendMessage(hSlider, TBM_SETPOS, TRUE, (LPARAM)thickness);
		thickness = 2.0f;

		AutoResizeUI(hWnd);
		LayoutUI(hWnd);
		
        return 0;
    }

    case WM_HSCROLL:
	{
		char buf[64];
		wsprintfA(buf, "Thickness: %d\n", (int)thickness);
		OutputDebugStringA(buf);

	    if ((HWND)lParam == hSlider || hSlider != NULL)
	    {
	        thickness = (float)SendMessage(hSlider, TBM_GETPOS, 0, 0);

	        if (hWndGL)
	            InvalidateRect(hWndGL, NULL, FALSE);
	    }
	    return 0;
	}

	case WM_SIZE:
	{
	    LayoutUI(hWnd);
	    return 0;
	}

	case WM_COMMAND:
	{
	    if (LOWORD(wParam) == ID_CLEAR)
	    {
	        pointCount = 0;
	        strokeCount = 0;
	        InvalidateRect(hWndGL, NULL, TRUE);
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

	        if (ChooseColor(&cc))
	        {
	            brushColor = cc.rgbResult;
	        }
	    }
	    else if (LOWORD(wParam) == ID_SAVE)
	    {
	        SavePNG(hWndGL);
	    }
		else if (LOWORD(wParam) == ID_LOAD)
		{
		    LoadPNG(hWndGL);
		}
	    return 0;   
	}

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

//
// ---------------- MAIN ----------------
//
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProcGL;
    wc.hInstance = hInst;
    wc.lpszClassName = L"GLWindow";
    RegisterClass(&wc);

    WNDCLASS ui = {0};
    ui.lpfnWndProc = WndProcUI;
    ui.hInstance = hInst;
    ui.lpszClassName = L"UIWindow";
    RegisterClass(&ui);

    hWndGL = CreateWindow(
        L"GLWindow",
        L"Canvas",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 800, 600,
        NULL, NULL, hInst, NULL
    );

    hWndUI = CreateWindow(
    L"UIWindow",
    L"Control",
    WS_OVERLAPPED | WS_VISIBLE,
    920, 100, 360, 180,
    NULL, NULL, hInst, NULL
	);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
