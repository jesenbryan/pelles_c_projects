#include "renderer.h"
#include <GL/gl.h>
#include <math.h>
#include "geometry.h"
#include "robot.h"
#include "environment.h"
#include "config.h"

void drawCircle(Point c, float r)
{
    glBegin(GL_LINE_LOOP);
    for (float t = 0; t < 6.28318f; t += 0.05f)
    {
        glVertex2f(
            c.x + cosf(t) * r,
            c.y + sinf(t) * r
        );
    }
    glEnd();
}

void drawBezier(Point p0, Point p1, Point p2)
{
    glBegin(GL_LINE_STRIP);

    for (float t = 0; t <= 1.0f; t += 0.02f)
    {
        float u = 1 - t;

        float x =
            u*u*p0.x +
            2*u*t*p1.x +
            t*t*p2.x;

        float y =
            u*u*p0.y +
            2*u*t*p1.y +
            t*t*p2.y;

        glVertex2f(x, y);
    }

    glEnd();
}

void drawHandle(Point p, int selected, float radius)
{
    const int segments = 64;

    if (selected)
        glColor3f(1.0f, 0.85f, 0.35f);
    else
        glColor3f(1.0f, 0.0f, 0.0f);

    glBegin(GL_TRIANGLE_FAN);

    glVertex2f(p.x, p.y);

    for (int i = 0; i <= segments; i++)
    {
        float t = 2.0f * 3.1415926f * i / segments;

        glVertex2f(
            p.x + cosf(t) * radius,
            p.y + sinf(t) * radius
        );
    }

    glEnd();
}

void drawCapsuleBody(AppState* app, CapsuleBody b, int activeHandle)
{
    Point center = getCenter(b);

    float angle = b.angle;

	int topActive = app->draggingTop;
	int bottomActive = app->draggingBottom;

    // rotated circle centers
    Point leftCenter  = rotatePoint((Point){b.leftX,  b.y}, center, angle);
    Point rightCenter = rotatePoint((Point){b.rightX, b.y}, center, angle);

	// inner circle
	Point inner = rotatePoint(b.innerCircle, center, angle);

    // circles
	if (app->sliderDraggingLeft)
	    glColor3f(0.2f, 0.4f, 1.0f); 
	else
	    glColor3f(0, 0, 0);

	drawCircle(leftCenter, app->robotScene.robot.leftRadius);

    if (app->sliderDraggingRight)
	    glColor3f(0.2f, 0.4f, 1.0f); 
	else
	    glColor3f(0, 0, 0);

	drawCircle(rightCenter, app->robotScene.robot.rightRadius);

	if (app->draggingInner)
	    glColor3f(1.0f, 0.85f, 0.35f); // highlight
	else
	    glColor3f(0, 0, 0);

	drawCircle(inner, b.innerRadius);

    // TOP curve (use circle surface, not center)
    Point topP0 = circleEdge(leftCenter,  b.leftRadius,  angle + 90);
    Point topP2 = circleEdge(rightCenter, b.rightRadius, angle + 90);
    Point topP1 = rotatePoint(b.topCtrl, center, angle);

    // BOTTOM curve
    Point botP0 = circleEdge(leftCenter,  b.leftRadius,  angle - 90);
    Point botP2 = circleEdge(rightCenter, b.rightRadius, angle - 90);
    Point botP1 = rotatePoint(b.bottomCtrl, center, angle);

	if (topActive)
	    glColor3f(0.2f, 0.4f, 1.0f); // blue
	else
	    glColor3f(0, 0, 0);

	drawBezier(topP0, topP1, topP2);

	if (bottomActive)
	    glColor3f(0.2f, 0.4f, 1.0f);
	else
	    glColor3f(0, 0, 0);

	drawBezier(botP0, botP1, botP2);


	// HANDLE
	Point topHandle = rotatePoint(b.topCtrl, center, angle);
	Point bottomHandle = rotatePoint(b.bottomCtrl, center, angle);

	drawHandle(topHandle, app->activeHandle == 1, HANDLE_RADIUS);
	drawHandle(bottomHandle, app->activeHandle == 2, HANDLE_RADIUS);

	drawHandle(inner, app->draggingInner, INNER_HANDLE_RADIUS);
	
	if (app->activeHandle == 1)
	    glColor3f(1.0f, 0.3f, 0.3f); // highlight top
	else if (app->activeHandle == 2)
	    glColor3f(0.3f, 0.3f, 1.0f); // highlight bottom
	else
	    glColor3f(0, 0, 0);
}

void renderApp(AppState* app, HDC hdc)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0,0,0);
	
    drawEnvironment();

	if (app->mode == MODE_ENVIRONMENT && app->isDrawingWall)
	{
	    glColor3f(1.0f, 0.0f, 0.0f);

	    glBegin(GL_LINES);
	    glVertex2f(app->wallStart.x, app->wallStart.y);
	    glVertex2f(app->mouseGL.x, app->mouseGL.y);
	    glEnd();
	}
    
    drawCapsuleBody(app, app->robotScene.robot, app->activeHandle);

    // ---- FINAL STROKE (BLACK) ----
    if (app->paintCount >= 4 && !app->painting)
	{
	    glColor3f(0, 0, 0);

	    float thickness = 0.01f;

	    glBegin(GL_TRIANGLE_STRIP);

	    for (int i = 2; i < app->paintCount - 2; i += 2)
	    {
	        float x0 = app->paintPoints[i - 2];
	        float y0 = app->paintPoints[i - 1];

	        float x1 = app->paintPoints[i];
	        float y1 = app->paintPoints[i + 1];

	        float x2 = app->paintPoints[i + 2];
	        float y2 = app->paintPoints[i + 3];

	        if (x0 == BREAK_POINT_X || x1 == BREAK_POINT_X || x2 == BREAK_POINT_X)
	            continue;

	        float dx = x2 - x0;
	        float dy = y2 - y0;

	        float len = sqrtf(dx * dx + dy * dy);
	        if (len == 0) continue;

	        dx /= len;
	        dy /= len;

	        float px = -dy * thickness;
	        float py = dx * thickness;

	        glVertex2f(x1 + px, y1 + py);
	        glVertex2f(x1 - px, y1 - py);
	    }

	    glEnd();
	}

    // ---- LIVE PREVIEW (RED WHILE HOLDING MOUSE) ----
    if (app->mode == MODE_PAINT && app->painting)
	{
	    glColor3f(1.0f, 0.0f, 0.0f);

	    float thickness = 0.01f;

	    glBegin(GL_TRIANGLE_STRIP);

	    for (int i = 2; i < app->paintCount; i += 2)
	    {
	        float x0 = app->paintPoints[i - 2];
	        float y0 = app->paintPoints[i - 1];

	        float x1 = app->paintPoints[i];
	        float y1 = app->paintPoints[i + 1];

	        if (x0 == BREAK_POINT_X || x1 == BREAK_POINT_X)
	            continue;

	        float dx = x1 - x0;
	        float dy = y1 - y0;

	        float len = sqrtf(dx * dx + dy * dy);
	        if (len == 0) continue;

	        dx /= len;
	        dy /= len;

	        float px = -dy * thickness;
	        float py = dx * thickness;

	        glVertex2f(x1 + px, y1 + py);
	        glVertex2f(x1 - px, y1 - py);
	    }

	    // extend last segment to mouse
	    if (app->paintCount >= 2)
	    {
	        float x0 = app->paintPoints[app->paintCount - 2];
	        float y0 = app->paintPoints[app->paintCount - 1];

	        float x1 = app->mouseGL.x;
	        float y1 = app->mouseGL.y;

	        float dx = x1 - x0;
	        float dy = y1 - y0;

	        float len = sqrtf(dx * dx + dy * dy);
	        if (len != 0)
	        {
	            dx /= len;
	            dy /= len;

	            float px = -dy * thickness;
	            float py = dx * thickness;

	            glVertex2f(x1 + px, y1 + py);
	            glVertex2f(x1 - px, y1 - py);
	        }
	    }

	    glEnd();
	}

    SwapBuffers(hdc);
}
