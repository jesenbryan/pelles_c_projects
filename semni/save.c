#include <stdio.h>
#include "app.h"

void saveRobot(const char* filename, AppState* app)
{
    FILE* f = fopen(filename, "wb");
    if (!f) return;

    fwrite(&app->robotScene.robot,
           sizeof(app->robotScene.robot),
           1,
           f);

    fclose(f);

	char path[MAX_PATH];

	GetCurrentDirectory(MAX_PATH, path);

	printf("Saved robot.dat in: %s\\robot.dat\n", path);
}

int loadRobot(const char* path, AppState* app)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return 0;

    fread(&app->robotScene, sizeof(RobotScene), 1, f);
    fclose(f);

    // reset runtime/editor state
    app->paintCount = 0;
    app->painting = 0;
    app->isDrawingWall = 0;

    return 1;
}
