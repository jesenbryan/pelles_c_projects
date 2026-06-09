#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>

#include "app.h"

#include "environment.h"
#include "mode.h"

extern AppState app;

Wall walls[MAX_WALLS];
int wallCount = 0;

void addWall(Point p1, Point p2)
{
    if (wallCount >= MAX_WALLS) return;

    walls[wallCount].p1 = p1;
    walls[wallCount].p2 = p2;
    wallCount++;
}

void drawEnvironment(void)
{
    glColor3f(0, 0, 0);

    glBegin(GL_LINES);
    for (int i = 0; i < wallCount; i++)
    {
        glVertex2f(walls[i].p1.x, walls[i].p1.y);
        glVertex2f(walls[i].p2.x, walls[i].p2.y);
    }
    glEnd();

    // preview ONLY if environment mode
    if (app.mode == MODE_ENVIRONMENT && app.drawingWall)
    {
        glColor3f(0.5f, 0.5f, 0.5f);

        glBegin(GL_LINES);
        glVertex2f(app.wallStart.x, app.wallStart.y);
        glVertex2f(app.mouseGL.x, app.mouseGL.y);
        glEnd();
    }
}
