#include "nvme.h"
#include "../../pci.h"
#include "../../vmalloc.h"
#include "../../idt.h"
#include "../../io.h"
#include "../../apic.h"
#include "../../panic.h"

#define NVME_CAP 0x0
#define NVME_CSTS 0x1C
#define NVME_CC 0x14
#define NVME_NSSR 0x20
#define NVME_AQA 0x24
#define NVME_ASQ 0x28
#define NVME_ACQ 0x30
#define NVME_VS 0x8
#define NVME_INTMC 0x10
#define NVME_INTMS 0xC
#define NVME_RESET_MAGIC 0x4E564D65

#define NVME_OPCODE_CREATE_IO_SUBMISSION 0x1
#define NVME_OPCODE_CREATE_IO_COMPLETION 0x5
#define NVME_OPCODE_WRITE 0x1
#define NVME_OPCODE_READ 0x2

#define NVME_QUEUE_ENTRIES_SIZE 64
#define NVME_STATUS_RING_SIZE 128

static int CommandCompleted;

typedef struct
{
    uint32_t DWORD0;
    uint32_t NSID;
    uint64_t Reserved;
    uint64_t MetadataPtr;
    uint64_t PRP0;
    uint64_t PRP1;
    uint32_t CmdSpecific[6];
} __attribute__((packed)) NVME_Command;

typedef struct
{
    uint64_t Low;
    uint64_t High;
} __attribute__((packed)) NVME_Completion;

typedef struct
{
    NVME_Command* Submission;
    NVME_Completion* Completion;
    int Tail;
    int CompletionHead;
    int NumEntries;
    uint8_t Phase;
    int ID;
} __attribute__((packed)) NVME_IO_Pair;

typedef struct
{
    uint64_t Capabilities;
    int MaxPageSize;
    int MaxQueueSize;
    int OrigMaxPageSize;
    int DoorbellStride;
    int NumEntries;
    uint64_t PageSize;
    uint64_t OrigPageSize;
    int Tail;
    int CompletionHead;
    NVME_IO_Pair* IOPairs;
    int IOPairCount;
    pci_device_path Path;
    void* vBar0;
    NVME_Command* AdminSubmission;
    NVME_Completion* AdminCompletion;
    bool IO;
    uint8_t Phase;
    uint32_t NSIDs[32];
    int NSIDCount;
    int ID;
} NVME_Controller;

NVME_Controller* NVMe_Controllers;
static int NVMe_ControllerCount;

extern float MSTicks;

static uint64_t ForceAlign(uint64_t _Ptr, uint64_t _Boundary)
{
    return (_Ptr - (_Ptr % _Boundary)) + _Boundary;
}

static uint8_t* NVMe_MetadataPtr;

extern int Int70Fired;
extern int SuspendPIT;

static bool SendCommand(NVME_Controller* Controller, NVME_Command Cmd)
{
    Cmd.MetadataPtr = NVMe_MetadataPtr;

    // Add a new command
    Controller->AdminSubmission[Controller->Tail] = Cmd;
    Controller->Tail = (Controller->Tail + 1) % Controller->NumEntries;
    CommandCompleted = 0;
    *(uint32_t*)(Controller->vBar0 + 0x1000) = Controller->Tail;
    while (!CommandCompleted) IO_Wait();
    *(uint32_t*)(Controller->vBar0 + 0x1000 + Controller->DoorbellStride) = Controller->Tail;
}

static bool SendIOCommand(NVME_Controller* Controller, NVME_Command Cmd, NVME_IO_Pair* Pair, int Index)
{
    SuspendPIT = 1;
    Cmd.MetadataPtr = NVMe_MetadataPtr;
    // Add a new command
    Pair->Submission[Pair->Tail] = Cmd;
    Pair->Tail = (Pair->Tail + 1) % Pair->NumEntries;
    CommandCompleted = 0;
    *(uint32_t*)(Controller->vBar0 + 0x1000 + Index * 2 * Controller->DoorbellStride) = Pair->Tail;
    while (!CommandCompleted) IO_In8(0x80);
    *(uint32_t*)(Controller->vBar0 + 0x1000 + Index * 2 * Controller->DoorbellStride + Controller->DoorbellStride) = Pair->Tail;
    SuspendPIT = 0;
    
    uint8_t StatusCode = Pair->Completion[Pair->CompletionHead].High << 49;
    Pair->CompletionHead = (Pair->CompletionHead + 1) % Pair->NumEntries;

    return StatusCode == 0;
}

static void CreateIOPair(NVME_Controller* Controller)
{

    NVME_IO_Pair Pair;
    Pair.Tail = 0;
    Pair.CompletionHead = 0;
    Pair.ID = Controller->IOPairCount + 1;
    Pair.NumEntries = Controller->MaxQueueSize;
    if (NVME_QUEUE_ENTRIES_SIZE < Pair.NumEntries) Pair.NumEntries = NVME_QUEUE_ENTRIES_SIZE;
    Pair.Submission = ForceAlign(AllocPhys(sizeof(NVME_Command) * Pair.NumEntries + Controller->PageSize), Controller->PageSize);
    Pair.Completion = ForceAlign(AllocPhys(sizeof(NVME_Completion) * Pair.NumEntries + Controller->PageSize), Controller->PageSize);

    NVME_Command Command;
    
    Command.DWORD0 = NVME_OPCODE_CREATE_IO_COMPLETION;
    Command.NSID = 0;
    Command.Reserved = 0;
    Command.PRP0 = (uint64_t)Pair.Completion;
    Command.PRP1 = 0;
    Command.CmdSpecific[0] = (Controller->IOPairCount + 1) | ((Pair.NumEntries - 1) << 16); // 16 entries - 1
    Command.CmdSpecific[1] = 1 | 2 | (1 << 16);
    Command.CmdSpecific[2] = 0;
    Command.CmdSpecific[3] = 0;
    Command.CmdSpecific[4] = 0;
    Command.CmdSpecific[5] = 0;

    SendCommand(Controller, Command);
    

    Command.DWORD0 = NVME_OPCODE_CREATE_IO_SUBMISSION;
    Command.NSID = 0;
    Command.Reserved = 0;
    Command.PRP0 = (uint64_t)Pair.Submission;
    Command.PRP1 = 0;
    Command.CmdSpecific[0] = (Controller->IOPairCount + 1) | ((Pair.NumEntries - 1) << 16); // 16 entries - 1
    Command.CmdSpecific[1] = 1 | ((Controller->IOPairCount + 1) << 16); // Flags is in low word, so flags is 1, since the flag for contiguous entries is 1 << 0
    Command.CmdSpecific[2] = 0;
    Command.CmdSpecific[3] = 0;
    Command.CmdSpecific[4] = 0;
    Command.CmdSpecific[5] = 0;

    SendCommand(Controller, Command);
    Pair.Tail = *(uint32_t*)(Controller->vBar0 + 0x1000 + ((Controller->IOPairCount + 1) * 2) * Controller->DoorbellStride);

    Controller->IOPairs[Controller->IOPairCount] = Pair;
    Controller->IOPairCount++;
}

static void ReadSectors(NVME_Controller* Controller, uint32_t NSID, uint64_t LBA, uint32_t Num, uint64_t Dest)
{
    uint64_t DataBuf = AllocPhys(0x1000 + Controller->PageSize);
    uint64_t* PRPList = ForceAlign(DataBuf, Controller->PageSize);
    for (int i = 1;i < (Num * 512) / Controller->PageSize;i++)
    {
        PRPList[i - 1] = Dest + Controller->PageSize * i;
    }
    
    NVME_Command Command;
    Command.DWORD0 = NVME_OPCODE_READ;
    Command.NSID = NSID;
    Command.Reserved = 0;
    
    Command.PRP0 = Dest;
    if (Num >= 2 * (Controller->PageSize / 512) - (LBA % (Controller->PageSize / 512)))
    {
        Command.PRP1 = (uint64_t)PRPList & 0xFFFFFFFFULL;
    }
    else if (Num >= 1 * (Controller->PageSize / 512) - (LBA % (Controller->PageSize / 512)))
    {
        Command.PRP1 = Dest + Controller->PageSize;
    }
    else
    {
        Command.PRP1 = 0;
    }

    Command.MetadataPtr = NVMe_MetadataPtr;
    Command.CmdSpecific[0] = LBA;
    Command.CmdSpecific[1] = 0;
    Command.CmdSpecific[2] = (Num - 1) & 0xFFFF; // Logical Block Count is 0 based
    Command.CmdSpecific[3] = 0;
    Command.CmdSpecific[4] = 0;
    Command.CmdSpecific[5] = 0;

    SendIOCommand(Controller, Command, &Controller->IOPairs[0], 1);

    FreeVM(DataBuf);
}

static void WriteSectors(NVME_Controller* Controller, uint32_t NSID, uint64_t LBA, uint32_t Num, uint64_t Src)
{
    uint64_t DataBuf = AllocPhys(0x1000 + Controller->PageSize);
    uint64_t* PRPList = ForceAlign(DataBuf, Controller->PageSize);
    for (int i = 1;i < (Num * 512) / Controller->PageSize;i++)
    {
        PRPList[i - 1] = Src + Controller->PageSize * i;
    }
    
    NVME_Command Command;
    Command.DWORD0 = NVME_OPCODE_WRITE;
    Command.NSID = NSID;
    Command.Reserved = 0;
    
    Command.PRP0 = Src;
    if (Num >= 2 * (Controller->PageSize / 512) - (LBA % (Controller->PageSize / 512)))
    {
        Command.PRP1 = (uint64_t)PRPList & 0xFFFFFFFFULL;
    }
    else if (Num >= 1 * (Controller->PageSize / 512) - (LBA % (Controller->PageSize / 512)))
    {
        Command.PRP1 = Src + Controller->PageSize;
    }
    else
    {
        Command.PRP1 = 0;
    }

    Command.MetadataPtr = NVMe_MetadataPtr;
    Command.CmdSpecific[0] = LBA;
    Command.CmdSpecific[1] = 0;
    Command.CmdSpecific[2] = (Num - 1) & 0xFFFF; // Logical Block Count is 0 based
    Command.CmdSpecific[3] = 0;
    Command.CmdSpecific[4] = 0;
    Command.CmdSpecific[5] = 0;

    SendIOCommand(Controller, Command, &Controller->IOPairs[0], 1);

    FreeVM(DataBuf);
}

static void RefreshNamespaces(NVME_Controller* Controller)
{
    uint64_t DataBuf = AllocPhys(0x1000 + Controller->PageSize);
    uint64_t IdenOut = ForceAlign(DataBuf, Controller->PageSize);
    NVME_Command Identify;
    Identify.DWORD0 = 6;
    Identify.Reserved = 0;
    Identify.NSID = 0;
    Identify.PRP0 = IdenOut;
    Identify.PRP1 = 0;
    Identify.CmdSpecific[0] = 2;
    Identify.CmdSpecific[1] = 0;
    Identify.CmdSpecific[2] = 0;
    Identify.CmdSpecific[3] = 0;
    Identify.CmdSpecific[4] = 0;
    Identify.CmdSpecific[5] = 0;
    SendCommand(Controller, Identify);

    Controller->NSIDCount = 0;

    for (int i = 0;i < 32;i++)
    {
        if (!((uint32_t*)IdenOut)[i]) break;
        Controller->NSIDs[Controller->NSIDCount++] = ((uint32_t*)IdenOut)[i];
    }
    
    FreeVM(DataBuf);
}

static void GetNamespaceData(NVME_Controller* Controller, uint32_t NSID, uint64_t* MaxLBA)
{
    uint64_t DataBuf = AllocPhys(0x1000 + Controller->PageSize);
    uint64_t IdenOut = ForceAlign(DataBuf, Controller->PageSize);
    NVME_Command Identify;
    Identify.DWORD0 = 6;
    Identify.Reserved = 0;
    Identify.NSID = NSID;
    Identify.PRP0 = IdenOut;
    Identify.PRP1 = 0;
    Identify.CmdSpecific[0] = 0;
    Identify.CmdSpecific[1] = 0;
    Identify.CmdSpecific[2] = 0;
    Identify.CmdSpecific[3] = 0;
    Identify.CmdSpecific[4] = 0;
    Identify.CmdSpecific[5] = 0;
    SendCommand(Controller, Identify);

    *MaxLBA = *(uint64_t*)IdenOut;

    FreeVM(DataBuf);
}

static void CheckNVMe(pci_device_path Path, int UseIRQ)
{
    pci_device_header Header = PCI_QueryDeviceHeader(Path);
    pci_device_specialty Specialty = PCI_QueryDeviceSpecialty(Header);
    if (Specialty == PCI_DEVICE_NVME)
    {

        NVME_Controller Controller;
        Controller.Path = Path;

        // Traverse the PCI capabilities list to find the MSI-X capability
        
        
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
                msixTableBase[0].Data = UseIRQ;
                msixTableBase[0].Mask = 0;
                msixTableBase[1].AddressLow = GetApicBase() & (~0xFULL);
                msixTableBase[1].AddressHigh = 0;
                msixTableBase[1].Data = UseIRQ;
                msixTableBase[1].Mask = 0;
                AllocUnMap(msixTableBase0, 0x1000);

                // Enable MSI-X
                msixControl |= 1ULL << 15; // Set the MSI-X enable bit
                //*(uint32_t*)0xFFFFFFFF9000000E = 0xFFFFFFFF;
                PCI_Write16(Path, CapPointer + 0x02, msixControl);
            }
            // Check for MSI capability
            if (PCI_Read8(Path, CapPointer) == 0x05)
            {
                uint16_t msiControl = PCI_Read16(Path, CapPointer + 0x02);
                PCI_Write16(Path, CapPointer + 0x02, msiControl & ~1ULL);
            }
            CapPointer = PCI_Read8(Path, CapPointer + 0x1);
        }

        Header.Command |= 1 << 2; // Enable bus master
        Header.Command &= ~(1ULL << 10); // Enable bus master
        Header.Command |= 1 << 1; // Enable memory space
        PCI_Write16(Path, 0x4, Header.Command);

        if ((Header.BAR0 & 0b110) == 0b100)
        {
            Controller.vBar0 = ((uint64_t)Header.BAR0 & 0xFFFFFFF0) | ((uint64_t)Header.BAR1 << 32);
        }
        else 
        {
            Controller.vBar0 = ((uint64_t)Header.BAR0 & 0xFFFFFFF0);
        }
        Controller.vBar0 = AllocVMAtFlags(Controller.vBar0, 0x10000, (1 << MALLOC_USER_SUPERVISOR_BIT) || (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));
        
        Controller.Capabilities = *(uint64_t*)(Controller.vBar0 + NVME_CAP);
        Controller.DoorbellStride = 1 << (2 + ((Controller.Capabilities >> 32) & 0b1111ULL));
        Controller.MaxPageSize = 1 << (12 + ((Controller.Capabilities & (0b1111ULL << 52)) >> 52));
        Controller.MaxQueueSize = (Controller.Capabilities & 0xFFFF) + 1;
        Controller.OrigMaxPageSize = (Controller.Capabilities & (0b1111ULL << 52)) >> 52;

        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) &= ~(1ULL); // Reset the controller
        while ((*(volatile uint32_t*)(Controller.vBar0 + NVME_CSTS) & 1) == 1); // Wait until the controller is ready
        
        uint32_t Version = *(uint32_t*)(Controller.vBar0 + NVME_VS);
        uint8_t VersionMinor = (Version >> 8) & 0xFF;
        uint16_t VersionMajor = (Version >> 16) & 0xFFFF;

        uint64_t CapCSS = (*(uint64_t*)(Controller.vBar0 + NVME_CAP) >> 37);

        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) &= ~(0b111U << 4);
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) |= (0b110U << 4);
        
        Controller.OrigPageSize = (*(uint64_t*)(Controller.vBar0 + NVME_CAP) >> 48) & 0xF;
        Controller.PageSize = 1 << (12 + Controller.OrigPageSize);
        if (Controller.PageSize != 0x1000) KernelPanic("PANIC: Invalid NVMe controller page size!");

        // Set round robin arbitration mechanism
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) &= ~(0b111UL << 11);
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) &= ~(0b1111UL << 7);
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) |= Controller.OrigPageSize << 7;
        
        *(volatile uint32_t*)(Controller.vBar0 + NVME_AQA) &= ~(0xFFFULL);
        *(volatile uint32_t*)(Controller.vBar0 + NVME_AQA) &= ~(0xFFFULL << 16);
        *(volatile uint32_t*)(Controller.vBar0 + NVME_AQA) |= (NVME_QUEUE_ENTRIES_SIZE - 1) & 0xFFF;
        *(volatile uint32_t*)(Controller.vBar0 + NVME_AQA) |= ((NVME_QUEUE_ENTRIES_SIZE - 1) & 0xFFF) << 16;
        Controller.NumEntries = NVME_QUEUE_ENTRIES_SIZE;

        // Allocate and register the admin queues
        Controller.AdminSubmission = ForceAlign(AllocPhys(sizeof(NVME_Command) * NVME_QUEUE_ENTRIES_SIZE + Controller.PageSize), Controller.PageSize);
        Controller.AdminCompletion = ForceAlign(AllocPhys(sizeof(NVME_Completion) * NVME_QUEUE_ENTRIES_SIZE + Controller.PageSize), Controller.PageSize);

        *(volatile uint64_t*)(Controller.vBar0 + NVME_ASQ) = (uint64_t)Controller.AdminSubmission & (~0xFFFULL);
        *(volatile uint64_t*)(Controller.vBar0 + NVME_ACQ) = (uint64_t)Controller.AdminCompletion & (~0xFFFULL);

        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) &= ~(0xFU << 16);
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) &= ~(0xFU << 20);

        // *(volatile uint32_t*)(Controller.vBar0 + NVME_INTMS) = 0xFFFFFFFF;

        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) &= ~(0b1111ULL << 16);
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) &= ~(0b1111ULL << 20);
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) |= 6 << 16;
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) |= 4 << 20;
        
        *(volatile uint32_t*)(Controller.vBar0 + NVME_CC) |= 1; // Enable the controller
        while ((*(volatile uint32_t*)(Controller.vBar0 + NVME_CSTS) & 1) == 0); // Wait until the controller is ready

        Controller.Tail = 0;
        Controller.CompletionHead = 0;
        Controller.Phase = 1;
        Controller.NSIDCount = 0;
        Controller.IOPairCount = 0;
        Controller.IOPairs = AllocVM(sizeof(NVME_IO_Pair) * 16);
        NVMe_Controllers[NVMe_ControllerCount] = Controller;

        uint64_t DataBuf = AllocPhys(0x1000 + NVMe_Controllers[NVMe_ControllerCount].PageSize);
        uint64_t IdenOut = ForceAlign(DataBuf, NVMe_Controllers[NVMe_ControllerCount].PageSize);
        NVME_Command Identify;
        Identify.DWORD0 = 6;
        Identify.Reserved = 0;
        Identify.NSID = 0;
        Identify.PRP0 = IdenOut;
        Identify.PRP1 = 0;
        Identify.CmdSpecific[0] = 1;
        Identify.CmdSpecific[1] = 0;
        Identify.CmdSpecific[2] = 0;
        Identify.CmdSpecific[3] = 0;
        Identify.CmdSpecific[4] = 0;
        Identify.CmdSpecific[5] = 0;
        SendCommand(&NVMe_Controllers[NVMe_ControllerCount], Identify);
       
        if (*(uint8_t*)(IdenOut + 111) < 2) // either 0 or 1
        {
            NVMe_Controllers[NVMe_ControllerCount].IO = true;

            CreateIOPair(&NVMe_Controllers[NVMe_ControllerCount]);
        }
        else
        {
            KernelPanic("PANIC: NVMe controller not I/O compatible!");
        }

        FreeVM(DataBuf);

        RefreshNamespaces(&NVMe_Controllers[NVMe_ControllerCount]);

        NVMe_ControllerCount++;
    }
}

// Also initializes NVMe device
static void ScanForNVMe(int UseIRQ)
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
                    CheckNVMe(Path, UseIRQ);
                }
            }
            else
            {
                CheckNVMe(Path, UseIRQ);
            }
        }
    }
}

static void NVMEInit(int UseIRQ)
{
    NVMe_MetadataPtr = AllocVM(4096);
    NVMe_ControllerCount = 0;
    NVMe_Controllers = AllocVM(sizeof(NVME_Controller) * 256);
    ScanForNVMe(UseIRQ);
    if (NVMe_ControllerCount > 1)
    {
        KernelPanic("PANIC: More than one NVMe controller found, only one is supported");
    }
}

static bool NVMERead(uint32_t NSID, size_t Num, uint32_t LBA, void* Dest)
{
    ReadSectors(&NVMe_Controllers[0], NSID, LBA, Num, Dest);
    return true;
}

static bool NVMEWrite(uint32_t NSID, size_t Num, uint32_t LBA, void* Src)
{
    WriteSectors(&NVMe_Controllers[0], NSID, LBA, Num, Src);
    return true;
}

static void NVME_Driver_IRQHandler()
{
    CommandCompleted = 1;
    ApicEOI();
}

static void RefreshDriverDevices(DriverMan_StorageDriver* MyDriver)
{
    size_t TotalDeviceCount = 0;
    for (int i = 0;i < NVMe_ControllerCount;i++)
    {
        TotalDeviceCount += NVMe_Controllers[i].NSIDCount;
    }
    MyDriver->NumDevices = TotalDeviceCount;
    if (MyDriver->Devices) FreeVM(MyDriver->Devices);
    MyDriver->Devices = AllocVM(sizeof(DriverMan_StorageDevice) * TotalDeviceCount);
    int CurDeviceIdx = 0;
    for (int i = 0;i < NVMe_ControllerCount;i++)
    {
        for (int j = 0; j < NVMe_Controllers[i].NSIDCount;j++)
        {
            uint32_t CurrentNSID = NVMe_Controllers[i].NSIDs[j];
            
            uint64_t NamespaceSize;
            GetNamespaceData(&NVMe_Controllers[i], CurrentNSID, &NamespaceSize);
            //NamespaceSize *= NVMe_Controllers[i].PageSize;
            MyDriver->Devices[CurDeviceIdx].Capacity = NamespaceSize;
            MyDriver->Devices[CurDeviceIdx].MaxReadSectorCount = NamespaceSize / 512;
            MyDriver->Devices[CurDeviceIdx].MaxWriteSectorCount = NamespaceSize / 512;
            MyDriver->Devices[CurDeviceIdx].MyID = CurDeviceIdx;
            MyDriver->Devices[CurDeviceIdx].MyUID = CurrentNSID;
            MyDriver->Devices[CurDeviceIdx].ParentDriver = MyDriver;
            CurDeviceIdx++;
        }
    }
}

static void NVME_Driver_Init(DriverMan_StorageDriver* MyDriver)
{
    CommandCompleted = 0;
    NVMEInit(MyDriver->IRQ);
    RefreshDriverDevices(MyDriver);
}

static void NVME_Driver_Run(DriverMan_StorageDriver* MyDriver)
{
    for (int i = 0;i < NVMe_ControllerCount;i++)
    {
        RefreshNamespaces(&NVMe_Controllers[i]);
    }
    RefreshDriverDevices(MyDriver);
}

static bool NVME_Driver_Read(DriverMan_StorageDriver* MyDriver, int DeviceIdx, size_t NumSectors, uint32_t LBA, void* Dest)
{
    return NVMERead(MyDriver->Devices[DeviceIdx].MyUID, NumSectors, LBA, Dest);
}

static bool NVME_Driver_Write(DriverMan_StorageDriver* MyDriver, int DeviceIdx, size_t NumSectors, uint32_t LBA, void* Src)
{
    return NVMEWrite(MyDriver->Devices[DeviceIdx].MyUID, NumSectors, LBA, Src);
}

DriverMan_StorageDriver* NVMe_GetDriver()
{
    DriverMan_StorageDriver* OutDriver = AllocVM(sizeof(DriverMan_StorageDriver));

    OutDriver->Header.Name = "NVMe";
    OutDriver->Header.Version = 0x00010000;
    OutDriver->NeedsIRQ = 1;
    OutDriver->DriverIRQ = NVME_Driver_IRQHandler;
    OutDriver->DriverInit = NVME_Driver_Init;
    OutDriver->DriverRun = NVME_Driver_Run;
    OutDriver->StorageRead = NVME_Driver_Read;
    OutDriver->StorageWrite = NVME_Driver_Write;

    return OutDriver;
}