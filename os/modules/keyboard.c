#include "keyboard.h"
#include "../log.h"
#include "../mem.h"

KeyboardKey* KeyEventQueue = 0;
size_t KeyEventQueueIdx = 0;
Keyboard* KeyboardState = 0;

extern char PollScancode(char* Out);

const char TransLo[128] = 
{
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, '\t', 
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's', 
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 
    'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '
};

const char TransHi[128] = 
{
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, '\t', 
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S', 
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V', 
    'B', 'N', 'M', '<', '>', '?', 0, 0, 0, ' '
};

uint8_t TransformScancode(char Scancode) {
    if (Scancode >= 128) return 0;
    uint8_t tl = TransLo[Scancode];
    
    if (KeyboardState->LShift || KeyboardState->RShift) 
    {
        if (KeyboardState->CapsLock && tl >= 'a' && tl <= 'z') 
        {
            return tl;
        }
        return TransHi[Scancode];
    }
    if (KeyboardState->CapsLock && tl >= 'a' && tl <= 'z') 
    {
        return TransHi[Scancode];
    }
    return TransLo[Scancode];
}

static void ProcessScancode(char Scancode)
{
    uint8_t Alternative = 0;
    
    if (Scancode == 0xE0) 
    {
        Alternative = 1;
        PollScancode(&Scancode);
    }
    
    uint8_t Released = Scancode >= 128;
    if (Released) Scancode -= 128;
    if (Alternative) Scancode += 128;

    switch (Scancode) 
    {
        case KEY_LSHIFT: case KEY_FKLSHIFT:
            KeyboardState->LShift = !Released;
            break;
        case KEY_RSHIFT: case KEY_FKRSHIFT:
            KeyboardState->RShift = !Released;
            break;
        case KEY_LCTRL:       KeyboardState->LCtrl = !Released; break;
        case KEY_RCTRL:       KeyboardState->RCtrl = !Released; break;
        case KEY_LALT:        KeyboardState->LAlt = !Released; break;
        case KEY_RALT:        KeyboardState->RAlt = !Released; break;
        case KEY_CAPS_LOCK:   if (!Released) KeyboardState->CapsLock = KeyboardState->CapsLock; break;
    
        case 0: return;
    }


    KeyEventQueue[KeyEventQueueIdx++] = (KeyboardKey) 
    {
        1,
        KeyboardState->CapsLock,
        KeyboardState->LShift,
        KeyboardState->RShift,
        KeyboardState->LCtrl,
        KeyboardState->RCtrl,
        KeyboardState->LAlt,
        KeyboardState->RAlt,
        KeyboardState->LShift || KeyboardState->RShift,
        KeyboardState->LCtrl || KeyboardState->RCtrl,
        KeyboardState->LAlt || KeyboardState->RAlt,
        Released, Scancode,
        TransformScancode(Scancode),
        Scancode < 128 ? TransLo[Scancode] : 0
    };
    if (KeyEventQueueIdx >= KEYBOARD_QUEUE_SIZE) KeyEventQueueIdx = KEYBOARD_QUEUE_SIZE - 1;
    Alternative = 0;
}

void StartKeyboard()
{
    Log("Keyboard started", LOG_SUCCESS);

    KeyEventQueue = (KeyboardKey*)malloc(sizeof(KeyboardKey) * KEYBOARD_QUEUE_SIZE);
    KeyboardState = (Keyboard*)malloc(sizeof(Keyboard));
    KeyEventQueueIdx = 0;
    while (1)
    {
        char Scancode;
        PollScancode(&Scancode);
        if (Scancode != 0)
        {
            ProcessScancode(Scancode);
        }
    }
}

KeyboardKey KeyboardPollKey()
{
    if (KeyEventQueueIdx == 0) return (KeyboardKey){ 0 };
    KeyboardKey Key = KeyEventQueue[0];
    for (int i = 0;i < KEYBOARD_QUEUE_SIZE - 1;i++)
    {
        KeyEventQueue[i] = KeyEventQueue[i + 1];
    }
    KeyEventQueueIdx--;
    return Key;
}