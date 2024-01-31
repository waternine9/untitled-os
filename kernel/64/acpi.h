#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

typedef struct {
  char Signature[4];
  uint32_t Length;
  uint8_t Revision;
  uint8_t Checksum;
  char OEMID[6];
  char OEMTableID[8];
  uint32_t OEMRevision;
  uint32_t CreatorID;
  uint32_t CreatorRevision;
} __attribute__((packed)) ACPI_SDTHeader;

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
} __attribute__((packed)) RSDP_Descriptor;



void ACPIInit();
void *ACPIFindTable(char name[4]);

#endif