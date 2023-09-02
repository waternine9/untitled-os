#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../include.h"

#define KEYBOARD_QUEUE_SIZE 32


#define KEY_ESC       0x01
#define KEY_BACKSPACE 0x0E
#define KEY_LCTRL     0x1D
#define KEY_RCTRL     0x1D+128
#define KEY_LSHIFT    0x2A
#define KEY_RSHIFT    0x36
#define KEY_FKLSHIFT  0x2A+128
#define KEY_FKRSHIFT  0x36+128
#define KEY_CAPS_LOCK 0x3A
#define KEY_LALT      0x38
#define KEY_RALT      0x38+128
#define KEY_LEFT      0x4B+128
#define KEY_RIGHT     0x4D+128
#define KEY_UP        0x48+128
#define KEY_DOWN      0x50+128

typedef struct {
    uint8_t CapsLock;
    uint8_t LShift;
    uint8_t RShift;
    uint8_t LCtrl;
    uint8_t RCtrl;
    uint8_t LAlt;
    uint8_t RAlt;
} Keyboard;

typedef struct {
    uint8_t Valid;
    uint8_t CapsLock;
    uint8_t LShift;
    uint8_t RShift;
    uint8_t LCtrl;
    uint8_t RCtrl;
    uint8_t LAlt;
    uint8_t RAlt;
    uint8_t Shift, Ctrl, Alt;
    uint8_t Released;
    uint8_t Scancode;
    uint8_t ASCII;
    uint8_t ASCIIOriginal;
} KeyboardKey;

extern KeyboardKey* KeyEventQueue;
extern size_t KeyEventQueueIdx;
extern Keyboard* KeyboardState;

void StartKeyboard();
KeyboardKey KeyboardPollKey();

#endif // KEYBOARD_H