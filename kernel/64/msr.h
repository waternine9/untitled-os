#ifndef MSR_H
#define MSR_H

#include "../include.h"

void GetMSR(uint32_t msr, uint32_t *lo, uint32_t *hi);
void SetMSR(uint32_t msr, uint32_t lo, uint32_t hi);

#endif // MSR_H