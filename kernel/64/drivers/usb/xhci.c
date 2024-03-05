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

// Register masks (little endian) | Mask       | Bit offset | 
#define XHCIM_CAPLENGTH             0xFF,        16
#define XHCIM_HCIVERSION            0xFFFF,      0
#define XHCIM_HCSPARAMS1            0xFF,        0
// TODO

typedef struct
{
    // uint8_t CapLength;
    // uint8_t Reserved0;
    // uint16_t HciVersion;

    // uint32_t MaxDeviceSlots : 8;
    // uint32_t MaxInterrupters : 11;
    // uint32_t Reserved1 : 5;
    // uint32_t MaxPorts : 8;
    
    // uint32_t IST : 4;
    // uint32_t ERSTMax : 4;
    // uint32_t Reserved2 : 13;
    // uint32_t MaxScratchpadBufsHi : 5;
    // uint32_t SPR : 1;
    // uint32_t MaxScratchpadBufsLo : 5;

    // uint32_t U1DeviceExitLatency : 8;
    // uint32_t U2DeviceExitLatency : 8;
    // uint32_t Reserved3 : 16;

    // uint32_t AC64 : 1;
    // uint32_t BNC : 1;
    // uint32_t CSZ : 1;
    // uint32_t PPC : 1;
    // uint32_t PIND : 1;
    // uint32_t LHRC : 1;
    // uint32_t LTC : 1;
    // uint32_t NSS : 1;
    // uint32_t PAE : 1;
    // uint32_t SPC : 1;
    // uint32_t SEC : 1;
    // uint32_t CFC : 1;
    // uint32_t MaxPSASize : 4;
    // uint32_t XECP : 16; 

    // uint32_t Reserved4 : 2;
    // uint32_t DBOffs : 30;

    // uint32_t Reserved5 : 5;
    // uint32_t RRSOffs : 27;

    // uint32_t U3C : 1;
    // uint32_t CMC : 1;
    // uint32_t FSC : 1;
    // uint32_t CTC : 1;
    // uint32_t LEC : 1;
    // uint32_t CIC : 1;
    // uint32_t ETC : 1;
    // uint32_t TSC : 1;
    // uint32_t GSC : 1;
    // uint32_t VTC : 1;
    // uint32_t Reserved6 : 22;
    uint32_t DWORD[8];
} __attribute__((packed)) xHC_CapRegisters;

typedef struct
{
    // uint32_t RunStop : 1;
    // uint32_t HCReset : 1;
    // uint32_t IntEnable : 1;
    // uint32_t HSErrorEnable : 1;
    // uint32_t Reserved0 : 3;
    // uint32_t LHCReset : 1;
    // uint32_t CSS : 1;
    // uint32_t CRS : 1;
    // uint32_t EWE : 1;
    // uint32_t EU3Stop : 1;
    // uint32_t Reserved1 : 1;
    // uint32_t CMEnable : 1;
    // uint32_t ETEnable : 1;
    // uint32_t TSCEnable : 1;
    // uint32_t VTIOEnable : 1;
    // uint32_t Reserved2 : 15;
    uint32_t DWORD[1];
} USBCMD;

typedef struct
{
    // uint32_t HCHalted : 1;
    // uint32_t Reserved0 : 1;
    // uint32_t HSError : 1;
    // uint32_t EInt : 1;
    // uint32_t PCDetect : 1;
    // uint32_t Reserved1 : 3;
    // uint32_t SSStatus : 1;
    // uint32_t RSStatus : 1;
    // uint32_t SRError : 1;
    // uint32_t CNR : 1;
    // uint32_t HCE : 1;
    // uint32_t Reserved2 : 19;
    uint32_t DWORD[1];
} USBSTS;

typedef struct
{
    // uint32_t RouteString : 20;
    // uint32_t Speed : 4;
    // uint32_t Reserved0 : 1;
    // uint32_t MTT : 1;
    // uint32_t Hub : 1;
    // uint32_t ContextEntries : 5;

    // uint32_t MaxExitLatency : 16;
    // uint32_t RootHubPortNumber : 8;
    // uint32_t NumberOfPorts : 8;

    // uint32_t ParentHubSlotID : 8;
    // uint32_t ParentPortNumber : 8;
    // uint32_t TTThinkTime : 2;
    // uint32_t Reserved1 : 4;
    // uint32_t InterrupterTarget : 10;

    // uint32_t USBDeviceAddress : 8;
    // uint32_t Reserved2 : 19;
    // uint32_t SlotState : 5;

    // uint64_t Reserved3;
    // uint64_t Reserved4;
    uint32_t DWORD[8];
} __attribute__((packed)) SlotContext;

typedef struct
{
    // uint32_t EPState : 3;
    // uint32_t Reserved0 : 5;
    // uint32_t Mult : 2;
    // uint32_t MaxPStreams : 5;
    // uint32_t LSA : 1;
    // uint32_t Interval : 8;
    // uint32_t MaxESITPayloadHi : 8;

    // uint32_t Reserved1 : 1;
    // uint32_t ErrorCount : 2;
    // uint32_t EPType : 3;
    // uint32_t Reserved2 : 1;
    // uint32_t HID : 1;
    // uint32_t MaxBurstSize : 8;
    // uint32_t MaxPacketSize : 16;

    // uint64_t DCS : 1;
    // uint64_t Reserved3 : 3;
    // uint64_t TRDequeuePtr : 60; // Make sure to shift the actual pointer to the right by 4

    // uint32_t AverageTRBLength : 16;
    // uint32_t MaxESITPayloadLo : 16;

    // uint32_t Reserved4;
    // uint64_t Reserved5;

    uint32_t DWORD[8];
} __attribute__((packed)) EndpointContext;

typedef struct
{
    // uint64_t DataPtr;
    
    // uint32_t TRBTransferLength : 17;
    // uint32_t TDSize : 5;
    // uint32_t InterrupterTarget : 10;

    // uint32_t Cycle : 1;
    // uint32_t ENT : 1;
    // uint32_t ISP : 1;
    // uint32_t NoSnoop : 1;
    // uint32_t ChainBit : 1;
    // uint32_t IOC : 1;
    // uint32_t IDT : 1;
    // uint32_t Reserved0 : 2;
    // uint32_t BEI : 1;
    // uint32_t TRBType : 6;
    // uint32_t Reserved1 : 16;

    uint32_t DWORD[4];
} __attribute__((packed)) NormalTRB;

typedef uint32_t CommandTRB[4];

typedef struct
{
    // uint64_t Reserved0 : 6;
    // uint64_t RingSegmentBaseAddress : 58;
    
    // uint64_t RingSegmentSize : 16;
    // uint64_t Reserved1 : 48;
    
    uint32_t DWORD[4];
} __attribute__((packed)) ERSTEntry; // Event Ring Segment Table Entry

typedef struct
{
    // uint32_t IntPending : 1;
    // uint32_t IntEnable : 1;
    // uint32_t Reserved0 : 30;

    // uint32_t IntModInterval : 16;
    // uint32_t IntModCounter : 16;
    
    // uint32_t ERSTSize : 16;
    // uint32_t Reserved1 : 16;
    
    // uint32_t Reserved2;

    // uint64_t Reserved3 : 6;
    // uint64_t ERSTBaseAddress : 58;
    
    // uint64_t DequeueERSTSegmentIdx : 3;
    // uint64_t EventHandlerBusy : 1;
    // uint64_t EventRingDequeuePtr : 60;

    uint32_t DWORD[8];    
} __attribute__((packed)) xHC_InterrupterRegisters;

typedef struct 
{
    // uint32_t CCS : 1;
    // uint32_t PED : 1;
    // uint32_t Reserved0 : 1;
    // uint32_t OCA : 1;
    // uint32_t PortReset : 1;
    // uint32_t PLS : 4;
    // uint32_t PortPower : 1;
    // uint32_t PortSpeed : 4;
    // uint32_t PortIndicatorControl : 2;
    // uint32_t LWS : 1;
    // uint32_t CSC : 1;
    // uint32_t PEC : 1;
    // uint32_t WRC : 1;
    // uint32_t OCC : 1;
    // uint32_t PRC : 1;
    // uint32_t PLC : 1;
    // uint32_t CEC : 1;
    // uint32_t CAS : 1;
    // uint32_t WCE : 1;
    // uint32_t WDE : 1;
    // uint32_t WOE : 1;
    // uint32_t Reserved1 : 2; 
    // uint32_t DeviceRemovable : 1;
    // uint32_t WPR : 1;

    // uint32_t U1Timeout : 8;
    // uint32_t U2Timeout : 8;
    // uint32_t FLA : 1;
    // uint32_t Reserved2: 15;
    
    // uint32_t L1S : 3;
    // uint32_t RWE : 1;
    // uint32_t BESL : 4;
    // uint32_t L1DeviceSlot : 8;
    // uint32_t HLE : 1;
    // uint32_t Reserved3 : 11;
    // uint32_t PortTestControl : 4;

    // uint32_t LinkErrorCount: 16;
    // uint32_t RLC : 4;
    // uint32_t TLC : 4;
    // uint32_t Reserved4 : 8;

    uint32_t DWORD[4];
} __attribute__((packed)) xHC_PortRegisters;

typedef struct
{
    SlotContext SlotCtx;
    EndpointContext EndpointCtx[31];
} __attribute__((packed)) DeviceContext;

typedef struct
{
    USBCMD UsbCmd;      // 1
    USBSTS UsbSts;      // 1
    uint32_t PageSize;  // 1
    uint64_t Reserved0; // 2
    uint32_t DNControl; // 1
    uint64_t CRControl; // 2
    uint64_t Reserved1; // 2
    uint64_t Reserved2; // 2
    uint64_t DCBAAP;    // 2
    uint32_t Config;    // 1
    uint32_t Reserved3[241];
    xHC_PortRegisters PortRegs[256];
} xHC_OpRegisters;

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

void XHCIInit()
{
}