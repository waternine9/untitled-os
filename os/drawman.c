#include "drawman.h"
#include "../kernel/draw.h"
#include "font.h"
#include "drawcall.h"

static uint32_t* CtxBuff = 0;
static uint32_t CtxW = 0;
static uint32_t CtxH = 0;

void DrawSetContext(uint32_t* buff, uint32_t w, uint32_t h)
{
    CtxBuff = buff;
    CtxW = w;
    CtxH = h;
}

void DrawClearFrame()
{
    for (int i = 0;i < CtxW * CtxH;i++)
    {
        CtxBuff[i] = 0;
    }
}

void DrawText(int x, int y, char* text, uint32_t col)
{
    for (int i = 0;text[i];i++)
    {
        uint8_t* ValP = SysFont[text[i]];
        for (int iy = 0;iy < 8;iy++)
        {
            uint8_t ValY = ValP[iy];
            for (int ix = 0;ix < 8;ix++)
            {
                int nx = ix + i * 0x8 + x;
                int ny = iy + y;
                uint8_t Val = (ValY >> ix) & 1;
                if (!Val) 
                {
                    CtxBuff[(nx + ny * CtxW)] = 0;
                    continue;
                }
                if (nx < 0) continue;
                if (ny < 0) continue;
                if (nx >= CtxW) continue;
                if (ny >= CtxH) continue;
                CtxBuff[(nx + ny * CtxW)] = col;
            }
        }
    }
}

void DrawRect(int x, int y, int w, int h, uint32_t col)
{
    for (int ny = y;ny < y + h;ny++)
    {
        for (int nx = x;nx < x + w;nx++)
        {
            if (nx < 0) continue;
            if (ny < 0) continue;
            if (nx >= CtxW) continue;
            if (ny >= CtxH) continue;
            CtxBuff[(nx + ny * CtxW)] = col;
        }
    }
}

void DrawPoint(int x, int y, uint32_t col)
{
    if (x < 0) return;
    if (y < 0) return;
    if (x >= CtxW) return;
    if (y >= CtxH) return;
    CtxBuff[(x + y * CtxW)] = col;
}

void DrawLine(int x0, int y0, int x1, int y1, uint32_t col)
{
    float fx = x0;
    float fy = y0;
    for (;(int)fx != x1 || (int)fy != y1;fx += ((float)x1 - x0) / CtxW,fy += ((float)y1 - y0) / CtxW)
    {
        int x = fx;
        int y = fy;
        if (x < 0) continue;
        if (y < 0) continue;
        if (x >= CtxW) continue;
        if (y >= CtxH) continue;
        CtxBuff[(x + y * CtxW)] = col;
    }
}

void DrawImageStencil(int x, int y, int w, int h, uint32_t* img)
{
    for (int ny = 0;ny < h;ny++)
    {
        if (ny + y < 0) continue;
        if (ny + y >= CtxH) continue;
        for (int nx = 0;nx < w;nx++)
        {
            if (nx + x < 0) continue;
            if (nx + x >= CtxW) continue;
            uint32_t col = img[nx + ny * w];
            if (col >> 24)
            {
                CtxBuff[((x + nx) + (y + ny) * CtxW)] = col;
            }
        }   
    }
}

void DrawUpdate(int x, int y)
{
    Draw_Packet Packet;
    Packet.imagesSize = 1;
    Packet.rectsSize = 0;
    Packet.linesSize = 0;
    Packet.pointsSize = 0;
    Packet.textsSize = 0;

    Draw_ImageEntry Image;
    Image.image = CtxBuff;
    Image.x = x;
    Image.y = y;
    Image.w = CtxW;
    Image.h = CtxH;

    Packet.images = &Image;

    DrawCall(&Packet);
}
