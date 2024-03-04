#ifndef DRIVER_MAN_H
#define DRIVER_MAN_H

#include "../../include.h"

typedef struct
{
    const char* Name;
    uint32_t Version; // Low 16 bits: Minor version, High 16 bits: Major version
} DriverMan_DriverHeader;

struct _DriverMan_StorageDriver;

typedef struct
{
    struct _DriverMan_StorageDriver* ParentDriver; // The driver that controls the device
    size_t MyID; // The index of the device in it's parent driver's device list
    size_t MyUID; // The unique identifier of this device (not unique across different drivers), useful for checking if the device is still in the parent driver's devices list
    size_t MaxReadSectorCount; // When performing a read operation, the amount of sectors to read can never be above this value
    size_t MaxWriteSectorCount; // When performing a write operation, the amount of sectors to read can never be above this value
    size_t Capacity; // Size of the storage device in bytes
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
    // void(*DriverFree)(struct _DriverMan_StorageDriver* MyDriver); // This function is called when it has been requested that this driver shuts down
    void(*DriverRun)(struct _DriverMan_StorageDriver* MyDriver); // This function runs repeatedly so that the driver can do things like refresh the connected devices
    bool(*StorageRead)(struct _DriverMan_StorageDriver* MyDriver, int DeviceIdx, size_t NumSectors, uint32_t LBA, void* Dest);
    bool(*StorageWrite)(struct _DriverMan_StorageDriver* MyDriver, int DeviceIdx, size_t NumSectors, uint32_t LBA, void* Src);
} DriverMan_StorageDriver;

typedef struct _DriverMan_Manager
{
    DriverMan_StorageDevice** StorageDevices;
    size_t NumStorageDevices;
    DriverMan_StorageDriver** StorageDrivers;
    size_t NumStorageDrivers;
    bool* StorageDriversEnabled;

    size_t NextIRQ;
} DriverMan_Manager;

void DriverMan_Init();
void DriverMan_Run();

void DriverMan_RegisterStorageDriver(DriverMan_StorageDriver* Driver);
void DriverMan_GetStorageDrivers(DriverMan_StorageDriver*** OutStorageDrivers, size_t* OutStorageDriversNum);
void DriverMan_GetStorageDevices(DriverMan_StorageDevice*** OutStorageDevices, size_t* OutStorageDevicesNum);
void DriverMan_StorageReadFromDevice(DriverMan_StorageDevice* Device, size_t NumSectors, uint32_t LBA, void* Dest);
void DriverMan_StorageWriteToDevice(DriverMan_StorageDevice* Device, size_t NumSectors, uint32_t LBA, void* Src);

#endif // DRIVER_MAN_H