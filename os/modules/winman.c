#include "winman.h"
#include "keyboard.h"
#include "../drawman.h"

uint32_t* BackBuffer;

static int CursorX = 0;
static int CursorY = 0;

#define W 0xFFFFFFFF
#define B 0xFF000000
#define A 0x00000000

static uint32_t CursorImg[64] =
{
    B, B, B, B, B, B, B, A,
    B, W, W, W, W, W, W, B,
    B, W, W, W, W, B, B, A,
    B, W, W, W, W, B, A, A,
    B, W, W, W, W, B, A, A,
    B, W, B, B, B, W, B, A,
    B, W, B, A, A, B, W, B,
    A, B, A, A, A, A, B, W,
};

void EventHandling()
{
    KeyboardKey Key;
    Key = KeyboardPollKey();
    while (Key.Valid)
    {
        if (Key.ASCII == 'd') CursorX += 10;
        if (Key.ASCII == 'a') CursorX -= 10;
        if (Key.ASCII == 'w') CursorY -= 10;
        if (Key.ASCII == 's') CursorY += 10;
        Key = KeyboardPollKey();
    }
}

void NewFrame()
{
    DrawClearFrame();
    float Ms;
    GetMS(&Ms);
    DrawImageStencil(CursorX, CursorY, 8, 8, CursorImg);
    
    EventHandling();
}

void StartWindowManager()
{
    BackBuffer = (uint32_t*)malloc(1920 * 1080 * 4);

    DrawSetContext(BackBuffer, 1920, 1080);

    Log("Window manager started", LOG_SUCCESS);

    LogDisable();

    while (1)
    {
        NewFrame();

        DrawUpdate();
    }

    ExitProc();
}