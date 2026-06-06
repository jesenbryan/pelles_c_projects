#define UNICODE
#define _UNICODE

#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <commctrl.h>
#include <stdio.h>

#include "geometry.h"
#include "app.h"
#include "mode.h"

#include "environment.h"
#include "paint.h"

#include "robot.h"
#include "renderer.h"
#include "config.h"
#include "input.h"
#include "graphics.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")

// ---------------- DATA ----------------

AppState app;

int paintWasInside = 0;

HWND hwndSlider;

HWND labelLeft;
HWND sliderLeft;

HWND labelRight;
HWND sliderRight;

float MIN_R = 0.05f;
float MAX_R = 0.35f;

#define ID_LEFT  101
#define ID_RIGHT 102


// ---------------- SLIDERS ----------------

void createSliders(HINSTANCE hInst)
{
    hwndSlider = CreateWindow(
        L"SliderWin",
        L"Controls",
        WS_OVERLAPPEDWINDOW,
        850, 100,
        400, 350,
        NULL, NULL,
        hInst, NULL
    );

	labelLeft = CreateWindow(
	    L"STATIC",
	    L"Left Circle Radius",
	    WS_CHILD | WS_VISIBLE,
	    20, 0, 200, 20,
	    hwndSlider,
	    NULL,
	    hInst,
	    NULL
	);

    sliderLeft = CreateWindowEx(
        0, TRACKBAR_CLASS, L"Left Radius",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        20, 20, 240, 40,
        hwndSlider,
        (HMENU)ID_LEFT,
        hInst,
        NULL
    );

	app.ui.sliderLeft = sliderLeft;

	SendMessage(sliderLeft, TBM_SETRANGE, TRUE, MAKELPARAM(100, 300));
	SendMessage(sliderLeft, TBM_SETPOS, TRUE, 200);

	labelRight = CreateWindow(
	    L"STATIC",
	    L"Right Circle Radius",
	    WS_CHILD | WS_VISIBLE,
	    20, 60, 200, 20,
	    hwndSlider,
	    NULL,
	    hInst,
	    NULL
	);

    sliderRight = CreateWindowEx(
        0, TRACKBAR_CLASS, L"Right Radius",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        20, 80, 240, 40,
        hwndSlider,
        (HMENU)ID_RIGHT,
        hInst,
        NULL
    );

	app.ui.sliderRight = sliderRight;

	SendMessage(sliderRight, TBM_SETRANGE, TRUE, MAKELPARAM(100, 300));
    SendMessage(sliderRight, TBM_SETPOS, TRUE, 200);

    ShowWindow(hwndSlider, SW_SHOW);
}

// ---------------- WINDOW PROC ----------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
		case WM_SIZE:
		{
		    int w = LOWORD(lParam);
		    int h = HIWORD(lParam);

		    if (h == 0) h = 1;

		    glViewport(0, 0, w, h);

		    glMatrixMode(GL_PROJECTION);
		    glLoadIdentity();

		    float aspect = (float)w / (float)h;

		    if (aspect >= 1.0f)
		        glOrtho(-1.5f * aspect, 1.5f * aspect, -1.5f, 1.5f, -1, 1);
		    else
		        glOrtho(-1.5f, 1.5f, -1.5f / aspect, 1.5f / aspect, -1, 1);

		    glMatrixMode(GL_MODELVIEW);
		}
		break;

        case WM_HSCROLL:
		{
		    HWND src = (HWND)lParam;
		    int code = LOWORD(wParam);

		    if (src == sliderLeft)
		    {
		        if (code == TB_THUMBTRACK)
		            app.sliderDraggingLeft = 1;
		        else if (code == TB_ENDTRACK)
		            app.sliderDraggingLeft = 0;
	
				LRESULT pos = SendMessage(sliderLeft, TBM_GETPOS, 0, 0);
		        int value = (int)pos;

		        app.robotScene.robot.leftRadius = value / 1000.0f;
		    }
		    else if (src == sliderRight)
		    {
		        if (code == TB_THUMBTRACK)
		            app.sliderDraggingRight = 1;
		        else if (code == TB_ENDTRACK)
		            app.sliderDraggingRight = 0;

		        LRESULT pos = SendMessage(sliderRight, TBM_GETPOS, 0, 0);
		        int value = (int)pos;

		        app.robotScene.robot.rightRadius = value / 1000.0f;
		    }
		}
		break;

		case WM_LBUTTONDOWN:
		{
		    int mx = LOWORD(lParam);
		    int my = HIWORD(lParam);

		    //float x, y;
		    screenToGL(hwnd, mx, my, &app.mouseGL.x, &app.mouseGL.y);

		    Point mouse = app.mouseGL;

		    if (app.mode == MODE_ROBOT)
		    {
		        app.draggingTop = 0;
		        app.draggingBottom = 0;
		        app.draggingInner = 0;

		        Point topHandle = rotatePoint(app.robotScene.robot.topCtrl, getCenter(app.robotScene.robot), app.robotScene.robot.angle);
		        Point bottomHandle = rotatePoint(app.robotScene.robot.bottomCtrl, getCenter(app.robotScene.robot), app.robotScene.robot.angle);

		        Point center = getCenter(app.robotScene.robot);
		        Point innerWorld = rotatePoint(app.robotScene.robot.innerCircle, center, app.robotScene.robot.angle);

		        app.activeHandle = 0;

		        if (isNear(mouse, topHandle, HANDLE_RADIUS))
		        {
		            app.draggingTop = 1;
		            app.activeHandle = 1;
		        }
		        else if (isNear(mouse, bottomHandle, HANDLE_RADIUS))
		        {
		            app.draggingBottom = 1;
		            app.activeHandle = 2;
		        }
		        else if (isNear(mouse, innerWorld, INNER_HANDLE_RADIUS))
		        {
		            app.draggingInner = 1;
		            app.activeHandle = 3;
		        }
		    }
		    else if (app.mode == MODE_ENVIRONMENT)
		    {
		        //app.wallStart = mouse;   // IMPORTANT: use local mouse
		        app.isDrawingWall = 1;
				printf("CLICK wallStart: %f %f\n", app.wallStart.x, app.wallStart.y);
		    }
			else if (app.mode == MODE_PAINT)
			{
			    app.painting = 1;
			    app.paintCount = 0;
			    SetCapture(hwnd);

				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(tme);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;
				TrackMouseEvent(&tme);
			}
		}
		break;

		case WM_MOUSEMOVE:
		{
		    int mx = LOWORD(lParam);
		    int my = HIWORD(lParam);

		    screenToGL(hwnd, mx, my, &app.mouseGL.x, &app.mouseGL.y);

		    // robot only logic
		    if (app.mode == MODE_ROBOT)
		    {
		        if (!app.draggingTop && !app.draggingBottom && !app.draggingInner)
		            break;

		        Point mouse = app.mouseGL;

		        Point center = getCenter(app.robotScene.robot);
		        float angle = app.robotScene.robot.angle;

		        Point localMouse = inverseRotate(mouse, center, angle);

		        if (app.draggingTop)
		            app.robotScene.robot.topCtrl = localMouse;

		        if (app.draggingBottom)
		            app.robotScene.robot.bottomCtrl = localMouse;

		        if (app.draggingInner)
		            app.robotScene.robot.innerCircle = localMouse;
		    }
			if (app.mode == MODE_ENVIRONMENT && app.isDrawingWall)
			{
			    screenToGL(hwnd, mx, my, &app.mouseGL.x, &app.mouseGL.y);
			}

			if (app.mode == MODE_PAINT && app.painting)
			{
			    RECT r;
			    GetClientRect(hwnd, &r);

			    POINT pt = { mx, my };

			    int inside =
			        (pt.x >= 0 && pt.y >= 0 &&
			         pt.x < r.right && pt.y < r.bottom);

			    if (!inside)
			    {
			        addBreak();   // IMPORTANT
			        break;
			    }

			    float nx, ny;
			    screenToGL(hwnd, mx, my, &nx, &ny);

			    if (app.paintCount + 2 < MAX_POINTS)
			    {
			        app.paintPoints[app.paintCount++] = nx;
			        app.paintPoints[app.paintCount++] = ny;
			    }
			}
		}
		break;

		case WM_LBUTTONUP:
		{
			int mx = LOWORD(lParam);
		    int my = HIWORD(lParam);

			if (app.mode == MODE_ENVIRONMENT && app.isDrawingWall)
			{
			    Point end;
			    screenToGL(hwnd, mx, my, &end.x, &end.y);

			    addWall(app.wallStart, end);

			    app.isDrawingWall = 0;
			}

			if (app.mode == MODE_PAINT)
			{
			    app.painting = 0;
			    ReleaseCapture();
			}

		    app.draggingTop = 0;
		    app.draggingBottom = 0;
		    app.draggingInner = 0;
    		app.activeHandle = 0;
		}
		break;

		case WM_MOUSELEAVE:
		{
		    if (app.mode == MODE_PAINT && app.painting)
		    {
		        addBreak();   // prevents line continuation
		        app.painting = 0;
		        ReleaseCapture();
		    }
		    return 0;
		}

		case WM_KEYDOWN:
		{
		    if (wParam == VK_LEFT)
		        app.robotScene.robot.angle += 2.0f;

		    if (wParam == VK_RIGHT)
		        app.robotScene.robot.angle -= 2.0f;

			if (wParam == 'E')
			    app.mode = MODE_ENVIRONMENT;
		        

			if (wParam == 'R')
			    app.mode = MODE_ROBOT;

			if (wParam == 'P')
			    app.mode = MODE_PAINT;
		        
		}
		break;

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ---------------- MAIN ----------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd)
{
	AllocConsole();
	if (!freopen("CONOUT$", "w", stdout))
	{
	    MessageBox(NULL, L"stdout redirect failed", L"Error", MB_OK);
	}

	if (!freopen("CONOUT$", "w", stderr))
	{
	    MessageBox(NULL, L"stderr redirect failed", L"Error", MB_OK);
	}

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    app.robotScene.robot.leftX = -0.6f;
    app.robotScene.robot.rightX = 0.6f;
    app.robotScene.robot.y = 0.0f;
    app.robotScene.robot.leftRadius = 0.2f;
    app.robotScene.robot.rightRadius = 0.2f;

	app.robotScene.robot.topCtrl.x = 0.0f;
	app.robotScene.robot.topCtrl.y = 0.4f;

	app.robotScene.robot.bottomCtrl.x = 0.0f;
	app.robotScene.robot.bottomCtrl.y = -0.4f;

	app.robotScene.robot.innerCircle.x = 0.0f;
	app.robotScene.robot.innerCircle.y = 0.0f;
	app.robotScene.robot.innerRadius = 0.12f;

	app.paintCount = 0;
	app.painting = 0;

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CapsulerobotScene.robot";

    RegisterClass(&wc);

    WNDCLASS wcSlider = {0};
    wcSlider.lpfnWndProc = WndProc;
    wcSlider.hInstance = hInst;
    wcSlider.lpszClassName = L"SliderWin";

    RegisterClass(&wcSlider);

    HWND hwnd = CreateWindow(
        L"CapsulerobotScene.robot",
        L"Capsule robotScene.robot Body",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL,
        hInst, NULL
    );

    ShowWindow(hwnd, nShowCmd);

    setupOpenGL(hwnd);
    createSliders(hInst);

    MSG msg;

    while (1)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return 0;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        renderApp(&app, graphicsGetHDC());
    }
}
