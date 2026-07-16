#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "geometry.h"
#include "file_dialogs.h"

int savePathCSV(const char *filename, Point path[], int numPoints);
