#ifndef DRAWCALL_H
#define DRAWCALL_H

#include "include.h"

void DrawSetContext(uint32_t* buff, uint32_t w, uint32_t h);
void DrawClearFrame();
void DrawText(int x, int y, const char* text, uint32_t col);
void DrawRect(int x, int y, int w, int h, uint32_t col);
void DrawPoint(int x, int y, uint32_t col);
void DrawLine(int x0, int y0, int x1, int y1, uint32_t col);
void DrawImageStencil(int x, int y, int w, int h, uint32_t* img);
void DrawUpdate();

#endif // DRAWCALL_H