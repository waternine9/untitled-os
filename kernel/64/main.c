#include "../include.h"
#include "vmalloc.h"
#include "idt.h"
#include "pic.h"
#include "apic.h"
#include "acpi.h"
#include "softtss.h"
#include "draw.h"
#include "scheduler.h"
#include "panic.h"
#include "../vbe.h"
#include "pci.h"

#include "drivers/driverman.h"

#define FRAMEBUFFER_VM 0xFFFFFFFF90000000

extern int SuspendPIT;
extern float MSTicks;

volatile uint64_t __attribute__((section(".main64"))) main64()
{
    for (uint64_t i = 0;i < 0x800000;i += 1)
    {
        *(uint8_t*)(0xFFFFFFFFC1200000 + i) = 0;
    }

    AllocInit();

    Scheduler_Init();

    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;

    if (!AllocMap(FRAMEBUFFER_VM, VbeModeInfo->Framebuffer, (VbeModeInfo->Bpp / 8) * VbeModeInfo->Width * VbeModeInfo->Height, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_WRITE_THROUGH_BIT)))
    {
        KernelPanic("PANIC: Failed to map framebuffer to virtual memory!");
    }

    AllocIdMap(0xB00000, 0x100000, (1ULL << MALLOC_READWRITE_BIT) | (1ULL << MALLOC_USER_SUPERVISOR_BIT));
    Draw_Init(FRAMEBUFFER_VM);


    ACPIInit();
    PicInit();
    PicSetMask(0xFFFF);
    SuspendPIT = 1;
    IDT_Init();
    LoadIDT();
    ApicInit();
    
    DriverMan_Init();

    DriverMan_StorageDevice** StorageDevices;
    size_t StorageDevicesCount;
    DriverMan_GetStorageDevices(&StorageDevices, &StorageDevicesCount);
    DriverMan_FilesysTryFormat(StorageDevices[0], "CustomFS"); 

    if (AllocVMAtStack(0xC00000, 0x100000) == 0) KernelPanic("PANIC: Failed to allocate OS stack!");
    
    SuspendPIT = 0;
    MSTicks = 0;
    return 0xB00000;
}