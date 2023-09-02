#include "proc.h"
#include "timer.h"

extern void _StartProc(uint64_t, uint64_t);
extern void _ExitProc();

void StartProc(uint64_t Addr, uint64_t Size)
{
    _StartProc(Addr, Size);

    float InitMS;
    GetMS(&InitMS);
    float Ms = 0.0f;
    while (Ms - InitMS < 100.0f) GetMS(&Ms);
}


void ExitProc(uint64_t Addr, uint64_t Size)
{
    _ExitProc();
}