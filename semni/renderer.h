#ifndef RENDERER_H
#define RENDERER_H

#include <windows.h>
#include "app.h"

typedef struct {
    int activeHandle;
    int draggingTop;
    int draggingBottom;
    int draggingInner;
    int sliderDraggingLeft;
    int sliderDraggingRight;
} RenderState;

void renderApp(AppState* app, HDC hdc);
void drawSemni(Semni b, RenderState* rs);

#endif
