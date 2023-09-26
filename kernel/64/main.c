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

static const char* Str = "Long Mode Started";

extern SoftTSS* SchedRing;
extern int SchedRingSize;
extern int SchedRingIdx;

volatile uint64_t __attribute__((section(".main64"))) main64()
{
    for (uint64_t i = 0;i < 0x800000;i += 1)
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

    *(uint32_t*)0xFFFFFFFF90000000 = 0xAA0000AA;
    FSFormat();

    if (AllocVMAtStack(0xC00000, 0x100000) == 0) asm volatile ("cli\nhlt" :: "a"(0x2454));
    *(uint32_t*)0xFFFFFFFF90000000 = 0x44444444;
    
    SchedRing = AllocVM(sizeof(SoftTSS) * 16);
    SchedRing[0].Privilege = 1;
    SchedRingSize = 1;
    SchedRingIdx = 0;

    return 0xB00000;
}