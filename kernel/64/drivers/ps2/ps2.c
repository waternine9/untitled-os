#include "ps2.h"
#include "../../io.h"

void MouseWait()
{
    uint64_t timeout = 100000;
    while (timeout--){
        if ((IO_In8(0x64) & 0b10) == 0){
            return;
        }
    }
}

void MouseWaitInput()
{
    uint64_t timeout = 100000;
    while (timeout--){
        if (IO_In8(0x64) & 0b1){
            return;
        }
    }
}

void MouseWrite(uint8_t AWrite)
{
    IO_Out8(0x64, 0xD4);
    IO_Wait();
    IO_Out8(0x60, AWrite);
    IO_Wait();
}

uint8_t MouseRead()
{
    IO_Wait();
    return IO_In8(0x60);
}

void PS2Install()
{
    IO_Out8(0x64, 0xAD);
    MouseWait();
    IO_Out8(0x64, 0xA7);
    MouseWaitInput();
    IO_Out8(0x64, 0x20);
    MouseWait();
    uint8_t status = IO_In8(0x60) | 3;
    MouseWait();
    IO_Out8(0x64, 0x60);
    MouseWait();
    IO_Out8(0x60, status);
    MouseWrite(0xF6);
    MouseRead();
    MouseWrite(0xF4);
    MouseRead();

    IO_Out8(0x64, 0xAE);
    MouseWait();
    IO_Out8(0x64, 0xA8);
    MouseWait();
}