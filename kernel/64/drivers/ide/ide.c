#include "ide.h"

extern uint64_t _ReadAta(uint64_t LBA, void* Out, uint64_t IOStart, uint64_t IsSlave);
extern uint64_t _AtaAvailable(uint64_t LBA, uint64_t IOStart, uint64_t IsSlave);

static uint64_t BootDriveStart = 0;
static uint64_t BootDriveMS = 0;

uint8_t IDEInit()
{
    char* TestBuf = 0x7B00;
    if (_ReadAta(0, TestBuf, 0x1f0, 0))
    {
        for (int i = 0;i < 40;i++)
        {
            if (TestBuf[i] == 'U' && TestBuf[i + 1] == 'N')
            {
                BootDriveStart = 0x1f0;
                BootDriveMS = 0;
                return 1;
            }
        }
    }
    if (_ReadAta(0, TestBuf, 0x1f0, 1))
    {
        for (int i = 0;i < 40;i++)
        {
            if (TestBuf[i] == 'U' && TestBuf[i + 1] == 'N')
            {
                BootDriveStart = 0x1f0;
                BootDriveMS = 1;
                return 1;
            }
        }
    }
    if (_ReadAta(0, TestBuf, 0x170, 0))
    {
        for (int i = 0;i < 40;i++)
        {
            if (TestBuf[i] == 'U' && TestBuf[i + 1] == 'N')
            {
                BootDriveStart = 0x170;
                BootDriveMS = 0;
                return 1;
            }
        }
    }
    if (_ReadAta(0, TestBuf, 0x170, 1))
    {
        for (int i = 0;i < 40;i++)
        {
            if (TestBuf[i] == 'U' && TestBuf[i + 1] == 'N')
            {
                BootDriveStart = 0x170;
                BootDriveMS = 1;
                return 1;
            }
        }
    }
    return 0;
}

void IDEReadBoot(uint64_t LBA, void* Out);