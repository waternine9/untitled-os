#include "xhci.h"
#include "../../pci.h"
#include "../../vmalloc.h"
#include "../../idt.h"
#include "../../io.h"
#include "../../apic.h"

#define MAX_CONTROLLERS 256
#define COMMAND_RING_SIZE 64
#define EVENT_RING_SIZE 32
#define ERST_SIZE 16

typedef struct
{
    uint8_t CapLength;
    uint8_t Reserved0;
    uint16_t HciVersion;
    uint32_t MaxDeviceSlots : 8;
    uint32_t MaxInterrupters : 11;
    uint32_t Reserved1 : 5;
    uint32_t MaxPorts : 8;
    uint32_t HcsParams1;
    uint32_t HcsParams2;
    uint32_t HcsParams3;
    uint32_t HccParams1;
    uint32_t DbOffset;
    uint32_t RtsOff;
    uint32_t HccParams2;
} xHC_CapRegisters;

typedef struct
{
    uint32_t RunStop : 1;
    uint32_t HCReset : 1;
    uint32_t IntEnable : 1;
    uint32_t HSErrorEnable : 1;
    uint32_t Reserved0 : 3;
    uint32_t LHCReset : 1;
    uint32_t CSS : 1;
    uint32_t CRS : 1;
    uint32_t EWE : 1;
    uint32_t EU3Stop : 1;
    uint32_t Reserved1 : 1;
    uint32_t CMEnable : 1;
    uint32_t ETEnable : 1;
    uint32_t TSCEnable : 1;
    uint32_t VTIOEnable : 1;
    uint32_t Reserved2 : 15;
} USBCMD;

typedef struct
{
    uint32_t HCHalted : 1;
    uint32_t Reserved0 : 1;
    uint32_t HSError : 1;
    uint32_t EInt : 1;
    uint32_t PCDetect : 1;
    uint32_t Reserved1 : 3;
    uint32_t SSStatus : 1;
    uint32_t RSStatus : 1;
    uint32_t SRError : 1;
    uint32_t CNR : 1;
    uint32_t HCE : 1;
    uint32_t Reserved2 : 19;
} USBSTS;

typedef struct
{
    uint32_t RouteString : 19;
    uint32_t Speed : 4;
    uint32_t Reserved0 : 1;
    uint32_t MultiTT : 1;
    uint32_t Hub : 1;
    uint32_t ContextEntries : 5;
    uint32_t MaxExitLatency : 16;
    uint32_t RootHubPortNumber : 8;
    uint32_t NumberOfPorts : 8;
    uint32_t ParentHubSlotID : 8;
    uint32_t ParentPortNumber : 8;
    uint32_t TTThinkTime : 2;
    uint32_t Reserved1 : 4;
    uint32_t InterrupterTarget : 10;
    uint32_t USBDeviceAddress : 8;
    uint32_t Reserved2 : 19;
    uint32_t SlotState : 5;
    uint64_t Reserved3;
    uint64_t Reserved4;
} __attribute__((packed)) SlotContext;

typedef struct
{
    uint32_t EPState : 3;
    uint32_t Reserved0 : 5;
    uint32_t Mult : 2;
    uint32_t MaxPStreams : 5;
    uint32_t LSA : 1;
    uint32_t Interval : 8;
    uint32_t MaxESITPayloadHi : 8;
    uint32_t Reserved1 : 1;
    uint32_t ErrorCount : 2;
    uint32_t EPType : 3;
    uint32_t Reserved2 : 1;
    uint32_t HID : 1;
    uint32_t MaxBurstSize : 8;
    uint32_t MaxPacketSize : 16;
    uint32_t DCS : 1;
    uint32_t Reserved3 : 3;
    uint64_t TRDequeuePtr : 60; // Make sure to shift the actual pointer to the right by 4
    uint32_t MaxESITPayloadLo : 16;
    uint32_t Reserved4;
    uint64_t Reserved5;
} __attribute__((packed)) EndpointContext;

typedef struct
{
    uint64_t DataPtr;
    uint32_t TRBTransferLength : 17;
    uint32_t TDSize : 5;
    uint32_t InterrupterTarget : 10;
    uint32_t Cycle : 1;
    uint32_t ENT : 1;
    uint32_t ISP : 1;
    uint32_t NoSnoop : 1;
    uint32_t ChainBit : 1;
    uint32_t IOC : 1;
    uint32_t IDT : 1;
    uint32_t Reserved0 : 2;
    uint32_t BEI : 1;
    uint32_t TRBType : 6;
    uint32_t Reserved1 : 16;
} __attribute__((packed)) NormalTRB;

typedef uint32_t CommandTRB[4];

typedef struct
{
    uint32_t Reserved0 : 6;
    uint64_t RingSegmentBaseAddress : 58;
    uint32_t RingSegmentSize : 16;
    uint64_t Reserved1 : 48;
} __attribute__((packed)) ERSTEntry; // Event Ring Segment Table Entry

typedef struct

typedef struct
{
    SlotContext SlotCtx;
    EndpointContext EndpointCtx[31];
} __attribute__((packed)) DeviceContext;

typedef struct
{
    USBCMD UsbCmd;
    USBSTS UsbSts;
    uint32_t PageSize;
    uint32_t DNControl;
    uint64_t CRControl;
    uint64_t DCBAAP;
    uint64_t Config;
} xHC_OpRegisters;

typedef struct
{
    uint32_t IntPending : 1;
    uint32_t IntEnable : 1;
    uint32_t Reserved0 : 30;
    uint32_t IntModInterval : 16;
    uint32_t IntModCounter : 16;
    uint32_t ERSTSize : 16;
    uint32_t Reserved1 : 22;
    uint64_t ERSTBaseAddress : 58;
    uint32_t DequeueERSTSegmentIdx : 3;
    uint32_t EventHandlerBusy : 1;
    uint64_t EventRingDequeuePtr : 60;
} __attribute__((packed)) xHC_InterrupterRegisters;

typedef struct
{
    xHC_CapRegisters* CapRegs;
    xHC_OpRegisters* OpRegs;
    uint64_t RunRegsBase;
    xHC_InterrupterRegisters* IntRegs;

    uint32_t MaxSlots;

    DeviceContext** DCBAA;
    CommandTRB* CommandRing;
    ERSTEntry* ERST;
} xHC;

static xHC xHCI_Controllers[MAX_CONTROLLERS];
static int xHCI_NumControllers;

void CheckXHC(pci_device_path Path)
{
    pci_device_header Header = PCI_QueryDeviceHeader(Path);
    pci_device_specialty Specialty = PCI_QueryDeviceSpecialty(Header);
    if (Specialty == PCI_DEVICE_XHCI)
    {
        xHC xhc;
        xhc.CapRegs = ((uint64_t)Header.BAR1 << 32) | ((uint64_t)Header.BAR0);
        xhc.CapRegs = AllocVMAtFlags(xhc.CapRegs, 0x10000, MALLOC_READWRITE_BIT | MALLOC_CACHE_DISABLE_BIT | MALLOC_USER_SUPERVISOR_BIT);
        xhc.OpRegs = (uint8_t*)xhc.CapRegs + xhc.CapRegs->CapLength;
        while (xhc.OpRegs->UsbSts.CNR);
        xhc.MaxSlots = xhc.CapRegs->HccParams1 & 0xFF;
        xhc.OpRegs->Config |= xhc.MaxSlots;
        xhc.DCBAA = AllocPhys(sizeof(DeviceContext*) * xhc.MaxSlots);
        for (int i = 0;i < xhc.MaxSlots;i++)
        {
            xhc.DCBAA[i] = AllocPhys(sizeof(DeviceContext));
        }
        xhc.OpRegs->DCBAAP |= (uint64_t)xhc.DCBAA & (~0b11111ULL);
        xhc.CommandRing = AllocPhys(sizeof(CommandTRB) * COMMAND_RING_SIZE); 
        xhc.OpRegs->CRControl = (uint64_t)xhc.CommandRing & (~0b111111ULL);
        
        uint8_t CapPointer = PCI_Read8(Path, 0x34) & (~0b11U);
        while (CapPointer != 0)
        {
            // Check for MSI-X capability
            if (PCI_Read8(Path, CapPointer) == 0x11)
            {
                uint16_t msixControl = PCI_Read16(Path, CapPointer + 0x02);
                uint32_t tableOffset = PCI_Read32(Path, CapPointer + 0x04) & ~0b111U;
                uint8_t barIndex = PCI_Read32(Path, CapPointer + 0x04) & 0b111U;

                uint64_t msixTableBase0 = (uint32_t)PCI_Read32(Path, 0x10 + barIndex * 4);
                if ((msixTableBase0 & 0b110) == 0b100)
                {
                    uint64_t msixTableBase1 = (uint32_t)PCI_Read32(Path, 0x14 + barIndex * 4);
                    msixTableBase0 |= msixTableBase1 << 32;
                }
                msixTableBase0 += tableOffset;
                msixTableBase0 &= ~0xFULL;
                
                
                MSIX_VectorControl* msixTableBase = msixTableBase0;
                AllocIdMap(msixTableBase0, 0x1000, MALLOC_READWRITE_BIT | MALLOC_CACHE_DISABLE_BIT | MALLOC_USER_SUPERVISOR_BIT);
                msixTableBase[0].AddressLow = GetApicBase() & (~0xFULL);
                msixTableBase[0].AddressHigh = 0;
                msixTableBase[0].Data = 0x71;
                msixTableBase[0].Mask = 0;
                AllocUnMap(msixTableBase0, 0x1000);

                // Enable MSI-X
                msixControl |= 1ULL << 15; // Set the MSI-X enable bit
                //*(uint32_t*)0xFFFFFFFF9000000E = 0xFFFFFFFF;
                PCI_Write16(Path, CapPointer + 0x02, msixControl);
                break;
            }
            CapPointer = PCI_Read8(Path, CapPointer + 0x1);
        }
        xhc.ERST = AllocPhys(ERST_SIZE * sizeof(ERSTEntry));
        for (int i = 0;i < ERST_SIZE;i++)
        {
            xhc.ERST[i].RingSegmentBaseAddress = AllocPhys(sizeof(NormalTRB) * EVENT_RING_SIZE) >> 6;
            xhc.ERST[i].RingSegmentSize = EVENT_RING_SIZE;
        }
        xhc.RunRegsBase = (uint64_t)xhc.CapRegs + xhc.CapRegs->RtsOff & (~0x1FULL);
        xhc.IntRegs = xhc.RunRegsBase + 0x20;

        xhc.IntRegs[0].ERSTSize = ERST_SIZE;
        xhc.IntRegs[0].EventRingDequeuePtr = (uint64_t)xhc.ERST >> 4;
    }
}

// Also initializes Host Controllers
void ScanForXHC()
{
    for (int Bus = 0;Bus < 256;Bus++)
    {
        for (int Device = 0;Device < 32;Device++)
        {
            pci_device_path Path;
            Path.Bus = Bus;
            Path.Device = Device;
            Path.Function = 0;
            pci_device_header Header = PCI_QueryDeviceHeader(Path);
            if (Header.VendorId == 0xFFFF) continue;
            if (Header.MultiFunction)
            {
                for (int Function = 1;Function < 8;Function++)
                {
                    Path.Function = Function;
                    CheckXHC(Path);
                }
            }
            else
            {
                CheckXHC(Path);
            }
        }
    }
}

void XHCIInit()
{
    xHCI_NumControllers = 0;
    ScanForXHC();
}