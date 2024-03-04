#ifndef NVME_H
#define NVME_H

#include "../../../include.h"
#include "../driverman.h"

typedef struct
{
    bool Sent;
    bool Completed;
} NVME_CmdStatus;

DriverMan_StorageDriver* NVMe_GetDriver();

#endif // NVME_H