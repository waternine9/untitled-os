#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800
 
#include "../include.h"
#include "msr.h"
#include "vmalloc.h"

void SetApicBase(uint32_t apic) 
{
   uint32_t edx = 0;
   uint32_t eax = (apic & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;
 
   SetMSR(IA32_APIC_BASE_MSR, eax, edx);
}

uint32_t GetApicBase() 
{
   uint32_t eax, edx;
   GetMSR(IA32_APIC_BASE_MSR, &eax, &edx);
   return (eax & 0xfffff000);
}

void WriteReg(int Offset, uint32_t Value)
{
    *(uint32_t*)(GetApicBase() + Offset) = Value; 
}

uint32_t ReadReg(int Offset)
{
    return *(uint32_t*)(GetApicBase() + Offset); 
}

void ApicInit() 
{
    AllocIdMap(GetApicBase(), 0x10000, MALLOC_CACHE_DISABLE_BIT);

    SetApicBase(GetApicBase());
    
    WriteReg(0xF0, ReadReg(0xF0) | 0x100);
}