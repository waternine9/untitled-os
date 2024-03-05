#include "vmalloc.h"

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
    uint64_t Data;
} __attribute__((packed, scalar_storage_order("little-endian"))) PageMapTable;

PageMapTable* Tier4;
PageMapTable* AllocOffset = 0x400000;

volatile PageMapTable InitTable(int Depth)
{
    PageMapTable Ret;
    Ret.Data = (uint64_t)AllocOffset & 0xFFFFFFFFF000;
    
    PageMapTable* TempAllocOffset = AllocOffset;
    AllocOffset += PMT_SIZE;
    if (Depth < 3) for (int i = 0;i < PMT_SIZE;i++)
    {
        (*TempAllocOffset) = InitTable(Depth + 1);
        TempAllocOffset++;
    }
    return Ret;
}

volatile bool AllocPage(PageMapTable* BasePte, uint64_t vAddr, uint64_t pAddr, int Depth, uint64_t Flags)
{
    uint64_t Index = (vAddr & (0x1FFULL << ((3 - Depth) * 9 + 12))) >> ((3 - Depth) * 9 + 12);
    Index &= 0x1FF;
    if (Depth == 3)
    {
        BasePte[Index].Data = Flags;
        BasePte[Index].Data |= pAddr & 0xFFFFFFFFF000;
        BasePte[Index].Data |= 1 << PRESENT_BIT;
        BasePte[Index].Data |= 1 << READWRITE_BIT;
        return true;
    }
    if (BasePte[Index].Data == 0)
    {
        AllocOffset += PMT_SIZE;
        if (AllocOffset >= 0x20000000) 
        {
            return false;
        }
        BasePte[Index].Data = Flags;
        BasePte[Index].Data |= (uint64_t)AllocOffset & 0xFFFFFFFFF000;
        for (int i = 0;i < PMT_SIZE;i++)
        {
            AllocOffset[i].Data = 0;
        }
    }
    else
    {
        BasePte[Index].Data &= 0x7FFFFFFFFFFFF000;
        BasePte[Index].Data |= Flags & 0b111111111;
    }
    BasePte[Index].Data |= 1 << PRESENT_BIT;
    BasePte[Index].Data |= 1 << READWRITE_BIT;
    return AllocPage(BasePte[Index].Data & 0xFFFFFFFFF000, vAddr, pAddr, Depth + 1, Flags);
}

volatile bool PageAvailable(PageMapTable* BasePte, uint64_t vAddr, int Depth)
{
    uint64_t Index = (vAddr & (0x1FFULL << ((3 - Depth) * 9 + 12))) >> ((3 - Depth) * 9 + 12);
    Index &= 0x1FF;
    
    if ((BasePte[Index].Data & (1ULL << PRESENT_BIT)) == 0) return true;
    if (Depth == 3) return false;
    return PageAvailable(BasePte[Index].Data & 0xFFFFFFFFF000, vAddr, Depth + 1);
}

volatile uint64_t GetPage(PageMapTable* BasePte, uint64_t vAddr, int Depth)
{
    uint64_t Index = (vAddr & (0x1FFULL << ((3 - Depth) * 9 + 12))) >> ((3 - Depth) * 9 + 12);
    Index &= 0x1FF;
    
    if (Depth == 3) 
    {
        if ((BasePte[Index].Data & (1ULL << PRESENT_BIT)) == 0) return BasePte[Index].Data;
        return 0;   
    }
    return PageAvailable(BasePte[Index].Data & 0xFFFFFFFFF000, vAddr, Depth + 1);
}

volatile void FreePage(PageMapTable* BasePte, uint64_t vAddr, int Depth)
{
    uint64_t Index = ((uint64_t)vAddr & (0x1FFULL << ((3 - Depth) * 9 + 12))) >> ((3 - Depth) * 9 + 12);
    Index &= 0x1FF;
    if (Depth == 3)
    {
        if (BasePte[Index].Data & (1ULL << PRESENT_BIT)) return;
        BasePte[Index].Data = 0;
        return;
    }
    if ((BasePte[Index].Data & (1ULL << PRESENT_BIT)) == 0) 
    {
        return;
    }
    FreePage(BasePte[Index].Data & 0xFFFFFFFFF000, vAddr, Depth + 1);
    PageMapTable* Check = BasePte[Index].Data & 0xFFFFFFFFF000;
    for (int i = 0;i < PMT_SIZE;i++)
    {
        if (Check[i].Data) return;
    }
    BasePte[Index].Data = 0;
}

static inline void Flush() 
{    
   asm volatile("mov $0x10000000, %rax\nmov %rax, %cr3");
}

volatile bool AllocIdMap(uint64_t Addr, uint64_t Size, uint64_t Flags)
{
    // FIXME: If the size is the factor of 0x1000 it will allocate an extra page
    for (uint64_t i = 0;i <= Size / 0x1000;i++)
    {
        if (!AllocPage(Tier4, Addr + i * 0x1000, Addr + i * 0x1000, 0, Flags)) return false;
    }
    Flush();
    return true;
}

volatile bool AllocMap(uint64_t vAddr, uint64_t pAddr, uint64_t Size, uint64_t Flags)
{
    // FIXME: If the size is the factor of 0x1000 it will allocate an extra page
    for (uint64_t i = 0;i <= Size / 0x1000;i++)
    {
        if (!AllocPage(Tier4, vAddr + i * 0x1000, pAddr + i * 0x1000, 0, Flags)) return false;
    }
    Flush();
    return true;
}

volatile void AllocUnMap(uint64_t vAddr, uint64_t Size)
{
    // FIXME: If the size is the factor of 0x1000 it will free an extra page
    for (uint64_t i = 0;i <= Size / 0x1000;i++)
    {
        FreePage(Tier4, vAddr + i * 0x1000, 0);
    }
    Flush();
}

#define PHYS_TAKEN_SIZE (0x80000000 / 0x1000)
#define PHYS_TAKEN_START 0x20000000

static uint8_t PhysTaken[PHYS_TAKEN_SIZE]; // 4 KiB (Page) Blocks
static size_t PhysTakenCache = 0;
static size_t VmCache = 0;

volatile uint64_t AllocVM(uint64_t Size)
{
    uint64_t MemMapSize = *(uint64_t*)0x7E01;
    uint64_t* MemMap = 0x7E05;

    uint64_t PAddr = 0;
    uint64_t PAddrI = 0;

    if (Size < 0x1000) Size = 0x1000;
    Size /= 0x1000;

    uint64_t StoreSize = Size;
    if (StoreSize > 0b1111111111ULL) StoreSize = (uint64_t)0b1111111111ULL;

    uint64_t Size0 = (StoreSize & (uint64_t)0b111) << 9;
    uint64_t Size1 = ((StoreSize & (uint64_t)0b1111111000) >> 3) << 52;

    for (uint64_t i = PhysTakenCache;i < PHYS_TAKEN_SIZE;i++)
    {
        uint64_t Con = i * 0x1000 + PHYS_TAKEN_START;
        for (uint64_t j = 0;j < Size;j++)
        {
            if (PhysTaken[i++]) 
            {
                i--;
                goto NextAttemptAllocVM;
            }
        }
        for (uint64_t k = 0;k < MemMapSize;k++)
        {
            if (MemMap[k * 2 + 0] < Con && MemMap[k * 2 + 0] + MemMap[k * 2 + 1] > Con + Size * 0x1000) goto FoundAllocVM;
        }
        continue;
        FoundAllocVM:

        PAddr = Con;
        PAddrI = i;
        break;

        NextAttemptAllocVM:
    }
    if (PAddr == 0) return 0;

    PhysTakenCache = PAddrI;

    for (uint64_t i = VmCache ? VmCache : 0x10000000;i < 0x80000000;i += 0x1000)
    {
        uint64_t Con = i;
        for (uint64_t j = 0;j < Size;j++)
        {
            if (!PageAvailable(Tier4, i, 0)) goto NextVAttemptAllocVM;
            i += 0x1000;
        }
        i = Con;
        for (uint64_t j = 0;j < Size;j++)
        {
            if (j == 0)
            {
                if (!AllocPage(Tier4, i, PAddr, 0, (1ULL << READWRITE_BIT) | (1ULL << USER_SUPERVISOR_BIT) | Size0 | Size1)) return 0;
            }
            else
            {
                if (!AllocPage(Tier4, i, PAddr, 0, (1ULL << READWRITE_BIT) | (1ULL << USER_SUPERVISOR_BIT))) return 0;
            }
            PhysTaken[(PAddr - PHYS_TAKEN_START) / 0x1000] = 1;
            i += 0x1000;
            PAddr += 0x1000;
        }

        Flush();

        VmCache = Con + Size * 0x1000;
        return Con;

        NextVAttemptAllocVM:
    }

    return 0;
}

volatile uint64_t AllocPhys(uint64_t Size)
{
    uint64_t MemMapSize = *(uint64_t*)0x7E01;
    uint64_t* MemMap = 0x7E05;

    uint64_t PAddr = 0;
    uint64_t PAddrI = 0;

    if (Size < 0x1000) Size = 0x1000;
    Size /= 0x1000;

    uint64_t StoreSize = Size;
    if (StoreSize > 0b1111111111ULL) StoreSize = (uint64_t)0b1111111111ULL;

    uint64_t Size0 = (StoreSize & (uint64_t)0b111) << 9;
    uint64_t Size1 = ((StoreSize & (uint64_t)0b1111111000) >> 3) << 52;

    for (uint64_t i = 0;i < PHYS_TAKEN_SIZE;i++)
    {
        uint64_t Con = i * 0x1000 + PHYS_TAKEN_START;
        for (uint64_t j = 0;j < Size;j++)
        {
            if (PhysTaken[i++]) 
            {
                i--; 
                goto NextAttemptAllocPhys;
            }
        }
        for (uint64_t k = 0;k < MemMapSize;k++)
        {
            if (MemMap[k * 2 + 0] < Con && MemMap[k * 2 + 0] + MemMap[k * 2 + 1] > Con + Size * 0x1000) goto FoundAllocPhys;
        }
        continue;
        FoundAllocPhys:

        PAddr = Con;
        PAddrI = i;
        break;

        NextAttemptAllocPhys:
    }
    if (PAddr == 0) return 0;

    uint64_t Con = PAddr;
    for (uint64_t j = 0;j < Size;j++)
    {
        if (j == 0)
        {
            if (!AllocPage(Tier4, Con + j * 0x1000, PAddr, 0, (1ULL << READWRITE_BIT) | (1ULL << USER_SUPERVISOR_BIT) | (1ULL << CACHE_DISABLE_BIT) | Size0 | Size1)) return 0;
        }
        else
        {
            if (!AllocPage(Tier4, Con + j * 0x1000, PAddr, 0, (1ULL << READWRITE_BIT) | (1ULL << USER_SUPERVISOR_BIT) | (1ULL << CACHE_DISABLE_BIT))) return 0;
        }
        PhysTaken[(PAddr - PHYS_TAKEN_START) / 0x1000] = 1;
        PAddr += 0x1000;
    }

    Flush();

    return Con;
}

volatile uint64_t AllocVMAtPhys(uint64_t pAddr, uint64_t Size)
{
    uint64_t PAddr = pAddr;
    uint64_t PAddrI = (PAddr - PHYS_TAKEN_START) / 0x1000;

    if (Size < 0x1000) Size = 0x1000;
    Size /= 0x1000;

    uint64_t StoreSize = Size;
    if (StoreSize > 0b1111111111ULL) StoreSize = (uint64_t)0b1111111111ULL;

    uint64_t Size0 = (StoreSize & (uint64_t)0b111) << 9;
    uint64_t Size1 = ((StoreSize & (uint64_t)0b1111111000) >> 3) << 52;

    for (uint64_t i = 0x10000000;i < 0x80000000;i += 0x1000)
    {
        uint64_t Con = i;
        for (uint64_t j = 0;j < Size;j++)
        {
            if (!PageAvailable(Tier4, i, 0)) goto NextVAttemptAllocVM;
            i += 0x1000;
        }
        i = Con;
        for (uint64_t j = 0;j < Size;j++)
        {
            if (j == 0)
            {
                if (!AllocPage(Tier4, i, PAddr, 0, (1ULL << READWRITE_BIT) | (1ULL << CACHE_DISABLE_BIT) | Size0 | Size1)) return 0;
            }
            else
            {
                if (!AllocPage(Tier4, i, PAddr, 0, (1ULL << READWRITE_BIT) | (1ULL << CACHE_DISABLE_BIT))) return 0;
            }
            i += 0x1000;
            PAddr += 0x1000;
        }

        Flush();

        return Con;

        NextVAttemptAllocVM:
    }

    return 0;
}

volatile void FreeVM(uint64_t vAddr)
{
    uint64_t Page = GetPage(Tier4, vAddr, 0);
    uint64_t Size0 = (Page >> 9) & 0b111;
    uint64_t Size1 = (Page >> 52) & 0b1111111;
    uint64_t PagePhys = Page >> 12;
    PagePhys &= 0xFFFFFULL;
    PagePhys *= 0x1000;

    if (vAddr < VmCache) VmCache = vAddr;
    PhysTakenCache = 0;

    if (Size0 == 0 && Size1 == 0) return;

    uint64_t Size = (Size1 << 3) | Size0;

    int PAddrI = (PagePhys - PHYS_TAKEN_START) / 0x1000;
    for (int i = 0;i < Size;i++)
    {
        FreePage(Tier4, vAddr + i * 0x1000, 0);
        PhysTaken[PAddrI++] = 0;
    }
}

volatile uint64_t AllocVMAt(uint64_t vAddr, uint64_t Size)
{
    uint64_t MemMapSize = *(uint64_t*)0x7E01;
    uint64_t* MemMap = 0x7E05;

    uint64_t PAddr = vAddr;

    for (uint64_t j = vAddr;j < vAddr + Size;j += 0x1000)
    {
        AllocPage(Tier4, j, PAddr, 0, (1ULL << READWRITE_BIT) | (1ULL << USER_SUPERVISOR_BIT));
        PAddr += 0x1000;
    }
    return vAddr;
}

volatile uint64_t AllocVMAtFlags(uint64_t vAddr, uint64_t Size, uint64_t Flags)
{
    uint64_t MemMapSize = *(uint64_t*)0x7E01;
    uint64_t* MemMap = 0x7E05;

    uint64_t PAddr = vAddr;

    for (uint64_t j = vAddr;j < vAddr + Size;j += 0x1000)
    {
        AllocPage(Tier4, j, PAddr, 0, Flags);
        PAddr += 0x1000;
    }
    return vAddr;
}

volatile uint64_t AllocVMAtStack(uint64_t vAddr, uint64_t Size)
{
    uint64_t MemMapSize = *(uint64_t*)0x7E01;
    uint64_t* MemMap = 0x7E05;

    uint64_t PAddr = vAddr;

    for (uint64_t j = vAddr;j < vAddr + Size;j += 0x1000)
    {
        AllocPage(Tier4, j, PAddr, 0, (1ULL << READWRITE_BIT) | (1ULL << USER_SUPERVISOR_BIT));
        PAddr += 0x1000;
    }
    return vAddr;
}

volatile void AllocInit()
{
    AllocOffset = *(uint32_t*)0x7FF0;
    Tier4 = 0x10000000;

    VmCache = 0;
    PhysTakenCache = 0;

    for (int i = 0;i < PHYS_TAKEN_SIZE;i++)
    {
        PhysTaken[i] = 0;
    }
}