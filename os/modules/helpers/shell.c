#include "shell.h"
#include "../../filecall.h"
#include "../../drawman.h"
#include "../../proc.h"
#include "../../log.h"
#include "bf/bf.h"

void ShellCmd_Ls(ShellContext* Ctx, char* Dir)
{
    Ctx->ScanY++;
    FileListResponse Response;
    ListFiles(Dir, &Response);
    for (int i = 0;i < Response.NumEntries;i++)
    {
        DrawText(0, (Ctx->ScanY++) * 8, Response.Data[i].Name, Response.Data[i].IsDir ? 0xFF00FF00 : 0xFF3333FF);
    }
}

bool IsCharacter(char C)
{
    return C >= 32 || C == '\n' || C == '\t';
}

void ShellCmd_Cat(ShellContext* Ctx, char* Dir)
{
    Ctx->ScanY++;
    FileResponse Response;
    ReadFile(Dir, &Response);
    if (Response.Data == 0) return;
    int CharsPrinted = 0;
    for (int i = 0;i < Response.BytesRead;i++)
    {
        if (IsCharacter(((char*)Response.Data)[i]))
        {
            CharsPrinted++;
            char Buf[2];
            Buf[0] = ((char*)Response.Data)[i];
            Buf[1] = 0;
            if (Buf[0] == '\n')
            {
                Ctx->ScanY++;
                CharsPrinted = 0;
            }
            else if (Buf[0] == '\t')
            {
                CharsPrinted += 4;
            }
            else
            {
                DrawText(((CharsPrinted - 1) % 256) * 8, Ctx->ScanY * 8, Buf, 0xFFFFFFFF);
                if (CharsPrinted % 256 == 0) Ctx->ScanY++;
            }
        }
    }
    free(Response.Data);
    Ctx->ScanY++;
}

void ShellCmd_Write(ShellContext* Ctx, char* Dir)
{
    Ctx->ScanY++;

    int DirSize = 0;
    while (Dir[DirSize] && Dir[DirSize] != ' ') DirSize++;
    char Old = Dir[DirSize];
    Dir[DirSize] = 0;
    if (Old == 0) DirSize--;

    int Bytes = 0;
    while (Dir[DirSize + 1 + Bytes]) Bytes++;

    FileRequest Request;
    Request.Bytes = Bytes;
    Request.Data = Dir + DirSize + 1;
    WriteFile(Dir, &Request);
}

void ShellCmd_Mkdir(ShellContext* Ctx, char* Dir)
{
    Ctx->ScanY++;
    MakeDir(Dir);
}
void ShellCmd_Touch(ShellContext* Ctx, char* Dir)
{
    Ctx->ScanY++;
    MakeFile(Dir);
}

static FileResponse BFResponse;

void _BFHandler()
{
    Log("Running BF file...", LOG_INFO);
    BFRunSource(StrFromArray(BFResponse.Data, BFResponse.BytesRead));
    //ExitProc();
}

void ShellCmd_Bf(ShellContext* Ctx, char* Dir)
{
    Ctx->ScanY++;
    FileResponse Response;
    ReadFile(Dir, &Response);
    if (Response.Data == 0) return;
    BFResponse = Response;
    
    _BFHandler();
    //StartProc(_BFHandler, 0x40000ULL | 0x100000000ULL); 
}
void ShellCmd_Rm(ShellContext* Ctx, char* Dir)
{
    Ctx->ScanY++;
    RemoveFileOrDir(Dir);
}
void ShellCmdNotFound(ShellContext* Ctx)
{
    Ctx->ScanY++;
    DrawText(0, (Ctx->ScanY++) * 8, "Command not found", 0xFFFFAAAA);
}
static uint32_t Random(uint32_t seed)
{
    return seed * 1664525 + 1013904223;
}
// returns next X coordinate
static int PrintInt(ShellContext* Ctx, int num, int x)
{
    int NumLen = 0;
    int Temp = num;
    // quick and lazy
    do {NumLen++;} while (Temp /= 10);
    
    char Buf[2];
    Buf[1] = 0;
    Temp = NumLen;
    do
    {
        Buf[0] = '0' + (num % 10);
        DrawText(x + (--NumLen) * 8, Ctx->ScanY * 8, Buf, 0xFFFFFFFF);
    } while (num /= 10);
    return x + Temp * 8;
}
void ShellCmd_Gamble(ShellContext* Ctx, char* Params)
{
    FileResponse res;
    ReadFile("sys/gamble.conf", &res);
    if (!res.Data)
    {
        MakeFile("sys/gamble.conf");
        FileRequest req;
        const int StartBalance = 100; 
        const uint32_t StartRandSeed = 6439857843;
        int Store[2];
        Store[0] = StartBalance;
        Store[1] = StartRandSeed; 
        req.Data = Store;
        req.Bytes = 8;
        WriteFile("sys/gamble.conf", &req);
        ReadFile("sys/gamble.conf", &res);
    }

    int balance = ((int*)res.Data)[0];
    uint32_t randseed = ((uint32_t*)res.Data)[1];
    free(res.Data);
    balance -= 10;

    Ctx->ScanY++;
    DrawText(0, (Ctx->ScanY) * 8, "Your balance is now: ", 0xFFFFFFFF);
    int NextX = PrintInt(Ctx, balance, 22 * 8);
    DrawText(NextX, (Ctx->ScanY++) * 8, "$ (-10$)", 0xFFFFFFFF);
    DrawText(0, (Ctx->ScanY++) * 8, "|WEAR   |WEAPON |SKIN   |STATTRAK", 0xFFFFFFFF);

    // Wear
    uint32_t newseed = Random(randseed);
    randseed = newseed;
    newseed %= 1000;
    if (newseed < 4) DrawText(0, (Ctx->ScanY) * 8, "|FACNEW", 0xFFFFFFFF);
    else if (newseed < 50) DrawText(0, (Ctx->ScanY) * 8, "|MINWEAR", 0xFFFFFFFF);
    else if (newseed < 300) DrawText(0, (Ctx->ScanY) * 8, "|TESTED", 0xFFFFFFFF);
    else DrawText(0, (Ctx->ScanY) * 8, "|BATSCAR", 0xFFFFFFFF);
    /*
    // Weapon
    
    */
    const char* weapons[9] = {
        "|AK-47",
        "|M4A4",
        "|M4A1-S",
        "|USP-S",
        "|GLOCK",
        "|AWP",
        "|SSG",
        "|SCAR-50",
        "|MP5"
    };

    const char* knives[4] = {
        "|GutKnif",
        "|Bayonet",
        "|Karamb ",
        "|Talon"
    };

    newseed = Random(randseed);
    randseed = newseed;
    if (newseed % 100 == 0)
    {
        // Knife
        newseed %= 4;
        DrawText(8 * 8, (Ctx->ScanY) * 8, knives[newseed], 0xFFFFFF00);
    }
    else
    {
        newseed %= 9;
        DrawText(8 * 8, (Ctx->ScanY) * 8, weapons[newseed], 0xFF777777);
    }

    // Skin
    
    const char* skins[7] = {
        "|DDPAT",
        "|Safari",
        "|Doppler",
        "|Camo",
        "|Gamma",
        "|Lore",
        "|Dragon"  
    };

    newseed = Random(randseed);
    randseed = newseed;
    newseed %= 7;
    DrawText(16 * 8, (Ctx->ScanY) * 8, skins[newseed], 0xFFFFFFFF);

    // Stattrak

    newseed = Random(randseed);
    randseed = newseed;

    if (newseed % 10 == 0)
    {
        DrawText(24 * 8, (Ctx->ScanY) * 8, "Stattrak", 0xFF00FF00);
    }
    else
    {
        DrawText(24 * 8, (Ctx->ScanY) * 8, "No", 0xFFFF7777);
    }
    Ctx->ScanY++;

    FileRequest req;
    int Store[2];
    Store[0] = balance;
    Store[1] = randseed; 
    req.Data = Store;
    req.Bytes = 8;
    WriteFile("sys/gamble.conf", &req);
}
bool ShellStrcmp(char* x, char* y)
{
	int xlen = 0, ylen = 0;
	while (x[xlen] && x[xlen] != ' ') xlen++;
	while (y[ylen] && x[ylen] != ' ') ylen++;

	if (xlen != ylen) return false;

	for (int i = 0;i < xlen;i++)
	{
		if (x[i] != y[i]) return false;
	}
	return true;
}
void ShellProcessCommand(char* Cmd, ShellContext* Ctx)
{
    int CmdSize = 0;
    while (Cmd[CmdSize] && Cmd[CmdSize] != ' ') CmdSize++;
    if (Cmd[CmdSize] == 0) CmdSize--;
    if (ShellStrcmp(Cmd, "ls")) ShellCmd_Ls(Ctx, Cmd + CmdSize + 1);
    else if (ShellStrcmp(Cmd, "rm")) ShellCmd_Rm(Ctx, Cmd + CmdSize + 1);
    else if (ShellStrcmp(Cmd, "mkdir")) ShellCmd_Mkdir(Ctx, Cmd + CmdSize + 1);
    else if (ShellStrcmp(Cmd, "touch")) ShellCmd_Touch(Ctx, Cmd + CmdSize + 1);
    else if (ShellStrcmp(Cmd, "write")) ShellCmd_Write(Ctx, Cmd + CmdSize + 1);
    else if (ShellStrcmp(Cmd, "cat")) ShellCmd_Cat(Ctx, Cmd + CmdSize + 1);
    else if (ShellStrcmp(Cmd, "bf")) ShellCmd_Bf(Ctx, Cmd + CmdSize + 1);
    else if (ShellStrcmp(Cmd, "gamble")) ShellCmd_Gamble(Ctx, Cmd + CmdSize + 1);
    else ShellCmdNotFound(Ctx);
}