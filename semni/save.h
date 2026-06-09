#ifndef SAVE_H
#define SAVE_H

#include "app.h"

void saveRobot(const char* filename, AppState* app);

int loadRobot(const char* path, AppState* app);

#endif
