#include "pic.h"
#include "apic.h"
#include "io.h"
#include "../vbe.h"
#include "softtss.h"
#include "keyboard.h"
#include <stdatomic.h>

int SchedRingIdx = 0;
SoftTSS* SchedRing = 0;
int SchedRingSize = 0;

int SuspendPIT;

float MSTicks = 0; // IN MS
SoftTSS* CHandlerIRQ0(SoftTSS* SaveState)
{
    MSTicks += 1000.0f / (1193182.0f) * 16000.0f;
    
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

    if (SuspendPIT == 0)
    {
        SchedRingIdx++;
        if (SchedRingIdx >= SchedRingSize) SchedRingIdx = 0;

        while (SchedRing[SchedRingIdx].Suspended)
        {
            SchedRingIdx++;
            if (SchedRingIdx >= SchedRingSize) SchedRingIdx = 0;
        }
    }

    ApicEOI();

    return SchedRing + SchedRingIdx;
}

char KeyQueue[KEY_QUEUE_SIZE];
size_t KeyQueueIdx = 0;

/* Keyboard Interrupt */
void CHandlerIRQ1()
{
    while (IO_In8(0x64) & 1)
    {
        char Scancode = IO_In8(0x60);
        for (int i = KEY_QUEUE_SIZE - 1; i > 0;i--)
        {
            KeyQueue[i] = KeyQueue[i - 1];
        }
        KeyQueue[0] = Scancode;
        KeyQueueIdx++;
        if (KeyQueueIdx > KEY_QUEUE_SIZE) KeyQueueIdx = KEY_QUEUE_SIZE;
    }
    ApicEOI();
}
/* Channel for Secondary PIC, don't use. */
void CHandlerIRQ2()
{
    ApicEOI();
}
/* COM2 */
void CHandlerIRQ3()
{
    ApicEOI();
}
/* COM1 */
void CHandlerIRQ4()
{
    ApicEOI();
}
/* LPT2 */
void CHandlerIRQ5()
{
    ApicEOI();
}
/* Floppy Disk */
void CHandlerIRQ6()
{
    ApicEOI();
}
/* LPT1 (spurious) */
void CHandlerIRQ7()
{
    ApicEOI();
}
/* CMOS Real time clock */
void CHandlerIRQ8()
{
    ApicEOI();
}
/* Free for peripherals */
void CHandlerIRQ9()
{
    ApicEOI();
}
/* Free for peripherals */
void CHandlerIRQ10()
{
    ApicEOI();
}
/* Free for peripherals */
void CHandlerIRQ11()
{
    ApicEOI();
}
/* PS/2 Mouse */
void CHandlerIRQ12()
{
    while (IO_In8(0x64) & 1)
    {
        IO_In8(0x60);
        IO_In8(0x60);
        IO_In8(0x60);

    }
    ApicEOI();
}
/* FPU */
void CHandlerIRQ13()
{
    ApicEOI();
}
/* Primary ATA */
void CHandlerIRQ14()
{
    ApicEOI();
}
/* Secondary ATA */
void CHandlerIRQ15()
{
    ApicEOI();
}