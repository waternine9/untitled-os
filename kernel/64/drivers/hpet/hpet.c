#include "hpet.h"
#include "../../vmalloc.h"

typedef struct {
    uint64_t RevID:8;
    uint64_t TimerCount:5;
    uint64_t CounterSize:1;
    uint64_t Reserved:1;
    uint64_t LegacyReplacement:1;
    uint64_t VendorID:16;
    uint64_t CounterPeriod:32;
} __attribute__((packed)) HPET_RegCapabilites;

typedef struct {
    uint64_t EnableCnf:1;
    uint64_t LegacyRouteCap:1;
    uint64_t Reserved:62;
} __attribute__((packed)) HPET_RegConfig;

typedef struct {
    uint64_t Timer0_InterruptStatus:1;
    uint64_t Timer1_InterruptStatus:1;
    uint64_t Timer2_InterruptStatus:1;
    uint64_t Reserved:62;
} __attribute__((packed)) HPET_RegInterrupt;

typedef struct {
    uint64_t Reserved0:1;
    uint64_t InterruptType:1;
    uint64_t InterruptEnable:1;
    uint64_t TimerType:1;
    uint64_t TimerPeriodicCapable:1;
    uint64_t TimerSize:1;
    uint64_t TimerValueSet:1;
    uint64_t Reserved1:1;
    uint64_t TimerInterruptNo:5;
    uint64_t TimerFSBEnable:1;
    uint64_t TimerFSBCapable:1;
    uint64_t Reserved2:17;
    uint64_t TimerInterruptCapabilities:32; // which interrupts can be routed to this timer (bitmap)
} __attribute__((packed)) HPET_TimerConfig;

typedef struct {
    uint64_t TimerValue;
} __attribute__((packed)) HPET_ComparatorValue;

typedef struct {
    uint32_t FSBValue; // what value to write to addr
    uint32_t FSBAddr; // what address to write to
} __attribute__((packed)) HPET_TimerComparator;

typedef struct {
    HPET_TimerConfig Config;
    uint64_t ComparatorValue;
    uint64_t ComparatorFSBValue;
    uint64_t Reserved;
} __attribute__((packed)) HPET_Timer;

typedef struct {
    HPET_RegCapabilites Capabilites;
    uint64_t Reserved0;
    HPET_RegConfig Config;
    uint64_t Reserved1;
    HPET_RegInterrupt Interrupt;
    uint64_t Reserved2;
    uint64_t MainCounterValue;
    uint64_t Reserved3;
    HPET_Timer Timers[32];
} __attribute__((packed)) HPET;

HPET *HPET_State;
static uint64_t HPET_Freq;
static uint16_t HPET_MinTick;


bool HPETInit()
{
    HPET_Table *HPET_TBL = ACPIFindTable("HPET");

    if (HPET_TBL)
    {
        HPET_State = (HPET*)HPET_TBL->address;
        AllocVMAtFlags(HPET_TBL->address, 0x1000, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_CACHE_DISABLE_BIT));

        HPET_Freq = 1e15 / HPET_State->Capabilites.CounterPeriod;
        HPET_MinTick = HPET_TBL->minimum_tick;
        HPET_RegConfig Config = HPET_State->Config;
        Config.EnableCnf = 1;
        HPET_State->Config = Config;
        

        // TODO: Find periodic capable timer instead of checking just the first one
        HPET_Timer timer = HPET_State->Timers[2];
        timer.Config.TimerInterruptNo = 15;
        timer.Config.InterruptEnable = 1;
        if (!timer.Config.TimerPeriodicCapable)
        {
            return false;
        }
        timer.Config.TimerType = 1;
        timer.Config.TimerValueSet = 1;
        timer.ComparatorValue = HPET_State->MainCounterValue + 10000000;
        HPET_State->Timers[2] = timer;
    }

    return HPET_TBL;
}
