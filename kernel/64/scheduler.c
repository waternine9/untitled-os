#include "scheduler.h"
#include "vmalloc.h"

static SoftTSS* SchedRing = 0;
static int SchedRingIdx = 0;
static int SchedRingSize = 0;

void Scheduler_Init()
{
    SchedRing = AllocVM(sizeof(SoftTSS) * 128);
    SchedRing[0].Privilege = 1;
    SchedRing[0].Suspended = 0;
    SchedRing[0].SuspendIdx = 0;
    SchedRingSize = 1;
    SchedRingIdx = 0;
}

static void StepRing()
{
    SchedRingIdx++;
    if (SchedRingIdx >= SchedRingSize) SchedRingIdx = 0;
}

static void SkipSuspended()
{
    while (SchedRing[SchedRingIdx].Suspended)
    {
        StepRing();
    }
}

SoftTSS* Scheduler_NextProcess(SoftTSS* SaveState)
{
    if (SchedRingIdx != 0x7FFFFFFF)
    {
        if (SchedRingIdx >= SchedRingSize) SchedRingIdx = 0;
        if (SchedRingIdx < 0) SchedRingIdx = 0;

        if (SchedRingSize <= 0) SchedRingSize = 1;

        uint64_t Priv = SchedRing[SchedRingIdx].Privilege;
        uint64_t StackStart = SchedRing[SchedRingIdx].StackStart;
        uint64_t Suspended = SchedRing[SchedRingIdx].Suspended;
        uint64_t SuspendIdx = SchedRing[SchedRingIdx].SuspendIdx;
        SchedRing[SchedRingIdx] = *SaveState;
        SchedRing[SchedRingIdx].Privilege = Priv;
        SchedRing[SchedRingIdx].StackStart = StackStart;
        SchedRing[SchedRingIdx].Suspended = Suspended;
        SchedRing[SchedRingIdx].SuspendIdx = SuspendIdx;
    }
    else
    {
        SchedRingIdx = 0;
    }

    StepRing();

    SkipSuspended();
    
    return SchedRing + SchedRingIdx;
}

void Scheduler_AddProcess(uint64_t rip, uint64_t Size)
{
    if ((SchedRingSize % 128) == 127) 
    {
        SoftTSS* NewRing = (SoftTSS*)AllocVM(sizeof(SoftTSS) * (SchedRingSize / 128 * 128 + 128));
        for (int i = 0;i < SchedRingSize;i++)
        {
            NewRing[i] = SchedRing[i];
        }
        FreeVM((uint64_t)SchedRing);
        SchedRing = NewRing;
    }

    SchedRing[SchedRingSize].Privilege = (Size & (uint64_t)0x100000000) >> 32;
    SchedRing[SchedRingSize].cs = 0x20 | 3;
    SchedRing[SchedRingSize].ds = 0x28 | 3;
    SchedRing[SchedRingSize].rip = rip;
    SchedRing[SchedRingSize].rsp = AllocVM(0x40000) + 0x40000;
    SchedRing[SchedRingSize].StackStart = SchedRing[SchedRingSize].rsp - 0x40000;
    SchedRing[SchedRingSize].rflags = 0x200;
    SchedRing[SchedRingSize].SuspendIdx = 0;
    SchedRing[SchedRingSize].Suspended = 0;
    SchedRingSize++;
}

SoftTSS* Scheduler_EndCurrentProcess(bool SkipToStart)
{
    if (SchedRingSize == 0) return;

    if (SchedRing[SchedRingIdx].SuspendIdx) SchedRing[SchedRing[SchedRingIdx].SuspendIdx - 1].Suspended = 0;

    for (int i = 0;i < SchedRingSize;i++)
    {
        if (i == SchedRingIdx) continue;
        if (SchedRing[i].SuspendIdx) if (SchedRing[i].SuspendIdx - 1 > SchedRingIdx) SchedRing[i].SuspendIdx--;
    }

    FreeVM(SchedRing[SchedRingIdx].StackStart);

    for (int i = SchedRingIdx + 1;i < SchedRingSize;i++)
    {
        SchedRing[i - 1] = SchedRing[i];
    }

    SchedRingSize--;

    if (SkipToStart) SchedRingIdx = 0;
    else SchedRingIdx = 0x7FFFFFFF;
    return SchedRing + SchedRingIdx;
}

void Scheduler_AddSyscallProcess(uint64_t rip, uint64_t Code, uint64_t rsi, uint64_t Selector)
{
    if ((SchedRingSize % 128) == 127) 
    {
        SoftTSS* NewRing = (SoftTSS*)AllocVM(sizeof(SoftTSS) * (SchedRingSize / 128 * 128 + 128));
        for (int i = 0;i < SchedRingSize;i++)
        {
            NewRing[i] = SchedRing[i];
        }
        FreeVM((uint64_t)SchedRing);
        SchedRing = NewRing;
    }

    SchedRing[SchedRingSize].Privilege = 1;
    SchedRing[SchedRingSize].cs = 0x18;
    SchedRing[SchedRingSize].ds = 0x10;
    SchedRing[SchedRingSize].rsi = rsi;
    SchedRing[SchedRingSize].rdi = Code;
    SchedRing[SchedRingSize].rdx = Selector;
    SchedRing[SchedRingSize].rip = rip;
    SchedRing[SchedRingSize].rsp = AllocVM(0x40000) + 0x40000;
    SchedRing[SchedRingSize].StackStart = SchedRing[SchedRingSize].rsp - 0x40000;
    SchedRing[SchedRingSize].rflags = 0x200;
    SchedRing[SchedRingSize].SuspendIdx = SchedRingIdx + 1;
    SchedRing[SchedRingSize].Suspended = 0;
    SchedRing[SchedRingIdx].Suspended = 1;
    SchedRingSize++;
}