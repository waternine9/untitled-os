#define IA32_APIC_BASE_MSR 0x1BU
#define IA32_APIC_BASE_MSR_BSP 0x100U // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800U
 
#include "../include.h"
#include "msr.h"
#include "vmalloc.h"
#include "acpi.h"

void* ApicVMAddr;
void* IOApicAddr;
void* IOApicVMAddr;
uint32_t ApicID;
uint32_t IOApicBaseInt;

void SetApicBase(uint64_t apic) 
{
   uint32_t edx = 0;
   uint32_t eax = (apic & 0xfffff000ULL) | IA32_APIC_BASE_MSR_ENABLE;
 
   SetMSR(IA32_APIC_BASE_MSR, eax, edx);
}

uint64_t GetApicBase() 
{
    uint32_t lo, hi;
    GetMSR(IA32_APIC_BASE_MSR, &lo, &hi);
    return lo & 0xfffff000ULL;
}

void WriteReg(uint32_t Offset, uint32_t Value)
{
    *(uint32_t volatile*)(ApicVMAddr + Offset) = Value; 
}

void ApicEOI()
{
    WriteReg(0xB0, 0x0);
}

uint32_t ReadReg(int Offset)
{
    return *(uint32_t volatile*)(ApicVMAddr + Offset); 
}

#define IOAPIC_IOREGSEL   0x00
#define IOAPIC_IOWIN      0x10

uint32_t ReadIoApic(uint32_t reg)
{
   uint32_t volatile *ioapic = (uint32_t volatile *)IOApicVMAddr;
   ioapic[0] = (reg & 0xff);
   return ioapic[4];
}

void WriteIoApic(uint32_t reg, uint32_t value)
{
   uint32_t volatile *ioapic = (uint32_t volatile *)IOApicVMAddr;
   ioapic[0] = (reg & 0xff);
   ioapic[4] = value;
}

void RemapIrqToVector(uint8_t Irq, uint8_t Vector) 
{
    WriteIoApic(0x10 + 2*Irq, Vector);
    WriteIoApic(0x11 + 2*Irq, ReadReg(0x20) << 24);
}

void FindIOAPIC()
{
    IOApicAddr = 0;

    uint8_t *Madt = ACPIFindTable("APIC"); // First find MADT
    if (!Madt)
    {
        *(uint32_t*)0xFFFFFFFF90000000 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF90000004 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF90000008 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF9000000c = 0xFFFFFFFF;
        asm volatile ("cli\nhlt");
    }


    uint8_t *MadtEnd = Madt + *(uint32_t*)(Madt + 4);
    AllocIdMap(Madt + 0x2C, MadtEnd - Madt, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
    for (uint8_t *Ptr = Madt + 0x2C; Ptr < MadtEnd;Ptr += Ptr[1])
    {
        if (*Ptr == 1) if (*(uint32_t*)(Ptr + 8) < 2)
        {
            IOApicAddr = *(uint32_t*)(Ptr + 4);
            IOApicBaseInt = *(uint32_t*)(Ptr + 8);
        }
        if (*Ptr == 0) ApicID = *(uint8_t*)(Ptr + 3);
    }

    if (!IOApicAddr)
    {
        *(uint32_t*)0xFFFFFFFF90000000 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF90000004 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF90000008 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF9000000c = 0xFFFFFFFF;
        asm volatile ("cli\nhlt");
    }


    IOApicVMAddr = AllocVMAtPhys(IOApicAddr, 0x1000);
    for (uint8_t *Ptr = Madt + 0x2C; Ptr < MadtEnd;Ptr += Ptr[1])
    {
        if (*Ptr == 2) 
        {
            if (*(Ptr + 3) == 0 && *(uint32_t*)(Ptr + 4) == 2 && IOApicBaseInt == 1) asm volatile("cli\nhlt");
            RemapIrqToVector(*(uint32_t*)(Ptr + 4), 32 + *(Ptr + 3));
        }
    }
}


void ApicInit() 
{
    ApicVMAddr = AllocVMAtPhys(GetApicBase(), 0x1000);

    SetApicBase(GetApicBase());

    WriteReg(0xF0, (ReadReg(0xF0) | 0x1FF) & ~(1ULL << 12));

    WriteReg(0xE0, ReadReg(0xE0) | 0b1111U);
    
    FindIOAPIC();
    
    //RemapIrqToVector(2, 32);
    RemapIrqToVector(1, 33);
    RemapIrqToVector(15, 0x72);
}