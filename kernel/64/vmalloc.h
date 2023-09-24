#ifndef VMALLOC_H
#define VMALLOC_H

#include "../include.h"

#define MALLOC_MAX_ADDR_BIT 47
#define MALLOC_PRESENT_BIT 0
#define MALLOC_READWRITE_BIT 1
#define MALLOC_USER_SUPERVISOR_BIT 2
#define MALLOC_WRITE_THROUGH_BIT 3
#define MALLOC_CACHE_DISABLE_BIT 4
#define MALLOC_ACCESSED_BIT 5
#define MALLOC_PAGE_SIZE_BIT 7
#define MALLOC_ADDR_BIT 12
#define MALLOC_AVAILABLE_BIT 52
#define MALLOC_NO_EXECUTE_BIT 63

volatile bool AllocIdMap(uint64_t Addr, uint64_t Size, uint64_t Flags);
volatile bool AllocMap(uint64_t vAddr, uint64_t pAddr, uint64_t Size, uint64_t Flags);
volatile void AllocUnMap(uint64_t vAddr, uint64_t Size);
volatile uint64_t AllocVM(uint64_t Size);
volatile uint64_t AllocPhys(uint64_t Size); // Must return 4 KiB aligned address for NVMe driver, etc
volatile void FreeVM(uint64_t vAddr);
volatile uint64_t AllocVMAt(uint64_t vAddr, uint64_t Size);
volatile uint64_t AllocVMAtPhys(uint64_t pAddr, uint64_t Size); // FOR DRIVERS
volatile uint64_t AllocVMAtStack(uint64_t vAddr, uint64_t Size);
volatile void AllocInit();

#endif VMALLOC_H