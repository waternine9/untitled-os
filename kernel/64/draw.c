#include "draw.h"
#include "font.h"
#include "../vbe.h"

static void* OutFramebuffer;

static uint32_t TransformCol(uint32_t Col)
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    uint32_t Blue = Col & 0xFF;
    uint32_t Green = (Col & 0xFF00) >> 8;
    uint32_t Red = (Col & 0xFF0000) >> 16;
    return (Red << VbeModeInfo->RedPosition) | (Green << VbeModeInfo->GreenPosition) | (Blue << VbeModeInfo->BluePosition);
}

void Draw_Init(void* Framebuffer)
{
    OutFramebuffer = Framebuffer;
}

void Draw_Point(Draw_PointEntry Point)
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    if (Point.x < 0) return;
    if (Point.y < 0) return;
    if (Point.x >= VbeModeInfo->Width) return;
    if (Point.y >= VbeModeInfo->Height) return;

    Point.col = TransformCol(Point.col);

    *(uint32_t*)(OutFramebuffer + (Point.x + Point.y * VbeModeInfo->Width) * 4) = Point.col;
}

void Draw_Line(Draw_LineEntry Line)
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    Line.col = TransformCol(Line.col);

    float fx = Line.x0;
    float fy = Line.y0;
    for (;(int)fx != Line.x1 || (int)fy != Line.y1;fx += ((float)Line.x1 - Line.x0) / VbeModeInfo->Width,fy += ((float)Line.y1 - Line.y0) / VbeModeInfo->Width)
    {
        int x = fx;
        int y = fy;
        if (x < 0) continue;
        if (y < 0) continue;
        if (x >= VbeModeInfo->Width) continue;
        if (y >= VbeModeInfo->Height) continue;
        *(uint32_t*)(OutFramebuffer + (x + y * VbeModeInfo->Width) * 4) = Line.col;
    }
}

void Draw_Rect(Draw_RectEntry Rect)
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    Rect.col = TransformCol(Rect.col);

    for (int y = Rect.y;y < Rect.y + Rect.h;y++)
    {
        if (y < 0) continue;
        if (y >= VbeModeInfo->Height) continue;
        for (int x = Rect.x;x < Rect.x + Rect.w;x++)
        {
            if (x < 0) continue;
            if (x >= VbeModeInfo->Width) continue;
            *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = Rect.col;
        }
    }
}

void Draw_Text(Draw_TextEntry Text)
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    Text.col = TransformCol(Text.col);

    for (int i = 0;Text.text[i];i++)
    {
        uint8_t* ValP = SysFont[Text.text[i]];
        for (int iy = 0;iy < 8;iy++)
        {
            uint8_t ValY = ValP[iy];
            for (int ix = 0;ix < 8;ix++)
            {
                int x = ix + i * 0x8 + Text.x;
                int y = iy + Text.y;
                uint8_t Val = (ValY >> ix) & 1;
                if (!Val) 
                {
                    *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = 0;
                    continue;
                }
                if (x < 0) continue;
                if (y < 0) continue;
                if (x >= VbeModeInfo->Width) continue;
                if (y >= VbeModeInfo->Height) continue;
                *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = Text.col;
            }
        }
    }
}

void Draw_Image(Draw_ImageEntry Image)
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    for (int y = Image.y;y < Image.y + Image.h;y++)
    {
        if (y < 0) continue;
        if (y >= VbeModeInfo->Height) continue;
        for (int x = Image.x;x < Image.x + Image.w;x++)
        {
            if (x < 0) continue;
            if (x >= VbeModeInfo->Width) continue;
            *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = TransformCol(Image.image[(x - Image.x) + (y - Image.y) * Image.w]);
        }
    }
}

size_t Draw_GetResX()
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    return VbeModeInfo->Width;
}

size_t Draw_GetResY()
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    return VbeModeInfo->Height;
}