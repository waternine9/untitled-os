#ifndef APIC_H
#define APIC_H

#include "../include.h"

typedef struct {
  char Signature[8];
  uint8_t Checksum;
  char OEMID[6];
  uint8_t Revision;
  uint32_t Rsdt;

  uint32_t Length;
  uint64_t Xsdt;
  uint8_t ExtendedChecksum;
  uint8_t reserved[3];
} __attribute__((packed)) rsdp_descriptor;

void ApicInit();
void ApicEOI();
uint32_t GetApicBase();

#endif // APIC_H