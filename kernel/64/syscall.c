#include "syscall.h"
#include "scheduler.h"
#include "draw.h"
#include "drivers/driverman.h"

#define KEY_QUEUE_SIZE 64

extern char KeyQueue[KEY_QUEUE_SIZE];
extern size_t KeyQueueIdx;

extern int MSTicks;

static void SyscallInline(uint64_t Code, uint64_t rsi, uint64_t Selector)
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
        Scheduler_AddProcess(rsi, Selector);
    }
    else if (Code == 4)
    {
        Draw_Packet* Packet = (Draw_Packet*)rsi;
        for (int i = 0; i < Packet->pointsSize; i++)
        {
            Draw_PointEntry Entry = ((Draw_PointEntry*)Packet->points)[i];

            Draw_Point(Entry);
        }
        for (int i = 0; i < Packet->linesSize; i++)
        {
            Draw_LineEntry Entry = ((Draw_LineEntry*)Packet->lines)[i];

            Draw_Line(Entry);
        }
        for (int i = 0; i < Packet->textsSize; i++)
        {
            Draw_TextEntry Entry = ((Draw_TextEntry*)Packet->texts)[i];

            Draw_Text(Entry);
        }
        for (int i = 0; i < Packet->rectsSize; i++)
        {
            Draw_RectEntry Entry = ((Draw_RectEntry*)Packet->rects)[i];
            
            Draw_Rect(Entry);
        }
        for (int i = 0; i < Packet->imagesSize; i++)
        {
            Draw_ImageEntry Entry = ((Draw_ImageEntry*)Packet->images)[i];

            Draw_Image(Entry);
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
}

static void SyscallIndirect(uint64_t Code, uint64_t rsi, uint64_t Selector)
{
    DriverMan_StorageDevice** StorageDevices;
    size_t StorageDevicesCount;
    DriverMan_GetStorageDevices(&StorageDevices, &StorageDevicesCount);
    
    if (Code == 7)
    {
        DriverMan_FilesysMakeDir(StorageDevices[0], (char*)rsi);
    }
    else if (Code == 8)
    {
        DriverMan_FilesysMakeFile(StorageDevices[0], (char*)rsi);
    }
    else if (Code == 9)
    {
        DriverMan_FilesysRemove(StorageDevices[0], (char*)rsi);
    }
    else if (Code == 10)
    {
        FileRequest* Req = (FileRequest*)Selector;
        DriverMan_FilesysWriteFile(StorageDevices[0], (char*)rsi, Req->Data, Req->Bytes);
    }
    else if (Code == 11)
    {
        FileResponse Response;
        Response.Data = DriverMan_FilesysReadFile(StorageDevices[0], (char*)rsi, &Response.BytesRead);
        *(FileResponse*)Selector = Response;
    }
    else if (Code == 12)
    {
        *(size_t*)Selector = DriverMan_FilesysFileSize(StorageDevices[0], (char*)rsi);
    }
    else if (Code == 13)
    {
        FileListResponse Response;
        Response.Data = DriverMan_FilesysListFiles(StorageDevices[0], (char*)rsi, &Response.NumEntries);
        *(FileListResponse*)Selector = Response;
    }
    Scheduler_EndCurrentProcess(false);
    while (1);
}

uint64_t Syscall(uint64_t Code, uint64_t rsi, uint64_t Selector, SoftTSS* SaveState)
{
    if (Code == 3)
    {
        return Scheduler_EndCurrentProcess(true);
    }

    if (Code < 7)
    {
        SyscallInline(Code, rsi, Selector);
        return 0;
    }

    Scheduler_AddSyscallProcess(SyscallIndirect, Code, rsi, Selector);

    return Scheduler_NextProcess(SaveState);
}