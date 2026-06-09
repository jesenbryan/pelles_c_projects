#ifndef APP_H
#define APP_H

#include <windows.h>

#include "geometry.h"
#include "mode.h"

#define MAX_POINTS 200000

#define ID_SAVE_BUTTON 1001
#define ID_LOAD_BUTTON 1002

typedef struct {
    float leftX, rightX;
    float y;

    float leftRadius;
    float rightRadius;

    Point topCtrl;
    Point bottomCtrl;

    Point innerCircle;
    float innerRadius;

    float angle;
} Semni;

typedef struct {
    Semni robot;
} RobotScene;

typedef struct {
    int dummy;
} EnvironmentScene;

typedef struct {
    HWND sliderLeft;
    HWND sliderRight;
	HWND labelLeft;
	HWND labelRight;

	HWND hSaveButton;
	HWND hLoadButton;
} UIState;

typedef struct {
	HWND hwndMain;          // main robot window
    HWND hwndUI;            // UI window (SliderWindow)

    RobotScene robotScene;
    EnvironmentScene envScene;

	UIState ui;

    Mode mode;

    int activeHandle;
    int draggingTop;
    int draggingBottom;
    int draggingInner;

    int sliderDraggingLeft;
    int sliderDraggingRight;

	//int showRobot;
    //int showEnvironment;

    Point mouseGL;
    Point wallStart;
    int drawingWall;

	int isDrawingWall;

	float paintPoints[MAX_POINTS];
	int paintCount;
	int painting;

	int strokeBreaks[MAX_POINTS];
	int newStroke;

	DWORD lastLogTime;

} AppState;

#endif
