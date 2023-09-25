#ifndef NVME_H
#define NVME_H

#include "../../../include.h"

void NVMEInit();
bool NVMERead(size_t Num, uint32_t LBA, void* Dest);
bool NVMEWrite(size_t Num, uint32_t LBA, void* Src);

#endif // NVME_H