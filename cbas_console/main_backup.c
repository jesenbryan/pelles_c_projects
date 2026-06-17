#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#include "bmp.h"
#include "extract.h"
#include "utils.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	freopen("CONIN$", "r", stdin);

	setvbuf(stdout, NULL, _IONBF, 0);

	printf("Console initialized!\n");
	// ---- YOUR ROBOTICS CODE HERE ----
    //Image* img = loadBMP("Untitled.bmp");

	//if (!img)
	//{
	    //printf("Failed to load BMP\n");
	    //return 0;
	//}

    //Path path = extractPath(img);

	printf("A\n");

	Image* img = loadBMP("Untitled.bmp");

	printf("B\n");

	Path path = extractPath(img);

	printf("C\n");

	printf("Points: %d\n", path.count);

	//if (path.count == 0)
	//{
	    //printf("No path extracted\n");
	//}

    //printf("Points: %d\n", path.count);
	//savePathCSV("path.csv", path);

	
    // ---------------------------------    
	
	
	const wchar_t CLASS_NAME[] = L"SimpleWindowClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc))
        return 0;

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Blank Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
