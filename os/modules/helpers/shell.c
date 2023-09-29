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
    else ShellCmdNotFound(Ctx);
}