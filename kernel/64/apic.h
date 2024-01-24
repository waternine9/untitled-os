#ifndef APIC_H
#define APIC_H

#include "../include.h"

void ApicInit();
void ApicEOI();
uint32_t GetApicBase();

#endif // APIC_H