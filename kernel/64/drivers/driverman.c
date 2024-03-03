#include "driverman.h"
#include "../vmalloc.h"
#include "../memtools.h"

static DriverMan_Manager Manager;

void DriverMan_RegisterStorageDriver(DriverMan_StorageDriver* Driver)
{

    Manager.StorageDrivers[Manager.NumStorageDrivers++] = Driver;
    DriverMan_StorageDriver** NewDrivers = AllocVM(sizeof(DriverMan_StorageDriver*) * (Manager.NumStorageDrivers + 1));
    memcpy(NewDrivers, Manager.StorageDrivers, sizeof(DriverMan_StorageDriver*) * Manager.NumStorageDrivers);
    FreeVM(Manager.StorageDrivers);
    Manager.StorageDrivers = NewDrivers;
    
    Manager.StorageDriversEnabled[Manager.NumStorageDrivers - 1] = true;
    bool* NewDriversEnabled = AllocVM(sizeof(bool) * (Manager.NumStorageDrivers + 1));
    memcpy(NewDriversEnabled, Manager.StorageDriversEnabled, Manager.NumStorageDrivers);
    FreeVM(Manager.StorageDriversEnabled);
    Manager.StorageDriversEnabled = NewDriversEnabled;
}

void DriverMan_Init()
{
    Manager.NextIRQ = 70;

    Manager.StorageDevices = 0;
    Manager.NumStorageDevices = 0;
    Manager.StorageDrivers = AllocVM(sizeof(DriverMan_StorageDriver*));
    Manager.NumStorageDrivers = 0;
    Manager.StorageDriversEnabled = AllocVM(sizeof(bool));

    DriverMan_RegisterStorageDriver(NVMe_GetDriver());
    DriverMan_RegisterStorageDriver(IDE_GetDriver()); 

    for (int i = 0;i < Manager.NumStorageDrivers;i++)
    {
        Manager.StorageDrivers[i]->IRQ = Manager.NextIRQ++;
        Manager.StorageDrivers[i]->DriverInit(Manager.StorageDrivers[i]);
        
        size_t OriginalNum = Manager.NumStorageDevices;
        Manager.NumStorageDevices += Manager.StorageDrivers[i]->NumDevices;
        DriverMan_StorageDevice** NewDevices = AllocVM(sizeof(DriverMan_StorageDevice*) * Manager.NumStorageDevices);
        memcpy(NewDevices, Manager.StorageDevices, sizeof(DriverMan_StorageDevice*) * OriginalNum);
        memcpy(NewDevices + OriginalNum, Manager.StorageDrivers[i]->Devices, sizeof(DriverMan_StorageDevice*) * Manager.StorageDrivers[i]->NumDevices);
        FreeVM(Manager.StorageDevices);
        Manager.StorageDevices = NewDevices;
    }
}

void DriverMan_Run()
{
    FreeVM(Manager.StorageDevices);
    Manager.StorageDevices = 0;
    Manager.NumStorageDevices = 0;

    for (int i = 0;i < Manager.NumStorageDrivers;i++)
    {
        Manager.StorageDrivers[i]->DriverRun(Manager.StorageDrivers[i]);

        size_t OriginalNum = Manager.NumStorageDevices;
        Manager.NumStorageDevices += Manager.StorageDrivers[i]->NumDevices;
        DriverMan_StorageDevice** NewDevices = AllocVM(sizeof(DriverMan_StorageDevice*) * Manager.NumStorageDevices);
        memcpy(NewDevices, Manager.StorageDevices, sizeof(DriverMan_StorageDevice*) * OriginalNum);
        memcpy(NewDevices + OriginalNum, Manager.StorageDrivers[i]->Devices, sizeof(DriverMan_StorageDevice*) * Manager.StorageDrivers[i]->NumDevices);
        FreeVM(Manager.StorageDevices);
        Manager.StorageDevices = NewDevices;
    }
}