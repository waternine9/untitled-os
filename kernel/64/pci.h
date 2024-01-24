#ifndef PCI_H
#define PCI_H

#include <stdint.h>

typedef struct {
    uint8_t Bus;        // Which bus device is on
    uint8_t Device;     // Which device
    uint8_t Function;   // Which function of the device
} pci_device_path;

typedef enum {
    PCI_GENERAL_DEVICE,
    PCI_TO_PCI_BRIDGE,
    PCI_TO_CARDBUS_BRIDGE
} pci_header_type;

typedef enum {
    PCI_DEVICE_UNKNOWN,
    PCI_DEVICE_VGA,
    PCI_DEVICE_ETHERNET,
    PCI_DEVICE_IDE,
    PCI_DEVICE_EHCI,
    PCI_DEVICE_XHCI,
    PCI_DEVICE_HOST_BRIDGE,
    PCI_DEVICE_ISA_BRIDGE,
    PCI_DEVICE_NVME
} pci_device_specialty;

typedef struct {
    pci_device_path  Path;
    uint16_t         VendorId;
    uint16_t         DeviceId;
    uint16_t         Command;
    uint16_t         Status;
    uint8_t          RevisionId;
    uint8_t          Interface;
    uint8_t          Subclass;
    uint8_t          Class;
    uint8_t          CacheLineSize;
    uint8_t          LatencyTimer;
    pci_header_type  HeaderType;
    uint8_t          BIST;      
    uint32_t         BAR0;
    uint32_t         BAR1;
    uint8_t          MultiFunction;
} pci_device_header;

typedef struct
{
    uint32_t AddressLow;
    uint32_t AddressHigh;
    uint32_t Data;
    uint32_t Mask;
} __attribute__((packed)) MSIX_VectorControl;

uint32_t PCI_PathToObject(pci_device_path Path, uint8_t RegOffset);
uint32_t PCI_Read32(pci_device_path Path, uint8_t RegOffset);
uint16_t PCI_Read16(pci_device_path Path, uint8_t RegOffset);
uint8_t PCI_Read8(pci_device_path Path, uint8_t RegOffset);
void PCI_Write32(pci_device_path Path, uint8_t RegOffset, uint32_t Value32);
void PCI_Write16(pci_device_path Path, uint8_t RegOffset, uint16_t Value16);
void PCI_Write8(pci_device_path Path, uint8_t RegOffset, uint8_t Value8);
pci_device_header PCI_QueryDeviceHeader(pci_device_path Path);
pci_device_specialty PCI_QueryDeviceSpecialty(pci_device_header Header);

#endif // PCI_H