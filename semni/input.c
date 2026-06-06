#include <stdio.h>
#include <windows.h>
#include <commctrl.h>

#include "app.h"
#include "paint.h"
#include "environment.h"
#include "input.h"
#include "geometry.h"
#include "config.h"



#include "robot.h"
#include "graphics.h"



LRESULT handleInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, AppState* app)
{
    switch (msg)
    {
        case WM_LBUTTONDOWN:
        {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);

            screenToGL(hwnd, mx, my, &app->mouseGL.x, &app->mouseGL.y);

            if (app->mode == MODE_ROBOT)
            {
                app->draggingTop = 0;
                app->draggingBottom = 0;
                app->draggingInner = 0;

                Point center = getCenter(app->robotScene.robot);

                Point mouse = app->mouseGL;

                Point topHandle = rotatePoint(app->robotScene.robot.topCtrl, center, app->robotScene.robot.angle);

                if (isNear(mouse, topHandle, HANDLE_RADIUS))
                {
                    app->draggingTop = 1;
                    app->activeHandle = 1;
                }
            }
        }
		break;

		case WM_LBUTTONUP:
		{
			int mx = LOWORD(lParam);
		    int my = HIWORD(lParam);

			if (app->mode == MODE_ENVIRONMENT && app->isDrawingWall)
			{
			    Point end;
			    screenToGL(hwnd, mx, my, &end.x, &end.y);

			    addWall(app->wallStart, end);

			    app->isDrawingWall = 0;
			}

			if (app->mode == MODE_PAINT)
			{
			    app->painting = 0;
			    ReleaseCapture();
			}

		    app->draggingTop = 0;
		    app->draggingBottom = 0;
		    app->draggingInner = 0;
    		app->activeHandle = 0;
		}
		break;

		case WM_MOUSEMOVE:
		{
		    int mx = LOWORD(lParam);
		    int my = HIWORD(lParam);

		    screenToGL(hwnd, mx, my, &app->mouseGL.x, &app->mouseGL.y);

		    // robot only logic
		    if (app->mode == MODE_ROBOT)
		    {
		        if (!app->draggingTop && !app->draggingBottom && !app->draggingInner)
		            break;

		        Point mouse = app->mouseGL;

		        Point center = getCenter(app->robotScene.robot);
		        float angle = app->robotScene.robot.angle;

		        Point localMouse = inverseRotate(mouse, center, angle);

		        if (app->draggingTop)
		            app->robotScene.robot.topCtrl = localMouse;

		        if (app->draggingBottom)
		            app->robotScene.robot.bottomCtrl = localMouse;

		        if (app->draggingInner)
		            app->robotScene.robot.innerCircle = localMouse;
		    }
			if (app->mode == MODE_ENVIRONMENT && app->isDrawingWall)
			{
			    screenToGL(hwnd, mx, my, &app->mouseGL.x, &app->mouseGL.y);
			}

			if (app->mode == MODE_PAINT && app->painting)
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

			    if (app->paintCount + 2 < MAX_POINTS)
			    {
			        app->paintPoints[app->paintCount++] = nx;
			        app->paintPoints[app->paintCount++] = ny;
			    }
			}
		}
		break;

		case WM_KEYDOWN:
		{
		    if (wParam == VK_LEFT)
		        app->robotScene.robot.angle += 2.0f;

		    if (wParam == VK_RIGHT)
		        app->robotScene.robot.angle -= 2.0f;

			if (wParam == 'E')
			    app->mode = MODE_ENVIRONMENT;
		        

			if (wParam == 'R')
			    app->mode = MODE_ROBOT;

			if (wParam == 'P')
			    app->mode = MODE_PAINT;
		        
		}
		break;

		case WM_MOUSELEAVE:
		{
		    if (app->mode == MODE_PAINT && app->painting)
		    {
		        addBreak();   // prevents line continuation
		        app->painting = 0;
		        ReleaseCapture();
		    }
		    return 0;
		}

        case WM_HSCROLL:
		{
		    HWND src = (HWND)lParam;
		    int code = LOWORD(wParam);

		    if (src == app->ui.sliderLeft)
		    {
		        if (code == TB_THUMBTRACK)
		            app->sliderDraggingLeft = 1;
		        else if (code == TB_ENDTRACK)
		            app->sliderDraggingLeft = 0;
	
				LRESULT pos = SendMessage(app->ui.sliderLeft, TBM_GETPOS, 0, 0);
		        int value = (int)pos;

		        app->robotScene.robot.leftRadius = value / 1000.0f;
		    }
		    else if (src == app->ui.sliderRight)
		    {
		        if (code == TB_THUMBTRACK)
		            app->sliderDraggingRight = 1;
		        else if (code == TB_ENDTRACK)
		            app->sliderDraggingRight = 0;

		        LRESULT pos = SendMessage(app->ui.sliderRight, TBM_GETPOS, 0, 0);
		        int value = (int)pos;

		        app->robotScene.robot.rightRadius = value / 1000.0f;
		    }
		}
		break;

		case WM_SIZE:
		{
		    int w = LOWORD(lParam);
		    int h = HIWORD(lParam);

		    graphicsOnResize(w, h);
		}
		break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
