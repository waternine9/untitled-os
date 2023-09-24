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
#include "drivers/ide/ide.h"
#include "drivers/nvme/nvme.h"

static const char* Str = "Long Mode Started";

extern SoftTSS* SchedRing;
extern int SchedRingSize;
extern int SchedRingIdx;

volatile uint64_t __attribute__((section(".main64"))) main64()
{
    for (uint64_t i = 0;i < 0x1000000;i += 1)
    {
        *(uint8_t*)(0xFFFFFFFFC1200000 + i) = 0;
    }

    AllocInit();


    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;

    if (!AllocMap(0xFFFFFFFF90000000, VbeModeInfo->Framebuffer, (VbeModeInfo->Bpp / 8) * VbeModeInfo->Width * VbeModeInfo->Height, (1 << MALLOC_READWRITE_BIT)));
    //AllocMap(0xFFFFFFFF90000000, 0xb8000, 80 * 25 * 2, (1 << MALLOC_READWRITE_BIT));

    AllocIdMap(0xB00000, 0x100000, (1ULL << MALLOC_READWRITE_BIT) | (1ULL << MALLOC_USER_SUPERVISOR_BIT));

    PicInit();
    PicSetMask(0xFFFF);
    ApicInit();
    IdtInit();
    NVMEInit();
    PicSetMask(0x0);

    void* Dest = AllocPhys(0x10000);
    for (int i = 0;i < 0x10000;i++)
    {
        *(uint8_t*)(Dest + i) = 0;
    }
    NVMERead(10, 0, Dest);
    for (int i = 0;i < 0x10000;i += 4)
    {
        *(uint32_t*)(0xFFFFFFFF90000000 + i)  = *(uint32_t*)(Dest + i);
    }
    asm volatile ("cli\nhlt" :: "a"(Dest));

    if (AllocVMAt(0xB00000, 0x100000) == 0) asm volatile ("cli\nhlt" :: "a"(0x2454));
    if (AllocVMAtStack(0xC00000, 0x100000) == 0) asm volatile ("cli\nhlt" :: "a"(0x2454));
    
    SchedRing = AllocVM(sizeof(SoftTSS) * 16);
    SchedRing[0].Privilege = 1;
    SchedRingSize = 1;
    SchedRingIdx = 0;

    return 0xB00000;
}