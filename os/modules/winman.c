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
                CmdLen--;
                if (CmdLen < 0) CmdLen = 0;
                CurCmd[CmdLen] = 0;
            }
            else if (Key.ASCII)
            {
                if (Key.ASCII == '\n')
                {
                    CurY++;
                }
                else if (CmdLen < 255)
                {
                    CurCmd[CmdLen] = Key.ASCII;
                    CmdLen++;
                }
            }
        }
        Key = KeyboardPollKey();
    }
}

void NewFrame(ShellContext* ShellCtx)
{
    float Ms;
    GetMS(&Ms);

    DrawRect(0, 0, 1920, 1, 0xFFFFFFFF);

    int OldY = CurY;
    EventHandling();
    CurY %= 100;
    DrawRect(0, CurY * 8, 1920, 8, 0xFF000000);
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

void WMPrintStr(char* Str)
{
    Log(Str, LOG_WARNING);
}

void WMPrintChar(char Char)
{
    LogChar(Char, LOG_INFO);
}

void StartWindowManager()
{
    BackBuffer = (uint32_t*)malloc(1920 * (1080 - 200) * 4);

    DrawSetContext(BackBuffer, 1920, 1080 - 200);
    DrawClearFrame();

    Log("Window manager started", LOG_SUCCESS);

    ShellContext ShellCtx;

    while (1)
    {
        NewFrame(&ShellCtx);

        DrawUpdate(0, 400);
    }

    ExitProc();
}