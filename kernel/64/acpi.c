#include "acpi.h"
#include "vmalloc.h"

RSDP_Descriptor* rsdp;

bool CompareSignature(char *a, char *b)
{
    for (int i = 0; i < 4; i++)
    {
        if (a[i] != b[i]) return false;
    }
    return true;
}

void *ACPIFindTable(char name[4])
{
    if (!rsdp->Xsdt && !rsdp->Revision) 
    {
        uint32_t* TablePtrs = rsdp->Rsdt + 36;
        uint32_t NumEntries = 0x1000;
        for (uint32_t i = 0; i < NumEntries; i++)
        {
            ACPI_SDTHeader *TableHeader = (ACPI_SDTHeader*)TablePtrs[i];
            AllocIdMap(TablePtrs[i], 0x1000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
            if (CompareSignature(TableHeader->Signature, name))
            {
                return TableHeader;
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
            ACPI_SDTHeader *TableHeader = (ACPI_SDTHeader*)TablePtrs[i];
            AllocIdMap(TablePtrs[i], 0x1000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
            if (CompareSignature(TableHeader->Signature, name))
            {
                return TableHeader;
            }
            AllocUnMap(TablePtrs[i], 0x1000);
        }
    }
}

void ACPIInit()
{
    rsdp = 0x7ED0;
    
    if (!rsdp->Xsdt && !rsdp->Revision) AllocIdMap(rsdp->Rsdt, 0x10000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
    else AllocIdMap(rsdp->Xsdt, 0x10000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
}
