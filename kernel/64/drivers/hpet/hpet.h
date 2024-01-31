#ifndef HPET_H
#define HPET_H

#ifndef __attribute__
#define __attribute__(x)
#endif
#include "../../acpi.h"
#include <stdbool.h>

typedef struct
{
    ACPI_SDTHeader header;

    uint8_t hardware_rev_id;
    uint8_t comparator_count:5;
    uint8_t counter_size:1;
    uint8_t reserved0:1;
    uint8_t legacy_replacement:1;
    uint16_t pci_vendor_id;
    uint8_t address_space_id;    // 0 - system memory, 1 - system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved1;
    uint64_t address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} HPET_Table __attribute__((packed));


bool HPETInit();

#endif