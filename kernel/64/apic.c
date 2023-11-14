#define IA32_APIC_BASE_MSR 0x1BU
#define IA32_APIC_BASE_MSR_BSP 0x100U // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800U
 
#include "../include.h"
#include "msr.h"
#include "vmalloc.h"

void* ApicVMAddr;
void* IOApicAddr;
void* IOApicVMAddr;
uint32_t ApicID;

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

typedef struct
{
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed)) acpi_sdt_header;

typedef struct
{
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t Rsdt;

    uint32_t Length;
    uint64_t Xsdt;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} __attribute__((packed)) rsdp_descriptor;

rsdp_descriptor* rsdp;

bool CompareSignature(char *a, char *b)
{
    for (int i = 0; i < 4; i++)
    {
        if (a[i] != b[i]) return false;
    }
    return true;
}

uint8_t *Madt;

void FindMadt()
{
    if (!rsdp->Xsdt && !rsdp->Revision) 
    {
        uint32_t* TablePtrs = rsdp->Rsdt + 36;
        uint32_t NumEntries = 0x1000;
        for (uint32_t i = 0; i < NumEntries; i++)
        {
            acpi_sdt_header *TableHeader = (acpi_sdt_header*)TablePtrs[i];
            AllocIdMap(TablePtrs[i], 0x1000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
            if (CompareSignature(TableHeader->Signature, "APIC"))
            {
                Madt = TableHeader;
                return;
            }
            AllocUnMap(TablePtrs[i], 0x1000);
        }
    }
    else 
    {
        uint64_t* TablePtrs = rsdp->Xsdt + 36;
        uint32_t NumEntries = 0x1000;
        for (uint32_t i = 0; i < NumEntries; i++)
        {
            acpi_sdt_header *TableHeader = (acpi_sdt_header*)TablePtrs[i];
            AllocIdMap(TablePtrs[i], 0x1000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
            if (CompareSignature(TableHeader->Signature, "APIC"))
            {
                Madt = TableHeader;
                return;
            }
            AllocUnMap(TablePtrs[i], 0x1000);
        }
    }

    
}

void FindIOAPIC()
{
    IOApicAddr = 0;
    rsdp = 0x7ED0;
    
    if (!rsdp->Xsdt && !rsdp->Revision) AllocIdMap(rsdp->Rsdt, 0x10000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
    else AllocIdMap(rsdp->Xsdt, 0x10000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
    Madt = 0;
    FindMadt(); // First find MADT
    if (!Madt)
    {
        *(uint32_t*)0xFFFFFFFF90000000 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF90000004 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF90000008 = 0xFFFFFFFF;
        *(uint32_t*)0xFFFFFFFF9000000c = 0xFFFFFFFF;
        asm volatile ("cli\nhlt");
    }


    uint8_t *MadtEnd = Madt + 0x2C + *(uint32_t*)(Madt + 4);
    AllocIdMap(Madt + 0x2C, 0x1000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
    for (uint8_t *Ptr = Madt + 0x2C; Ptr < MadtEnd;Ptr += Ptr[1])
    {
        if (*Ptr == 1) IOApicAddr = *(uint32_t*)(Ptr + 4);
        else if (*Ptr == 0) ApicID = *(uint8_t*)(Ptr + 3);
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
    WriteIoApic(0x11 + 2*Irq, ApicID << 24);
}

void ApicInit() 
{
    ApicVMAddr = AllocVMAtPhys(GetApicBase(), 0x1000);

    SetApicBase(GetApicBase());

    WriteReg(0xF0, (ReadReg(0xF0) | 0x1FF) & ~(1ULL << 12));

    WriteReg(0xE0, ReadReg(0xE0) | 0b1111U);

    FindIOAPIC();
    
    RemapIrqToVector(2, 32); // PIT
    RemapIrqToVector(1, 33); // Keyboard
    for (int i = 3;i < 16;i++) RemapIrqToVector(i, i + 32);
}