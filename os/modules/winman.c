#include "winman.h"
#include "keyboard.h"
#include "../drawman.h"
#include "helpers/shell.h"

uint32_t* BackBuffer;

static int CmdLen = 0;

static char CurCmd[256] = { 0 };

static char* CurCmdPrefix = "> ";

static int CurY = 0;

void EventHandling()
{
    KeyboardKey Key;
    Key = KeyboardPollKey();
    while (Key.Valid)
    {
        if (!Key.Released)
        {
            if (Key.Scancode == KEY_BACKSPACE)
            {
                CurCmd[CmdLen--] = 0;
                if (CmdLen < 0) CmdLen = 0;
            }
            else if (Key.ASCII == '\n')
            {
                CurY++;
            }
            else if (CmdLen < 255)
            {
                char Buf[2];
                Buf[0] = Key.ASCII;
                Buf[1] = 0;
                Log(Buf, LOG_WARNING);
                CurCmd[CmdLen] = Key.ASCII;
                CmdLen++;
            }
        }
        Key = KeyboardPollKey();
    }
}

void NewFrame(ShellContext* ShellCtx)
{
    float Ms;
    GetMS(&Ms);

    DrawRect(0, 0, 640, 1, 0xFFFFFFFF);

    int OldY = CurY;
    EventHandling();
    DrawRect(0, CurY * 8, 640, 8, 0xFF000000);
    DrawText(0, OldY * 8, CurCmdPrefix, 0xFFFFFFFF);
    DrawText(8 * 3, OldY * 8, CurCmd, 0xFFFFFFFF);
    if (OldY != CurY)
    {
        ShellCtx->ScanY = OldY;
        ShellProcessCommand(CurCmd, ShellCtx);
        CurY = ShellCtx->ScanY;
        CmdLen = 0;
        for (int i = 0;i < 256;i++) CurCmd[i] = 0;
    }
}

void StartWindowManager()
{
    BackBuffer = (uint32_t*)malloc(640 * (480 - 200) * 4);

    DrawSetContext(BackBuffer, 640, 480 - 200);
    DrawClearFrame();

    Log("Window manager started", LOG_SUCCESS);

    ShellContext ShellCtx;

    while (1)
    {
        NewFrame(&ShellCtx);

        DrawUpdate(0, 200);
    }

    ExitProc();
}