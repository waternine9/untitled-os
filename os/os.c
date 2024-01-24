#include "include.h"
#include "log.h"
#include "proc.h"
#include "filecall.h"
#include "mem.h"
#include "timer.h"

// MODULE INCLUDES
#include "modules/winman.h"
#include "modules/keyboard.h"

volatile void __attribute__((section(".startos"))) main()
{
    for (int i = 0;i < 0x50000;i++)
    {
        *(volatile uint8_t*)(0xB50000 + i) = 0;
    }

    Log("Starting keyboard...", LOG_INFO);
    StartProc((uint64_t)StartKeyboard, 0x40000ULL | 0x100000000ULL);

    Log("Starting window manager...", LOG_INFO);
    StartProc((uint64_t)StartWindowManager, 0x40000ULL | 0x100000000ULL);

    Log("Welcome to this OS!", LOG_SUCCESS);

    while (1)
    {

    }
}