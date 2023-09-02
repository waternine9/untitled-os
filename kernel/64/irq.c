#include "pic.h"
#include "io.h"
#include "../vbe.h"
#include "softtss.h"
#include "keyboard.h"

int SchedRingIdx = 0;
SoftTSS* SchedRing = 0;
int SchedRingSize = 0;

float MSTicks = 0; // IN MS
SoftTSS* CHandlerIRQ0(SoftTSS* SaveState)
{
    MSTicks += 54.9254f;

    if (SchedRingIdx >= SchedRingSize) SchedRingIdx = 0;
    if (SchedRingIdx < 0) SchedRingIdx = 0;

    if (SchedRingSize <= 0) SchedRingSize = 1;

    uint64_t Priv = SchedRing[SchedRingIdx].Privilege;  
    uint64_t CodeStart = SchedRing[SchedRingIdx].CodeStart;  
    uint64_t StackStart = SchedRing[SchedRingIdx].StackStart;  
    SchedRing[SchedRingIdx] = *SaveState;
    SchedRing[SchedRingIdx].Privilege = Priv;
    SchedRing[SchedRingIdx].CodeStart = CodeStart;
    SchedRing[SchedRingIdx].StackStart = StackStart;

    SchedRingIdx++;
    if (SchedRingIdx >= SchedRingSize) SchedRingIdx = 0;
    
    PicEndOfInterrupt(0);

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
    PicEndOfInterrupt(1);
}
/* Channel for Secondary PIC, don't use. */
void CHandlerIRQ2()
{
    PicEndOfInterrupt(2);
}
/* COM2 */
void CHandlerIRQ3()
{
    PicEndOfInterrupt(3);
}
/* COM1 */
void CHandlerIRQ4()
{
    PicEndOfInterrupt(4);
}
/* LPT2 */
void CHandlerIRQ5()
{
    PicEndOfInterrupt(5);
}
/* Floppy Disk */
void CHandlerIRQ6()
{
    PicEndOfInterrupt(6);
}
/* LPT1 (spurious) */
void CHandlerIRQ7()
{
    PicEndOfInterrupt(7);
}
/* CMOS Real time clock */
void CHandlerIRQ8()
{
    PicEndOfInterrupt(8);
}
/* Free for peripherals */
void CHandlerIRQ9()
{
    PicEndOfInterrupt(9);
}
/* Free for peripherals */
void CHandlerIRQ10()
{
    PicEndOfInterrupt(10);
}
/* Free for peripherals */
void CHandlerIRQ11()
{
    PicEndOfInterrupt(11);
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
    PicEndOfInterrupt(12);
}
/* FPU */
void CHandlerIRQ13()
{
    PicEndOfInterrupt(13);
}
/* Primary ATA */
void CHandlerIRQ14()
{
    PicEndOfInterrupt(14);
}
/* Secondary ATA */
void CHandlerIRQ15()
{
    PicEndOfInterrupt(15);
}