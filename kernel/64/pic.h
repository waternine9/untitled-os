#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void PicInit();
void PicSetMask(uint16_t Mask);
uint16_t PicGetMask();
void PicEndOfInterrupt(uint8_t Interrupt);

#endif // PIC_H