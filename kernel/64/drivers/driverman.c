#include "driverman.h"
#include "../vmalloc.h"
#include "../memtools.h"
#include "../panic.h"

// DRIVER INCLUDES

// Storage
#include "nvme/nvme.h"

// Filesystem
#include "fs/fs.h"

static DriverMan_Manager* Manager;

extern void* DriverIrqTable[10];

void DriverMan_RegisterStorageDriver(DriverMan_StorageDriver* Driver)
{
    Manager->StorageDrivers[Manager->NumStorageDrivers++] = Driver;
    DriverMan_StorageDriver** NewDrivers = AllocVM(sizeof(DriverMan_StorageDriver*) * (Manager->NumStorageDrivers + 1));
    memcpy(NewDrivers, Manager->StorageDrivers, sizeof(DriverMan_StorageDriver*) * Manager->NumStorageDrivers);
    FreeVM(Manager->StorageDrivers);
    Manager->StorageDrivers = NewDrivers;
}

void DriverMan_RegisterFilesysDriver(DriverMan_FilesystemDriver* Driver)
{
    Manager->FilesysDrivers[Manager->NumFilesysDrivers++] = Driver;
    DriverMan_FilesystemDriver** NewDrivers = AllocVM(sizeof(DriverMan_FilesystemDriver*) * (Manager->NumFilesysDrivers + 1));
    memcpy(NewDrivers, Manager->FilesysDrivers, sizeof(DriverMan_FilesystemDriver*) * Manager->NumFilesysDrivers);
    FreeVM(Manager->FilesysDrivers);
    Manager->FilesysDrivers = NewDrivers;
}

void DriverMan_GetFilesysDrivers(DriverMan_FilesystemDriver*** OutFilesysDrivers, size_t* OutFilesysDriversNum)
{
    *OutFilesysDrivers = Manager->FilesysDrivers;
    *OutFilesysDriversNum = Manager->NumFilesysDrivers;
}

void DriverMan_GetStorageDrivers(DriverMan_StorageDriver*** OutStorageDrivers, size_t* OutStorageDriversNum)
{
    *OutStorageDrivers = Manager->StorageDrivers;
    *OutStorageDriversNum = Manager->NumStorageDrivers;
}

void DriverMan_GetStorageDevices(DriverMan_StorageDevice*** OutStorageDevices, size_t* OutStorageDevicesNum)
{
    *OutStorageDevices = Manager->StorageDevices;
    *OutStorageDevicesNum = Manager->NumStorageDevices;
}

void DriverMan_Init()
{
    Manager = AllocVM(sizeof(DriverMan_Manager));

    Manager->NextIRQ = 0x70;

    Manager->StorageDevices = 0;
    Manager->NumStorageDevices = 0;
    Manager->StorageDrivers = AllocVM(sizeof(DriverMan_StorageDriver*));
    Manager->NumStorageDrivers = 0;

    DriverMan_RegisterStorageDriver(NVMe_GetDriver());

    for (int i = 0;i < Manager->NumStorageDrivers;i++)
    {
        if (Manager->StorageDrivers[i]->NeedsIRQ)
        {
            Manager->StorageDrivers[i]->IRQ = Manager->NextIRQ++;
            DriverIrqTable[Manager->StorageDrivers[i]->IRQ - 0x70] = Manager->StorageDrivers[i]->DriverIRQ;
        }
        Manager->StorageDrivers[i]->DriverInit(Manager->StorageDrivers[i]);


        size_t OriginalNum = Manager->NumStorageDevices;
        Manager->NumStorageDevices += Manager->StorageDrivers[i]->NumDevices;
        DriverMan_StorageDevice** NewDevices = AllocVM(sizeof(DriverMan_StorageDevice*) * Manager->NumStorageDevices);
        if (Manager->StorageDevices) memcpy(NewDevices, Manager->StorageDevices, sizeof(DriverMan_StorageDevice*) * OriginalNum);
        for (int j = 0;j < Manager->StorageDrivers[i]->NumDevices;j++)
        {
            NewDevices[OriginalNum + j] = &Manager->StorageDrivers[i]->Devices[j];
        }
        FreeVM(Manager->StorageDevices);
        Manager->StorageDevices = NewDevices;
    }

    
    Manager->FilesysDrivers = AllocVM(sizeof(DriverMan_FilesystemDriver*));
    Manager->NumFilesysDrivers = 0;

    DriverMan_RegisterFilesysDriver(CustomFS_GetDriver());

    for (int i = 0;i < Manager->NumStorageDevices;i++)
    {
        Manager->StorageDevices[i]->FormatType = 0;
        
        for (int j = 0;j < Manager->NumFilesysDrivers;j++)
        {
            if (Manager->FilesysDrivers[j]->FilesysIsFormatted(Manager->FilesysDrivers[j], Manager->StorageDevices[i]))
            {
                Manager->StorageDevices[i]->FormatType = Manager->FilesysDrivers[j];
                break;
            }
        }
    }
}

static bool Strcmp(char* x, char* y)
{
	int xlen = 0, ylen = 0;
	while (x[xlen] && x[xlen] != ' ') xlen++;
	while (y[ylen] && x[ylen] != ' ') ylen++;

	if (xlen != ylen) return false;

	for (int i = 0;i < xlen;i++)
	{
		if (x[i] != y[i]) return false;
	}
	return true;
}

DriverMan_StorageDriver* DriverMan_GetStorageDriver(const char* Name)
{
    for (int i = 0;i < Manager->NumStorageDrivers;i++)
    {
        if (Strcmp(Manager->StorageDrivers[i]->Header.Name, Name)) return Manager->StorageDrivers[i];
    }
    return 0;
}

DriverMan_FilesystemDriver* DriverMan_GetFilesysDriver(const char* Name)
{
    for (int i = 0;i < Manager->NumFilesysDrivers;i++)
    {
        if (Strcmp(Manager->FilesysDrivers[i]->Header.Name, Name)) return Manager->FilesysDrivers[i];
    }
    return 0;
}

void DriverMan_Run()
{
    FreeVM(Manager->StorageDevices);
    Manager->StorageDevices = 0;
    Manager->NumStorageDevices = 0;

    for (int i = 0;i < Manager->NumStorageDrivers;i++)
    {
        Manager->StorageDrivers[i]->DriverRun(Manager->StorageDrivers[i]);

        size_t OriginalNum = Manager->NumStorageDevices;
        Manager->NumStorageDevices += Manager->StorageDrivers[i]->NumDevices;
        DriverMan_StorageDevice** NewDevices = AllocVM(sizeof(DriverMan_StorageDevice*) * Manager->NumStorageDevices);
        memcpy(NewDevices, Manager->StorageDevices, sizeof(DriverMan_StorageDevice*) * OriginalNum);
        for (int j = 0;j < Manager->StorageDrivers[i]->NumDevices;j++)
        {
            NewDevices[OriginalNum + j] = &Manager->StorageDrivers[i]->Devices[j];
        }
        FreeVM(Manager->StorageDevices);
        Manager->StorageDevices = NewDevices;
    }
}

void DriverMan_StorageReadFromDevice(DriverMan_StorageDevice* Device, size_t NumSectors, uint32_t LBA, void* Dest)
{
    Device->ParentDriver->StorageRead(Device->ParentDriver, Device->MyID, NumSectors, LBA, Dest);
}

void DriverMan_StorageWriteToDevice(DriverMan_StorageDevice* Device, size_t NumSectors, uint32_t LBA, void* Src)
{
    Device->ParentDriver->StorageWrite(Device->ParentDriver, Device->MyID, NumSectors, LBA, Src);
}

void DriverMan_FilesysFormat(DriverMan_StorageDevice* Device, char* FSName)
{
    DriverMan_FilesystemDriver* Driver = DriverMan_GetFilesysDriver(FSName);
    if (!Driver) return;
    Driver->FilesysFormat(Driver, Device);
    Device->FormatType = Driver;
}

bool DriverMan_FilesysTryFormat(DriverMan_StorageDevice* Device, char* FSName)
{
    DriverMan_FilesystemDriver* Driver = DriverMan_GetFilesysDriver(FSName);
    if (!Driver) return;
    bool Formatted = Driver->FilesysTryFormat(Driver, Device);
    if (Formatted) Device->FormatType = Driver;
    return Formatted;
}

bool DriverMan_FilesysIsFormatted(DriverMan_StorageDevice* Device)
{
    return Device->FormatType != 0;
}

bool DriverMan_FilesysMakeFile(DriverMan_StorageDevice* Device, char* File)
{
    if (Device->FormatType == 0) return false;
    return Device->FormatType->FilesysMakeFile(Device->FormatType, Device, File);
}

bool DriverMan_FilesysMakeDir(DriverMan_StorageDevice* Device, char* Dir)
{
    if (Device->FormatType == 0) return false;
    return Device->FormatType->FilesysMakeDir(Device->FormatType, Device, Dir);
}

bool DriverMan_FilesysRemove(DriverMan_StorageDevice* Device, char* AnyDir)
{
    if (Device->FormatType == 0) return false;
    return Device->FormatType->FilesysRemove(Device->FormatType, Device, AnyDir);
}

bool DriverMan_FilesysWriteFile(DriverMan_StorageDevice* Device, char* File, void* Data, size_t Bytes)
{
    if (Device->FormatType == 0) return false;
    return Device->FormatType->FilesysWriteFile(Device->FormatType, Device, File, Data, Bytes);
}

void* DriverMan_FilesysReadFile(DriverMan_StorageDevice* Device, char* File, size_t* BytesRead)
{
    if (Device->FormatType == 0) return false;
    return Device->FormatType->FilesysReadFile(Device->FormatType, Device, File, BytesRead);
}

FileListEntry* DriverMan_FilesysListFiles(DriverMan_StorageDevice* Device, char* Dir, size_t* NumFiles)
{
    if (Device->FormatType == 0) return false;
    return Device->FormatType->FilesysListFiles(Device->FormatType, Device, Dir, NumFiles);
}

size_t DriverMan_FilesysFileSize(DriverMan_StorageDevice* Device, char* File)
{
    if (Device->FormatType == 0) return false;
    return Device->FormatType->FilesysFileSize(Device->FormatType, Device, File);
}