#ifndef DRAWCALL_H
#define DRAWCALL_H

#include "include.h"

volatile void DrawSetContext(uint32_t* buff, uint32_t w, uint32_t h);
volatile void DrawClearFrame();
volatile void DrawText(int x, int y, char* text, uint32_t col);
volatile void DrawRect(int x, int y, int w, int h, uint32_t col);
volatile void DrawPoint(int x, int y, uint32_t col);
volatile void DrawLine(int x0, int y0, int x1, int y1, uint32_t col);
volatile void DrawImageStencil(int x, int y, int w, int h, uint32_t* img);
volatile void DrawUpdate(int x, int y);

#endif // DRAWCALL_H