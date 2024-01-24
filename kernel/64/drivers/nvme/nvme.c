#include "nvme.h"
#include "../../pci.h"
#include "../../vmalloc.h"
#include "../../idt.h"
#include "../../io.h"
#include "../../apic.h"

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

static char Dec2Hexa[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

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

uint64_t ForceAlign(uint64_t _Ptr, uint64_t _Boundary)
{
    return (_Ptr - (_Ptr % _Boundary)) + _Boundary;
}

static uint8_t* NVMe_MetadataPtr;

extern int Int70Fired;
extern int SuspendPIT;

void SendCommand(NVME_Controller* Controller, NVME_Command Cmd)
{
    Cmd.MetadataPtr = NVMe_MetadataPtr;

    // Add a new command
    Controller->AdminSubmission[Controller->Tail] = Cmd;
    Controller->Tail = (Controller->Tail + 1) % Controller->NumEntries;
    Int70Fired = 0;
    *(uint32_t*)(Controller->vBar0 + 0x1000) = Controller->Tail;
    int Timeout = 20000;
    while (Timeout--) IO_Wait();
    *(uint32_t*)(Controller->vBar0 + 0x1000 + Controller->DoorbellStride) = Controller->Tail;
}

void SendIOCommand(NVME_Controller* Controller, NVME_Command Cmd, NVME_IO_Pair* Pair, int Index)
{
    SuspendPIT = 1;
    Cmd.MetadataPtr = NVMe_MetadataPtr;

    // Add a new command
    Pair->Submission[Pair->Tail] = Cmd;
    Pair->Tail = (Pair->Tail + 1) % Pair->NumEntries;
    Int70Fired = 0;
    *(uint32_t*)(Controller->vBar0 + 0x1000 + Index * 2 * Controller->DoorbellStride) = Pair->Tail;
    while (!Int70Fired) IO_In8(0x80);
    *(uint32_t*)(Controller->vBar0 + 0x1000 + Index * 2 * Controller->DoorbellStride + Controller->DoorbellStride) = Pair->Tail;
    SuspendPIT = 0;
}

void CreateIOPair(NVME_Controller* Controller)
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

void ReadSectors(NVME_Controller* Controller, uint64_t LBA, uint32_t Num, uint64_t Dest)
{
    while (!Int70Fired) IO_Wait();

    uint64_t* PRPList = ForceAlign(AllocPhys(0x1000 + Controller->PageSize), Controller->PageSize);
    for (int i = 1;i < (Num * 512) / Controller->PageSize;i++)
    {
        PRPList[i - 1] = Dest + Controller->PageSize * i;
    }
    
    NVME_Command Command;
    Command.DWORD0 = NVME_OPCODE_READ;
    Command.NSID = Controller->NSIDs[0];
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

    Command.CmdSpecific[0] = LBA;
    Command.CmdSpecific[1] = 0;
    Command.CmdSpecific[2] = (Num - 1) & 0xFFFF; // Logical Block Count is 0 based
    Command.CmdSpecific[3] = 0;
    Command.CmdSpecific[4] = 0;
    Command.CmdSpecific[5] = 0;

    SendIOCommand(Controller, Command, &Controller->IOPairs[0], 1);

    FreeVM(PRPList);
    FreeVM(Command.MetadataPtr);
}

void WriteSectors(NVME_Controller* Controller, uint64_t LBA, uint32_t Num, uint64_t Src)
{
    while (!Int70Fired) IO_Wait();
    uint64_t* PRPList = ForceAlign(AllocPhys(0x1000 + Controller->PageSize), Controller->PageSize);
    for (int i = 1;i < (Num * 512) / Controller->PageSize;i++)
    {
        PRPList[i - 1] = Src + Controller->PageSize * i;
    }
    
    NVME_Command Command;
    Command.DWORD0 = NVME_OPCODE_WRITE;
    Command.NSID = Controller->NSIDs[0];
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

    Command.CmdSpecific[0] = LBA;
    Command.CmdSpecific[1] = 0;
    Command.CmdSpecific[2] = (Num - 1) & 0xFFFF; // Logical Block Count is 0 based
    Command.CmdSpecific[3] = 0;
    Command.CmdSpecific[4] = 0;
    Command.CmdSpecific[5] = 0;

    SendIOCommand(Controller, Command, &Controller->IOPairs[0], 1);

    FreeVM(PRPList);
    FreeVM(Command.MetadataPtr);
}

void CheckNVMe(pci_device_path Path)
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
                msixTableBase[0].Data = 0x70;
                msixTableBase[0].Mask = 0;
                msixTableBase[1].AddressLow = GetApicBase() & (~0xFULL);
                msixTableBase[1].AddressHigh = 0;
                msixTableBase[1].Data = 0x70;
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
        
        // *(uint16_t*)(0xFFFFFFFF90000000) = 0x0F00 | (Dec2Hexa[Controller.PageSize % 16]);
        // *(uint16_t*)(0xFFFFFFFF90000002) = 0x0F00 | (Dec2Hexa[(Controller.PageSize >> 4) % 16]);
        // *(uint16_t*)(0xFFFFFFFF90000004) = 0x0F00 | (Dec2Hexa[(Controller.PageSize >> 8) % 16]);
        // *(uint16_t*)(0xFFFFFFFF90000006) = 0x0F00 | (Dec2Hexa[(Controller.PageSize >> 12) % 16]);
        // *(uint16_t*)(0xFFFFFFFF90000008) = 0x0F00 | (Dec2Hexa[(Controller.PageSize >> 16) % 16]);
        // *(uint16_t*)(0xFFFFFFFF9000000a) = 0x0F00 | (Dec2Hexa[(Controller.PageSize >> 20) % 16]);
        // *(uint16_t*)(0xFFFFFFFF9000000c) = 0x0F00 | (Dec2Hexa[(Controller.PageSize >> 24) % 16]);
        // *(uint16_t*)(0xFFFFFFFF9000000e) = 0x0F00 | (Dec2Hexa[(Controller.PageSize >> 28) % 16]);
        //asm volatile ("cli\nhlt");

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

            /*
            Controller.OrigPageSize = (*(uint64_t*)(Controller.vBar0 + NVME_CAP) >> 48) & 0b1111;
            Controller.PageSize = 1 << (12 + Controller.OrigPageSize);
            Controller.AdminSubmission = *(volatile uint64_t*)(Controller.vBar0 + NVME_ASQ) & (~0xFFFULL);
            Controller.AdminSubmission = AllocVMAtPhys(Controller.AdminSubmission, 0x5000);
            Controller.AdminCompletion = *(volatile uint64_t*)(Controller.vBar0 + NVME_ACQ) & (~0xFFFULL);
            Controller.AdminCompletion = AllocVMAtPhys(Controller.AdminCompletion, 0x5000);
            Controller.NumEntries = *(volatile uint32_t*)(Controller.vBar0 + NVME_AQA) & 0xFFFULL;
            */


        Controller.Tail = 0;
        Controller.CompletionHead = 0;
        Controller.Phase = 1;
        Controller.NSIDCount = 0;
        Controller.IOPairCount = 0;
        Controller.IOPairs = AllocVM(sizeof(NVME_IO_Pair) * 16);
        NVMe_Controllers[NVMe_ControllerCount] = Controller;

        uint64_t IdenOut = ForceAlign(AllocPhys(0x4000 + Controller.PageSize), Controller.PageSize);
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
       
        // while (1)
        //  {
        //     if (*(uint16_t*)IdenOut == Header.VendorId)
        //     {
        //         *(uint32_t*)(0xFFFFFFFF90000000) = 0xFFF00FFF;
        //         asm volatile ("cli\nhlt");
        //     }
        // }
        // asm volatile ("cli\nhlt");

        if (*(uint8_t*)(IdenOut + 111) < 2) // either 0 or 1
        {
            NVMe_Controllers[NVMe_ControllerCount].IO = true;

            CreateIOPair(&NVMe_Controllers[NVMe_ControllerCount]);
        }

        IdenOut = AllocPhys(0x4000);
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
        SendCommand(&NVMe_Controllers[NVMe_ControllerCount], Identify);



        for (int i = 0;i < 32;i++)
        {
            if (!((uint32_t*)IdenOut)[i]) continue;
            NVMe_Controllers[NVMe_ControllerCount].NSIDs[NVMe_Controllers[NVMe_ControllerCount].NSIDCount++] = ((uint32_t*)IdenOut)[i];
        }
        
        NVMe_ControllerCount++;
    }
}

// Also initializes NVMe device
void ScanForNVMe()
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
                    CheckNVMe(Path);
                }
            }
            else
            {
                CheckNVMe(Path);
            }
        }
    }
}

void NVMEInit()
{
    NVMe_MetadataPtr = AllocVM(4096);
    NVMe_ControllerCount = 0;
    NVMe_Controllers = AllocVM(sizeof(NVME_Controller) * 256);
    ScanForNVMe();
}

bool NVMERead(size_t Num, uint32_t LBA, void* Dest)
{
    for (int j = 0;j < NVMe_ControllerCount;j++)
    {
        if (!NVMe_Controllers[j].IO) continue;
        ReadSectors(&NVMe_Controllers[j], LBA, Num, Dest);
        return true;
    }
    return false;
}

bool NVMEWrite(size_t Num, uint32_t LBA, void* Src)
{
    for (int j = 0;j < NVMe_ControllerCount;j++)
    {
        if (!NVMe_Controllers[j].IO) continue;
        WriteSectors(&NVMe_Controllers[j], LBA, Num, Src);
        return true;
    }
    return false;
}