#include "xhci.h"
#include "../../pci.h"
#include "../../vmalloc.h"
#include "../../idt.h"
#include "../../io.h"
#include "../../iomem.h"
#include "../../apic.h"
#include "../../sleep.h"

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
    uint32_t IST : 4;
    uint32_t ERSTMax : 4;
    uint32_t Reserved2 : 13;
    uint32_t MaxScratchpadBufsHi : 5;
    uint32_t SPR : 1;
    uint32_t MaxScratchpadBufsLo : 5;
    uint32_t U1DeviceExitLatency : 8;
    uint32_t Reserved3 : 8;
    uint32_t U2DeviceExitLatency : 16;
    uint32_t AC64 : 1;
    uint32_t BNC : 1;
    uint32_t CSZ : 1;
    uint32_t PPC : 1;
    uint32_t PIND : 1;
    uint32_t LHRC : 1;
    uint32_t LTC : 1;
    uint32_t NSS : 1;
    uint32_t PAE : 1;
    uint32_t SPC : 1;
    uint32_t SEC : 1;
    uint32_t CFC : 1;
    uint32_t MaxPSASize : 4;
    uint32_t XECP : 16; 
    uint32_t Reserved4 : 2;
    uint32_t DBOffs : 30;
    uint32_t Reserved5 : 5;
    uint32_t RRSOffs : 27;
    uint32_t U3C : 1;
    uint32_t CMC : 1;
    uint32_t FSC : 1;
    uint32_t CTC : 1;
    uint32_t LEC : 1;
    uint32_t CIC : 1;
    uint32_t TC : 1;
    uint32_t ETC_TIC : 1;
    uint32_t GIC : 1;
    uint32_t VTC : 1;
    uint32_t Reserved6 : 22;
} __attribute__((packed)) xHC_CapRegisters;

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
    uint32_t Reserved1 : 16;
    uint64_t Reserved2 : 6;
    uint64_t ERSTBaseAddress : 58;
    uint64_t DequeueERSTSegmentIdx : 3;
    uint64_t EventHandlerBusy : 1;
    uint64_t EventRingDequeuePtr : 60;
} __attribute__((packed)) xHC_InterrupterRegisters;

typedef struct 
{
    // 4 bytes (first dword)
    uint32_t CCS : 1;
    uint32_t PED : 1;
    uint32_t Reserved0 : 1;
    uint32_t OCA : 1;
    uint32_t PortReset : 1;
    uint32_t PLS : 4;
    uint32_t PortPower : 1;
    uint32_t PortSpeed : 4;
    uint32_t PortIndicatorControl : 2;
    uint32_t LWS : 1;
    uint32_t CSC : 1;
    uint32_t PEC : 1;
    uint32_t WRC : 1;
    uint32_t OCC : 1;
    uint32_t PRC : 1;
    uint32_t PLC : 1;
    uint32_t CEC : 1;
    uint32_t CAS : 1;
    uint32_t WCE : 1;
    uint32_t WDE : 1;
    uint32_t WOE : 1;
    uint32_t Reserved1 : 2; 
    uint32_t DeviceRemovable : 1;
    uint32_t WPR : 1;

    // 4 bytes (second dword)
    uint32_t U1Timeout : 8;
    uint32_t U2Timeout : 8;
    uint32_t FLA : 1;
    uint32_t Reserved2: 15;
    
    // 4 bytes (third dword)
    uint32_t L1S : 3;
    uint32_t RWE : 1;
    uint32_t BESL : 4;
    uint32_t L1DeviceSlot : 8;
    uint32_t HLE : 1;
    uint32_t Reserved3 : 11;

    // 4 bytes (fourth dword)
    uint32_t PortTestControl : 4;
    uint32_t LinkErrorCount: 16;
    uint32_t RLC : 4;
    uint32_t TLC : 4;
    uint32_t Reserved4 : 8;
} __attribute__((packed)) xHC_PortRegisters;

typedef struct
{
    xHC_PortRegisters* PortRegs;
    xHC_CapRegisters* CapRegs;
    xHC_OpRegisters* OpRegs;
    uint64_t RunRegsBase;
    xHC_InterrupterRegisters* IntRegs;

    uint32_t MaxSlots;

    DeviceContext** DCBAA;
    CommandTRB* CommandRing;
    ERSTEntry* ERST[256];
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
        xhc.CapRegs = ((uint64_t)Header.BAR1 << 32) | ((uint64_t)Header.BAR0 & (~0b1111ULL));
        xhc.CapRegs = AllocVMAtFlags(xhc.CapRegs, 0x1000, MALLOC_READWRITE_BIT | MALLOC_CACHE_DISABLE_BIT | MALLOC_USER_SUPERVISOR_BIT);
        xhc.OpRegs = (uint8_t*)xhc.CapRegs + xhc.CapRegs->CapLength;


        USBCMD usbcmd;
        USBSTS usbsts;
        IOMEM_READ_VALUE(&usbsts, &xhc.OpRegs->UsbSts);
        IOMEM_READ_VALUE(&usbcmd, &xhc.OpRegs->UsbCmd);

        usbcmd.HCReset = 1;
        IOMEM_WRITE_VALUE(&xhc.OpRegs->UsbCmd, &usbcmd);

        bool ready = 0;
        for (int i = 0;i < 100;i++)
        {
            Sleep(10);

            IOMEM_READ_VALUE(&usbcmd, &xhc.OpRegs->UsbCmd);
            IOMEM_READ_VALUE(&usbsts, &xhc.OpRegs->UsbSts);

            ready = usbcmd.HCReset == 0 && usbsts.CNR == 0;
            if (ready) break;
        }

        if (ready)
        {
            // If it hit this, then the controller was successfully reset
            uint16_t hci_version;
            IOMEM_READ_VALUE(&hci_version, &xhc.CapRegs->HciVersion);
            //asm volatile("cli\nhlt" :: "a"( hci_version ));
        }
        else
        {
            // If it hit this, then the controller was not successfully reset
            //asm volatile("cli\nhlt" :: "a"( *(uint32_t*)&usbcmd ));
        }

        
        while (xhc.OpRegs->UsbSts.CNR);
        xhc.MaxSlots = xhc.CapRegs->MaxDeviceSlots;
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
        
        xhc.RunRegsBase = (uint64_t)xhc.CapRegs + (uint64_t)xhc.CapRegs->RRSOffs;
        xhc.IntRegs = xhc.RunRegsBase + 0x20;
        
        size_t ActiveInterruptors = xhc.CapRegs->MaxInterrupters;
        
        for (size_t i = 0;i < ActiveInterruptors; i++)
        {
            xhc.ERST[i] = AllocPhys(ERST_SIZE * sizeof(ERSTEntry));
            for (int j = 0;j < ERST_SIZE;j++)
            {
                xhc.ERST[i][j].RingSegmentBaseAddress = AllocPhys(sizeof(NormalTRB) * EVENT_RING_SIZE) >> 6;
                xhc.ERST[i][j].RingSegmentSize = EVENT_RING_SIZE;
            }

            xhc.IntRegs[i].ERSTSize = ERST_SIZE;
            xhc.IntRegs[i].EventRingDequeuePtr = (uint64_t)xhc.ERST[i][0].RingSegmentBaseAddress >> 4;
            xhc.IntRegs[i].ERSTBaseAddress = (uint64_t)xhc.ERST[i] >> 6;
            xhc.IntRegs[i].IntModInterval = 4000;
            xhc.IntRegs[i].IntEnable = 1;
        }

        xhc.PortRegs = (uint8_t*)xhc.OpRegs + 0x400;

        xhc.OpRegs->UsbCmd.IntEnable = 1;
        xhc.OpRegs->UsbCmd.RunStop = 1;

        xHCI_Controllers[xHCI_NumControllers++] = xhc;
    }
}


static size_t NumPortsEnabled(xHC *xhc)
{
    size_t NumPorts = 0;

    for (int i = 0;i < xhc->CapRegs->MaxDeviceSlots;i++)
    {
        for (int j = 0;j < 1000;j++)
        {
            if (xhc->PortRegs[i].CCS) break;
        }

        if (xhc->PortRegs[i].CCS) NumPorts++;
    }
    return NumPorts;
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
    asm volatile("cli\nhlt" :: "a"(NumPortsEnabled(&xHCI_Controllers[0])));
}