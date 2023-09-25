#include "shell.h"
#include "../../filecall.h"
#include "../../drawman.h"

void ShellCmd_Ls(ShellContext* Ctx, char* Dir)
{
    Ctx->ScanY++;
    FileListResponse Response;
    ListFiles(Dir, &Response);
    for (int i = 0;i < Response.NumEntries;i++)
    {
        DrawText(0, (Ctx->ScanY++) * 8, Response.Data[i].Name, Response.Data[i].IsDir ? 0xFF00FF00 : 0xFF0000FF);
    }
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
    else if (ShellStrcmp(Cmd, "mkdir")) ShellCmd_Mkdir(Ctx, Cmd + CmdSize + 1);
    else if (ShellStrcmp(Cmd, "touch")) ShellCmd_Touch(Ctx, Cmd + CmdSize + 1);
    else ShellCmdNotFound(Ctx);
}