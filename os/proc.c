#include "proc.h"
#include "timer.h"

extern volatile void _StartProc(uint64_t, uint64_t);
extern volatile void _ExitProc();

void StartProc(uint64_t Addr, uint64_t Size)
{
    _StartProc(Addr, Size);
}


void ExitProc(uint64_t Addr, uint64_t Size)
{
    _ExitProc();
}