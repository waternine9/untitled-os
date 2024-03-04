#ifndef DRIVER_MAN_H
#define DRIVER_MAN_H

#include "../../include.h"

typedef struct
{
    const char* Name;
    uint32_t Version; // Low 16 bits: Minor version, High 16 bits: Major version
} DriverMan_DriverHeader;

struct _DriverMan_StorageDriver;

struct _DriverMan_FilesystemDriver;

typedef struct
{
    struct _DriverMan_StorageDriver* ParentDriver; // The driver that controls the device
    size_t MyID; // The index of the device in it's parent driver's device list
    size_t MyUID; // The unique identifier of this device (not unique across different drivers), useful for checking if the device is still in the parent driver's devices list
    size_t MaxReadSectorCount; // When performing a read operation, the amount of sectors to read can never be above this value
    size_t MaxWriteSectorCount; // When performing a write operation, the amount of sectors to read can never be above this value
    size_t Capacity; // Size of the storage device in bytes
    
    struct _DriverMan_FilesystemDriver* FormatType; // 0 if unknown format
} DriverMan_StorageDevice;

typedef struct _DriverMan_StorageDriver
{
    DriverMan_DriverHeader Header;

    bool NeedsIRQ; // The driver sets this to true in its Init function if it needs an IRQ allocated for it

    size_t IRQ; // The IRQ that this driver is using for interrupts
    void(*DriverIRQ)(); // This is the IRQ handler for the driver
    
    DriverMan_StorageDevice* Devices;
    size_t NumDevices;

    void(*DriverInit)(struct _DriverMan_StorageDriver* MyDriver); // The driver must have the Devices member fully allocated and filled out by the end of this function
    void(*DriverRun)(struct _DriverMan_StorageDriver* MyDriver); // This function runs repeatedly so that the driver can do things like refresh the connected devices
    bool(*StorageRead)(struct _DriverMan_StorageDriver* MyDriver, int DeviceIdx, size_t NumSectors, uint32_t LBA, void* Dest);
    bool(*StorageWrite)(struct _DriverMan_StorageDriver* MyDriver, int DeviceIdx, size_t NumSectors, uint32_t LBA, void* Src);
} DriverMan_StorageDriver;

#include "../../file.h"

typedef struct _DriverMan_FilesystemDriver
{
    DriverMan_DriverHeader Header;

    void(*DriverInit)(struct _DriverMan_FilesystemDriver* MyDriver); // The driver must have the Devices member fully allocated and filled out by the end of this function
    void(*FilesysFormat)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device);
    bool(*FilesysTryFormat)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device); // Returns true if ended up formatting
    bool(*FilesysIsFormatted)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device);
    bool(*FilesysMakeFile)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* File);
    bool(*FilesysMakeDir)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* Dir);
    bool(*FilesysRemove)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* AnyDir);
    bool(*FilesysWriteFile)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* File, void* Data, size_t Bytes);
    void*(*FilesysReadFile)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* File, size_t* BytesRead);
    size_t(*FilesysFileSize)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* File);
    FileListEntry*(*FilesysListFiles)(struct _DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* Dir, size_t* NumFiles);
} DriverMan_FilesystemDriver;

typedef struct _DriverMan_Manager
{
    DriverMan_StorageDevice** StorageDevices;
    size_t NumStorageDevices;
    DriverMan_StorageDriver** StorageDrivers;
    size_t NumStorageDrivers;

    DriverMan_FilesystemDriver** FilesysDrivers;
    size_t NumFilesysDrivers;

    size_t NextIRQ;
} DriverMan_Manager;

void DriverMan_Init();
void DriverMan_Run();

void DriverMan_RegisterStorageDriver(DriverMan_StorageDriver* Driver);
DriverMan_StorageDriver* DriverMan_GetStorageDriver(const char* Name);
void DriverMan_GetStorageDrivers(DriverMan_StorageDriver*** OutStorageDrivers, size_t* OutStorageDriversNum);
void DriverMan_GetStorageDevices(DriverMan_StorageDevice*** OutStorageDevices, size_t* OutStorageDevicesNum);
void DriverMan_StorageReadFromDevice(DriverMan_StorageDevice* Device, size_t NumSectors, uint32_t LBA, void* Dest);
void DriverMan_StorageWriteToDevice(DriverMan_StorageDevice* Device, size_t NumSectors, uint32_t LBA, void* Src);

void DriverMan_RegisterFilesysDriver(DriverMan_FilesystemDriver* Driver);
DriverMan_FilesystemDriver* DriverMan_GetFilesysDriver(const char* Name);
void DriverMan_GetFilesysDrivers(DriverMan_FilesystemDriver*** OutFilesysDrivers, size_t* OutFilesysDriversNum);
void DriverMan_FilesysFormat(DriverMan_StorageDevice* Device, char* FSName);
bool DriverMan_FilesysTryFormat(DriverMan_StorageDevice* Device, char* FSName); // Returns true if ended up formatting
bool DriverMan_FilesysIsFormatted(DriverMan_StorageDevice* Device);
bool DriverMan_FilesysMakeFile(DriverMan_StorageDevice* Device, char* File);
bool DriverMan_FilesysMakeDir(DriverMan_StorageDevice* Device, char* Dir);
bool DriverMan_FilesysRemove(DriverMan_StorageDevice* Device, char* AnyDir);
bool DriverMan_FilesysWriteFile(DriverMan_StorageDevice* Device, char* File, void* Data, size_t Bytes);
void* DriverMan_FilesysReadFile(DriverMan_StorageDevice* Device, char* File, size_t* BytesRead);
size_t DriverMan_FilesysFileSize(DriverMan_StorageDevice* Device, char* File);
FileListEntry* DriverMan_FilesysListFiles(DriverMan_StorageDevice* Device, char* Dir, size_t* NumFiles);

#endif // DRIVER_MAN_H