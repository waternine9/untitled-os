#include "proc.h"
#include "timer.h"

extern void _StartProc(uint64_t, uint64_t);
extern void _ExitProc();

void StartProc(uint64_t Addr, uint64_t Size)
{
    _StartProc(Addr, Size);
}


void ExitProc(uint64_t Addr, uint64_t Size)
{
    _ExitProc();
}