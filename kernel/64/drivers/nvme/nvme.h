#ifndef NVME_H
#define NVME_H

#include "../../../include.h"

typedef struct
{
    bool Sent;
    bool Completed;
} NVME_CmdStatus;

void NVMEInit();
bool NVMERead(size_t Num, uint32_t LBA, void* Dest);
bool NVMEWrite(size_t Num, uint32_t LBA, void* Src);

#endif // NVME_H