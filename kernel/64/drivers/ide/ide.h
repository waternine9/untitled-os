#ifndef IDE_H
#define IDE_H

#include "stdint.h"

uint8_t IDEInit();
void IDEReadBoot(uint64_t LBA, void* Out);

#endif // IDE_H