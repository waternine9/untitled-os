#ifndef DRAW_H
#define DRAW_H

#include "../include.h"
#include "../draw.h"

void Draw_Init(void* Framebuffer);
void Draw_Point(Draw_PointEntry Point);
void Draw_Line(Draw_LineEntry Line);
void Draw_Rect(Draw_RectEntry Rect);
void Draw_Text(Draw_TextEntry Text);
void Draw_Image(Draw_ImageEntry Image);
size_t Draw_GetResX();
size_t Draw_GetResY();

#endif // DRAW_H