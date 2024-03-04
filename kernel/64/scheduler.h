#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../include.h"
#include "softtss.h"

void Scheduler_Init();
// Returns pointer to next program's TSS. Doesnt immediately switch to the next process.
SoftTSS* Scheduler_NextProcess(SoftTSS* SaveState);
SoftTSS* Scheduler_EndCurrentProcess(bool SkipToStart);
SoftTSS* Scheduler_AddProcess(uint64_t rip, uint64_t Size);
SoftTSS* Scheduler_AddSyscallProcess(uint64_t rip, uint64_t Code, uint64_t rsi, uint64_t Selector);

#endif // SCHEDULER_H