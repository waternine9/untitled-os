#include "panic.h"
#include "../vbe.h"
#include "draw.h"

void _KernelPanic(int Line, const char* File, const char* Message)
{
    Draw_RectEntry Rect;
    Rect.x = 0;
    Rect.y = 0;
    Rect.w = Draw_GetResX();
    Rect.x = Draw_GetResY();
    Rect.col = 0;
    Draw_Rect(Rect);
    Draw_TextEntry Text;
    Text.col = 0xFFFFFFFF;
    Text.x = 10;
    Text.y = 10;
    Text.text = Message;
    Draw_Text(Text);
    Text.x = 10;
    Text.y = 20;
    Text.text = File;
    Draw_Text(Text);
    asm volatile ("cli\nhlt");
}