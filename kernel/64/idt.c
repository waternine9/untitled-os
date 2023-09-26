#include "idt.h"
#include "vmalloc.h"
#include "softtss.h"
#include "../file.h"
#include "../draw.h"
#include "../vbe.h"
#include "font.h"
#include "keyboard.h"
#include "drivers/fs/fs.h"

#define PRESENT_BIT 47
#define DPL_BIT 45
#define RESERVED_1BIT 44

#define GATE_TYPE_BIT 40
#define RESERVED_5BIT 35
#define IST_BIT 32

#define OFFSET32_BIT 0
#define OFFSET16_BIT 48
#define SEGMENT_SEL_BIT 16
#define OFFSET0_BIT 0

typedef struct 
{
    uint16_t offset_1;        // offset bits 0..15
    uint16_t selector;        // a code segment selector in GDT or LDT
    uint8_t  ist;             // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
    uint8_t  type_attributes; // gate type, dpl, and p fields
    uint16_t offset_2;        // offset bits 16..31
    uint32_t offset_3;        // offset bits 32..63
    uint32_t zero;            // reserved
} __attribute__((packed)) IDTEntry;

static IDTEntry* IDTEntries;

/* Interrupt Handlers */

extern void LoadIDT();
extern void PageFaultS();
extern void GeneralProtectionFaultS();
extern void UnknownFaultS();
extern void TSSFaultS();
extern void SyscallS();
extern void OSStartS();

extern void HandlerIRQ0();
extern void HandlerIRQ1();
extern void HandlerIRQ2();
extern void HandlerIRQ3();
extern void HandlerIRQ4();
extern void HandlerIRQ5();
extern void HandlerIRQ6();
extern void HandlerIRQ7();
extern void HandlerIRQ8();
extern void HandlerIRQ9();
extern void HandlerIRQ10();
extern void HandlerIRQ11();
extern void HandlerIRQ12();
extern void HandlerIRQ13();
extern void HandlerIRQ14();
extern void HandlerIRQ15();
extern void HandlerIVT70();

void PageFault(void)
{

}

void GeneralProtectionFault(void)
{

}

void UnknownFault(void)
{

}

uint8_t Int70Fired = 0;

void CHandlerIVT70(void)
{
    Int70Fired = 1;
}

uint32_t TransformCol(uint32_t Col)
{
    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;
    uint32_t Blue = Col & 0xFF;
    uint32_t Green = (Col & 0xFF00) >> 8;
    uint32_t Red = (Col & 0xFF0000) >> 16;
    return (Red << VbeModeInfo->RedPosition) | (Green << VbeModeInfo->GreenPosition) | (Blue << VbeModeInfo->BluePosition);
}

extern float MSTicks;

extern int SchedRingIdx;
extern SoftTSS* SchedRing;
extern int SchedRingSize;

void AddProcess(uint64_t ip, uint64_t size)
{
    if (SchedRing[SchedRingIdx].Privilege == 1)
    {
        uint8_t* Allocated = (uint8_t*)AllocVM(size & 0xFFFFFFFF);
        
        for (int i = 0;i < (size & 0xFFFFFFFF);i++)
        {
            Allocated[i] = *(uint8_t*)(ip + i);
        }

        if ((SchedRingSize % 16) == 15) 
        {
            SoftTSS* NewRing = (SoftTSS*)AllocVM(sizeof(SoftTSS) * (SchedRingSize / 16 * 16 + 16));
            for (int i = 0;i < SchedRingSize;i++)
            {
                NewRing[i] = SchedRing[i];
            }
            FreeVM((uint64_t)SchedRing);
            SchedRing = NewRing;
        }

        SchedRing[SchedRingSize].Privilege = (size & (uint64_t)0x100000000) >> 32;
        SchedRing[SchedRingSize].cs = 0x20 | 3;
        SchedRing[SchedRingSize].ds = 0x28 | 3;
        SchedRing[SchedRingSize].rip = ip;
        SchedRing[SchedRingSize].rsp = AllocVM(0x4000) + 0x4000;
        SchedRing[SchedRingSize].CodeStart = Allocated;
        SchedRing[SchedRingSize].StackStart = SchedRing[SchedRingSize].rsp - 0x4000;
        SchedRing[SchedRingSize].rflags = 0x200;
        SchedRingSize++;
    }
}

void ExitProcess()
{
    if (SchedRingSize == 0) return;

    FreeVM(SchedRing[SchedRingIdx].CodeStart);
    FreeVM(SchedRing[SchedRingIdx].StackStart);

    for (int i = SchedRingIdx + 1;i < SchedRingSize;i++)
    {
        SchedRing[i - 1] = SchedRing[i];
    }

    SchedRingSize--;

    SchedRingIdx++;
    
    if (SchedRingIdx >= SchedRingSize) SchedRingIdx = 0;
}

extern char KeyQueue[KEY_QUEUE_SIZE];
extern size_t KeyQueueIdx;

uint64_t Syscall(uint64_t Code, uint64_t rsi, uint64_t Selector)
{
    if (Code == 0)
    {
        FreeVM(rsi);
    }
    else if (Code == 1)
    {
        *(void**)rsi = (void*)AllocVM(Selector);
    }
    else if (Code == 2)
    {
        AddProcess(rsi, Selector);
    }
    else if (Code == 3)
    {
        ExitProcess();
        return SchedRing + SchedRingIdx;
    }
    else if (Code == 4)
    {
        VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;

        Draw_Packet* Packet = (Draw_Packet*)rsi;
        for (int i = 0; i < Packet->pointsSize; i++)
        {
            Draw_PointEntry Entry = ((Draw_PointEntry*)Packet->points)[i];
            if (Entry.x < 0) continue;
            if (Entry.y < 0) continue;
            if (Entry.x >= VbeModeInfo->Width) continue;
            if (Entry.y >= VbeModeInfo->Height) continue;

            Entry.col = TransformCol(Entry.col);

            *(uint32_t*)(0xFFFFFFFF90000000 + (Entry.x + Entry.y * VbeModeInfo->Width) * 4) = Entry.col;
        }
        for (int i = 0; i < Packet->linesSize; i++)
        {
            Draw_LineEntry Entry = ((Draw_LineEntry*)Packet->lines)[i];

            Entry.col = TransformCol(Entry.col);

            float fx = Entry.x0;
            float fy = Entry.y0;
            for (;(int)fx != Entry.x1 || (int)fy != Entry.y1;fx += ((float)Entry.x1 - Entry.x0) / VbeModeInfo->Width,fy += ((float)Entry.y1 - Entry.y0) / VbeModeInfo->Width)
            {
                int x = fx;
                int y = fy;
                if (x < 0) continue;
                if (y < 0) continue;
                if (x >= VbeModeInfo->Width) continue;
                if (y >= VbeModeInfo->Height) continue;
                *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = Entry.col;
            }
        }
        for (int i = 0; i < Packet->textsSize; i++)
        {
            Draw_TextEntry Entry = ((Draw_TextEntry*)Packet->texts)[i];

            Entry.col = TransformCol(Entry.col);

            for (int i = 0;Entry.text[i];i++)
            {
                uint8_t* ValP = SysFont[Entry.text[i]];
                for (int iy = 0;iy < 8;iy++)
                {
                    uint8_t ValY = ValP[iy];
                    for (int ix = 0;ix < 8;ix++)
                    {
                        int x = ix + i * 0x8 + Entry.x;
                        int y = iy + Entry.y;
                        uint8_t Val = (ValY >> ix) & 1;
                        if (!Val) 
                        {
                            *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = 0;
                            continue;
                        }
                        if (x < 0) continue;
                        if (y < 0) continue;
                        if (x >= VbeModeInfo->Width) continue;
                        if (y >= VbeModeInfo->Height) continue;
                        *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = Entry.col;
                    }
                }
            }
        }
        for (int i = 0; i < Packet->rectsSize; i++)
        {
            Draw_RectEntry Entry = ((Draw_RectEntry*)Packet->rects)[i];
            
            Entry.col = TransformCol(Entry.col);

            for (int y = Entry.y;y < Entry.y + Entry.h;y++)
            {
                if (y < 0) continue;
                if (y >= VbeModeInfo->Height) continue;
                for (int x = Entry.x;x < Entry.x + Entry.w;x++)
                {
                    if (x < 0) continue;
                    if (x >= VbeModeInfo->Width) continue;
                    *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = Entry.col;
                }
            }
        }
        for (int i = 0; i < Packet->imagesSize; i++)
        {
            Draw_ImageEntry Entry = ((Draw_ImageEntry*)Packet->images)[i];

            for (int y = Entry.y;y < Entry.y + Entry.h;y++)
            {
                if (y < 0) continue;
                if (y >= VbeModeInfo->Height) continue;
                for (int x = Entry.x;x < Entry.x + Entry.w;x++)
                {
                    if (x < 0) continue;
                    if (x >= VbeModeInfo->Width) continue;
                    *(uint32_t*)(0xFFFFFFFF90000000 + (x + y * VbeModeInfo->Width) * 4) = TransformCol(Entry.image[(x - Entry.x) + (y - Entry.y) * Entry.w]);
                }
            }
        }
    }
    else if (Code == 5)
    {
        *(float*)rsi = MSTicks;
    }
    else if (Code == 6)
    {
        *(char*)rsi = KeyQueueIdx > 0 ? KeyQueue[--KeyQueueIdx] : 0;
    }
    else if (Code == 7)
    {
        FSMkdir((char*)rsi);
    }
    else if (Code == 8)
    {
        FSMkfile((char*)rsi);
    }
    else if (Code == 9)
    {
        FSRemove((char*)rsi);
    }
    else if (Code == 10)
    {
        FileRequest* Req = (FileRequest*)Selector;
        FSWriteFile((char*)rsi, Req->Data, Req->Bytes);
    }
    else if (Code == 11)
    {
        FileResponse Response;
        Response.Data = FSReadFile((char*)rsi, &Response.BytesRead);
        *(FileResponse*)Selector = Response;
    }
    else if (Code == 12)
    {
        *(size_t*)Selector = FSFileSize((char*)rsi);
    }
    else if (Code == 13)
    {
        FileListResponse Response;
        Response.Data = FSListFiles((char*)rsi, &Response.NumEntries);
        *(FileListResponse*)Selector = Response;
    }
    return 0;
}

/* Now init */

void IdtInit()
{
    IDTEntries = 0xFFFFFFFFC1300000;
    static void (*Handlers[16])() = {
        HandlerIRQ0,  HandlerIRQ1,  HandlerIRQ2,  HandlerIRQ3,
        HandlerIRQ4,  HandlerIRQ5,  HandlerIRQ6,  HandlerIRQ7,
        HandlerIRQ8,  HandlerIRQ9,  HandlerIRQ10, HandlerIRQ11,
        HandlerIRQ12, HandlerIRQ13, HandlerIRQ14, HandlerIRQ15,
    };
    for (int i = 0;i < 256;i++)
    {
        IDTEntries[i] = (IDTEntry){ 0, 0, 0, 0, 0, 0, 0 };
        if (i == 0xA)   // INVALID TSS FAULT
        {
            IDTEntries[i].type_attributes = 0x8F;
            IDTEntries[i].offset_3 = (((uint64_t)TSSFaultS & 0xFFFFFFFF00000000ULL) >> 32);
            IDTEntries[i].offset_2 = (((uint64_t)TSSFaultS & 0x00000000FFFF0000ULL) >> 16);
            IDTEntries[i].offset_1 = (((uint64_t)TSSFaultS & 0x000000000000FFFFULL) >> 0);
            IDTEntries[i].selector = 0x18; // 64-bit code segment is at 0x18 in the GDT
        }
        if (i == 0xE)   // PAGE FAULT
        {
            IDTEntries[i].type_attributes = 0x8F;
            IDTEntries[i].offset_3 = (((uint64_t)PageFaultS & 0xFFFFFFFF00000000ULL) >> 32);
            IDTEntries[i].offset_2 = (((uint64_t)PageFaultS & 0x00000000FFFF0000ULL) >> 16);
            IDTEntries[i].offset_1 = (((uint64_t)PageFaultS & 0x000000000000FFFFULL) >> 0);
            IDTEntries[i].selector = 0x18; // 64-bit code segment is at 0x18 in the GDT
        }
        else if (i == 0xD)   // GP FAULT
        {
            IDTEntries[i].type_attributes = 0x8F;
            IDTEntries[i].offset_3 = (((uint64_t)GeneralProtectionFaultS & 0xFFFFFFFF00000000ULL) >> 32);
            IDTEntries[i].offset_2 = (((uint64_t)GeneralProtectionFaultS & 0x00000000FFFF0000ULL) >> 16);
            IDTEntries[i].offset_1 = (((uint64_t)GeneralProtectionFaultS & 0x000000000000FFFFULL) >> 0);
            IDTEntries[i].selector = 0x18; // 64-bit code segment is at 0x18 in the GDT
        }
        else if (i == 0x70) // For NVME
        {
            IDTEntries[i].type_attributes = 0x8E;
            IDTEntries[i].offset_3 = 0xFFFFFFFF;
            IDTEntries[i].offset_2 = (((uint64_t)HandlerIVT70 & 0x00000000FFFF0000ULL) >> 16);
            IDTEntries[i].offset_1 = (((uint64_t)HandlerIVT70 & 0x000000000000FFFFULL) >> 0);
            IDTEntries[i].selector = 0x18; // 64-bit code segment is at 0x18 in the GDT
        }
        else if (i < 32) // UNKNOWN FAULT
        {
            IDTEntries[i].type_attributes = 0x8F;
            IDTEntries[i].offset_3 = (((uint64_t)UnknownFaultS & 0xFFFFFFFF00000000ULL) >> 32);
            IDTEntries[i].offset_2 = (((uint64_t)UnknownFaultS & 0x00000000FFFF0000ULL) >> 16);
            IDTEntries[i].offset_1 = (((uint64_t)UnknownFaultS & 0x000000000000FFFFULL) >> 0);
            IDTEntries[i].selector = 0x18; // 64-bit code segment is at 0x18 in the GDT
        }
        else if (i < 48)
        {
            IDTEntries[i].type_attributes = 0x8E;
            IDTEntries[i].offset_3 = (((uint64_t)Handlers[i - 32] & 0xFFFFFFFF00000000ULL) >> 32);
            IDTEntries[i].offset_2 = (((uint64_t)Handlers[i - 32] & 0x00000000FFFF0000ULL) >> 16);
            IDTEntries[i].offset_1 = (((uint64_t)Handlers[i - 32] & 0x000000000000FFFFULL) >> 0);
            IDTEntries[i].selector = 0x18; // 64-bit code segment is at 0x18 in the GDT
        }
        else if (i == 0x80)
        {
            IDTEntries[i].type_attributes = 0x8E | 0b01100000; // DPL is 3
            IDTEntries[i].offset_3 = 0xFFFFFFFF;
            IDTEntries[i].offset_2 = (((uint64_t)SyscallS & 0x00000000FFFF0000ULL) >> 16);
            IDTEntries[i].offset_1 = (((uint64_t)SyscallS & 0x000000000000FFFFULL) >> 0);
            IDTEntries[i].selector = 0x18; // 64-bit code segment is at 0x18 in the GDT
        }
    }
    *(uint16_t*)0x7E50 = sizeof(IDTEntry) * 256 - 1;
    *(uint64_t*)0x7E52 = (uint64_t)IDTEntries;
    LoadIDT();
}