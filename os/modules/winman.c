#include "winman.h"
#include "keyboard.h"
#include "../drawman.h"

uint32_t* BackBuffer;

static int CurX = 0;

void EventHandling()
{
    KeyboardKey Key;
    Key = KeyboardPollKey();
    while (Key.Valid)
    {
        if (!Key.Released)
        {
            char Buf[2];
            Buf[0] = Key.ASCIIOriginal;
            Buf[1] = 0;
            Log(Buf, LOG_WARNING);
            DrawText(CurX, 10, Buf, 0xFFFFFFFF);
            CurX += 8;
        }
        Key = KeyboardPollKey();
    }
}

void NewFrame()
{
    float Ms;
    GetMS(&Ms);

    DrawRect(0, 0, 1920, 1, 0xFFFFFFFF);
    EventHandling();
}

void StartWindowManager()
{
    BackBuffer = (uint32_t*)malloc(1920 * (1080 - 200) * 4);

    DrawSetContext(BackBuffer, 1920, 1080 - 200);
    DrawClearFrame();

    Log("Window manager started", LOG_SUCCESS);

    while (1)
    {
        NewFrame();

        DrawUpdate(0, 200);
    }

    ExitProc();
}