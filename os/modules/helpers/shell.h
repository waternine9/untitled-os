#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    char Directory[512];
    int ScanY;
} ShellContext;

void ShellProcessCommand(char* Cmd, ShellContext* Ctx);

#endif // SHELL_H