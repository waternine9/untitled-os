#include "../include.h"
#include "vmalloc.h"
#include "idt.h"
#include "pic.h"
#include "apic.h"
#include "softtss.h"
#include "../vbe.h"
#include "pci.h"

// DRIVER INCLUDES
#include "drivers/ps2/ps2.h"
#include "drivers/fs/fs.h"
#include "drivers/ide/ide.h"
#include "drivers/nvme/nvme.h"
#include "drivers/usb/xhci.h"

extern SoftTSS* SchedRing;
extern int SchedRingSize;
extern int SchedRingIdx;

extern int Int70Fired;
extern int SuspendPIT;

extern void LoadIDT();

volatile uint64_t __attribute__((section(".main64"))) main64()
{
    for (uint64_t i = 0;i < 0x800000;i += 1)
    {
        *(uint8_t*)(0xFFFFFFFFC1200000 + i) = 0;
    }

    AllocInit();


    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;

    if (!AllocMap(0xFFFFFFFF90000000, VbeModeInfo->Framebuffer, (VbeModeInfo->Bpp / 8) * VbeModeInfo->Width * VbeModeInfo->Height, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_WRITE_THROUGH_BIT)));

    AllocIdMap(0xB00000, 0x100000, (1ULL << MALLOC_READWRITE_BIT) | (1ULL << MALLOC_USER_SUPERVISOR_BIT));

    SchedRing = AllocVM(sizeof(SoftTSS) * 128);
    SchedRing[0].Privilege = 1;
    SchedRing[0].Suspended = 0;
    SchedRing[0].SuspendIdx = 0;
    SchedRingSize = 1;
    SchedRingIdx = 0;

    PicInit();
    PicSetMask(0xFFFF);
    SuspendPIT = 1;
    Int70Fired = 1;
    IdtInit();
    LoadIDT();
    ApicInit();

    NVMEInit();

    FSTryFormat();
    FSMkdir("home");
    FSMkdir("bin");
    FSMkdir("etc");
    FSMkdir("sys");

    XHCIInit();

    if (AllocVMAtStack(0xC00000, 0x100000) == 0) asm volatile ("cli\nhlt" :: "a"(0x2454));
    
    SuspendPIT = 0;
    return 0xB00000;
}