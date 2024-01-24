#include "../include.h"

volatile bool IdMap(uint32_t Addr, uint32_t Size);
volatile bool Map(uint32_t vAddr, uint32_t pAddr, uint32_t Size);
volatile bool MapHigher(uint32_t vAddr, uint32_t pAddr, uint32_t Size);
volatile void Init();
volatile void StoreNumber();