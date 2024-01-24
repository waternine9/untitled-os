#include "vmalloc32.h"

#define MAX_ADDR_BIT 47
#define PRESENT_BIT 0
#define READWRITE_BIT 1
#define USER_SUPERVISOR_BIT 2
#define WRITE_THROUGH_BIT 3
#define CACHE_DISABLE_BIT 4
#define ACCESSED_BIT 5
#define PAGE_SIZE_BIT 7
#define ADDR_BIT 12
#define AVAILABLE_BIT 52
#define NO_EXECUTE_BIT 63
#define PMT_SIZE 512

typedef struct
{
    uint32_t Low;
    uint32_t High;
} __attribute__((packed)) PageMapTable;

PageMapTable* Tier4;
PageMapTable* AllocOffset = 0x500000;

volatile PageMapTable InitTable(int Depth)
{
    PageMapTable Ret;
    Ret.High = 0;
    Ret.Low = (size_t)(AllocOffset) & 0xFFFFF000;
    
    PageMapTable* TempAllocOffset = AllocOffset;
    AllocOffset += PMT_SIZE;
    if (Depth < 3) for (int i = 0;i < PMT_SIZE;i++)
    {
        (*TempAllocOffset) = InitTable(Depth + 1);
        TempAllocOffset++;
    }
    return Ret;
}

volatile void Init()
{
    
    AllocOffset = 0x10000000 + 8 * PMT_SIZE;
    Tier4 = 0x10000000;
    for (int i = 0;i < PMT_SIZE;i++)
    {
        Tier4[i] = (PageMapTable){ 0, 0 };
    }
}

volatile bool AllocPage(PageMapTable* BasePte, uint64_t vAddr, uint32_t pAddr, int Depth, int L4Add, int L3Add)
{
    uint32_t Index = ((uint32_t)vAddr & (0x1FF << ((3 - Depth) * 9 + 12))) >> ((3 - Depth) * 9 + 12);
    if (Depth == 0) Index = L4Add;
    if (Depth == 1) Index += L3Add;
    if (Depth == 3)
    {
        BasePte[Index].High = 0;
        BasePte[Index].Low = (uint32_t)pAddr & 0xFFFFF000;
        BasePte[Index].Low |= 1 << PRESENT_BIT;
        BasePte[Index].Low |= 1 << READWRITE_BIT;
        return true;
    }
    if (BasePte[Index].Low == 0) 
    {
        BasePte[Index].High = 0;
        AllocOffset += PMT_SIZE;
        BasePte[Index].Low = (size_t)AllocOffset & 0xFFFFF000;
        for (int i = 0;i < PMT_SIZE;i++)
        {
            AllocOffset[i] = (PageMapTable){ 0, 0 };
        }
    }
    BasePte[Index].Low |= 1 << PRESENT_BIT;
    BasePte[Index].Low |= 1 << READWRITE_BIT;
    return AllocPage(BasePte[Index].Low & 0xFFFFF000, vAddr, pAddr, Depth + 1, L4Add, L3Add);
}

volatile bool IdMap(uint32_t Addr, uint32_t Size)
{
    for (int i = 0;i < Size / 0x1000;i++)
    {
        if (!AllocPage(Tier4, Addr + i * 0x1000, Addr + i * 0x1000, 0, 0, 0)) return false;
    }
    return true;
}

volatile bool Map(uint32_t vAddr, uint32_t pAddr, uint32_t Size)
{
    for (int i = 0;i < Size / 0x1000;i++)
    {
        if (!AllocPage(Tier4, vAddr + i * 0x1000, pAddr + i * 0x1000, 0, 0, 0)) return false;
    }
    return true;
}

volatile bool MapHigher(uint32_t vAddr, uint32_t pAddr, uint32_t Size)
{
    for (int i = 0;i < Size / 0x1000;i++)
    {
        if (!AllocPage(Tier4, vAddr + i * 0x1000, pAddr + i * 0x1000, 0, PMT_SIZE - 1, 508)) return false;
    }
    return true;
}

volatile void StoreNumber()
{
    *(uint32_t*)0x7FF0 = AllocOffset;
}