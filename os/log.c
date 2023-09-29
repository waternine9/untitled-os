#include "log.h"
#include "../kernel/draw.h"
#include "drawcall.h"
#include "timer.h"

static int CurY = 0;
static int CurX = 0;
static Draw_TextEntry LogTextPair[2];
static bool DoLogging = true;

void Log(char* Msg, LogSeverity Severity)
{
    if (!DoLogging) return;

    Draw_TextEntry text;
    text.text = Msg;
    text.x = 8 * 21 + CurX;
    text.y = CurY;
    if (Severity == LOG_SEVERE) text.col = 0x00FF6565;
    if (Severity == LOG_WARNING) text.col = 0x00FFAA33;
    if (Severity == LOG_INFO) text.col = 0x00FFFFFF;
    if (Severity == LOG_SUCCESS) text.col = 0x0065FF65;

    char Buff[200];
    for (int i = 0;i < 200;i++)
    {
        Buff[i] = ' ';
    }
    Buff[0] = '[';
    Buff[14] = '.';
    Buff[19] = ']';
    Buff[199] = 0;

    float MS;
    GetMS(&MS);

    MS /= 1000.0f;

    float Mul = 10.0f;
    for (int i = 0;i < 4;i++)
    {
        int Val = (int)(MS * Mul) % 10;
        Buff[i + 15] = '0' + Val;
        Mul *= 10.0f;
    }

    Mul = 1.0f;
    for (int i = 0;i < 7;i++)
    {
        if ((int)(MS * Mul) == 0) break;
        int Val = (int)(MS * Mul) % 10;
        Buff[13 - i] = '0' + Val;
        Mul /= 10.0f; 
    }

    Draw_TextEntry textTime;
    textTime.text = Buff;
    textTime.x = 0;
    textTime.y = CurY;
    textTime.col = 0xFFFFFF;

    LogTextPair[0] = textTime;
    LogTextPair[1] = text;

    Draw_Packet packet;
    packet.linesSize = 0;
    packet.rectsSize = 0;
    packet.pointsSize = 0;
    packet.textsSize = 2;
    packet.imagesSize = 0;

    packet.texts = LogTextPair;

    DrawCall(&packet);

    CurY += 10;
    CurY = CurY % 400;
    CurX = 0;
}

void LogChar(char Char, LogSeverity Severity)
{
    if (!DoLogging) return;

    if (Char != '\n')
    {
        char Buf[2];
        Buf[0] = Char;
        Buf[1] = 0;

        Draw_TextEntry text;
        text.text = Buf;
        text.x = 8 * 21 + CurX;
        text.y = CurY;
        if (Severity == LOG_SEVERE) text.col = 0x00FF6565;
        if (Severity == LOG_WARNING) text.col = 0x00FFAA33;
        if (Severity == LOG_INFO) text.col = 0x00FFFFFF;
        if (Severity == LOG_SUCCESS) text.col = 0x0065FF65;

        char Buff[200];
        for (int i = 0;i < 200;i++)
        {
            Buff[i] = ' ';
        }
        Buff[0] = '[';
        Buff[14] = '.';
        Buff[19] = ']';
        Buff[20] = 0;

        float MS;
        GetMS(&MS);

        MS /= 1000.0f;

        float Mul = 10.0f;
        for (int i = 0;i < 4;i++)
        {
            int Val = (int)(MS * Mul) % 10;
            Buff[i + 15] = '0' + Val;
            Mul *= 10.0f;
        }

        Mul = 1.0f;
        for (int i = 0;i < 7;i++)
        {
            if ((int)(MS * Mul) == 0) break;
            int Val = (int)(MS * Mul) % 10;
            Buff[13 - i] = '0' + Val;
            Mul /= 10.0f; 
        }

        Draw_TextEntry textTime;
        textTime.text = Buff;
        textTime.x = 0;
        textTime.y = CurY;
        textTime.col = 0xFFFFFF;

        LogTextPair[0] = textTime;
        LogTextPair[1] = text;

        Draw_Packet packet;
        packet.linesSize = 0;
        packet.rectsSize = 0;
        packet.pointsSize = 0;
        packet.textsSize = 2;
        packet.imagesSize = 0;

        packet.texts = LogTextPair;

        DrawCall(&packet);

        CurX += 8;
    }
    if (Char == '\n')
    {
        CurX = 0;
        CurY += 10;
        CurY = CurY % 400;
    }
}

void LogDisable()
{
    DoLogging = false;
}

void LogEnable()
{
    DoLogging = true;
}