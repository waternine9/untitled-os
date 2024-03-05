#include "panic.h"
#include "../vbe.h"
#include "draw.h"
#include <stdarg.h>

static const char LowercaseHexListing[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', 
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
};

static const char UppercaseHexListing[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', 
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
};

/*
 * Returns true if base passed exceeded the base limit or is less than two
 *   without trying to write into the string.
 * Otherwise prints the number `num` into `dest` using base `base`.
 *   the function is guaranteed to write a finishing nul-terminator.
 */
static bool StrU(uint64_t Num, uint64_t Base, bool DoUpper, char *Dest)
{
    if (Base < 2) {
        return true;
    }
    if (Base > sizeof(UppercaseHexListing)) {
        return true;
    }

    // Find the number of digits in the number
    uint64_t NumDigits = 0;
    {
        uint64_t x = Num;
        do {
            x /= Base;
            NumDigits += 1;
        } while (x != 0);
    }

    uint64_t Index = NumDigits;

    // Write the nul terminator
    Dest[Index] = 0;
    Index -= 1;

    // Write the remaining digits
    uint64_t x = Num;
    do {
        uint64_t Digit = x % Base;
        Dest[Index] = DoUpper 
            ? UppercaseHexListing[Digit]
            : LowercaseHexListing[Digit];
        x /= Base;
        Index -= 1;
    } while (x != 0);
    
    return false;
}

/*
 * Same as above, but can print negative numbers
 */
static bool StrI(int64_t Num, uint64_t Base, bool DoUpper, char *Dest)
{
    if (Base < 2) {
        return true;
    }
    if (Base > sizeof(UppercaseHexListing)) {
        return true;
    }

    uint64_t AbsNum;
    if (Num >= 0) {
        AbsNum = (uint64_t)Num;
    } else {
        AbsNum = (uint64_t)(-Num);
    }

    // Find the number of digits in the number
    uint64_t NumDigits = 0;
    {
        uint64_t x = AbsNum;
        do {
            x /= Base;
            NumDigits += 1;
        } while (x != 0);
    }

    uint64_t Index = NumDigits;
    if (Num < 0) {
        Index += 1;
    }

    // Write the nul terminator
    Dest[Index] = 0;
    Index -= 1;

    // Write the remaining digits
    uint64_t x = AbsNum;
    do {
        uint64_t digit = x % Base;
        Dest[Index] = DoUpper 
            ? UppercaseHexListing[digit]
            : LowercaseHexListing[digit];
        x /= Base;
        Index -= 1;
    } while (x != 0);

    // Write the negative sign if needed
    if (Num < 0) {
        Dest[Index] = '-';
    }
    
    return false;
}

static size_t ParseFmt(char *Dest, size_t WrittenThusFar, size_t *i, const char *Format, va_list Args) {
    *i += 1;
    // Temp buffer for numbers and misc
    char Temp[64] = { 0 };
    size_t WrittenCount = 0;
    switch (Format[*i]) {
        case 's': {
            const char *Arg = va_arg(Args, const char*);
            size_t i = 0;
            for (; Arg[i]; i += 1) {
                Dest[WrittenThusFar + i] = Arg[i];
            }
            WrittenCount += i;
            return WrittenCount;
        }
        break;
        case 'o':
            StrU((size_t)va_arg(Args, unsigned int), 8, false, Temp); 
        break;
        case 'i':
        case 'd':
            StrI((int32_t)va_arg(Args, int), 10, false, Temp); 
        break;
        case 'u':
            StrU((size_t)va_arg(Args, unsigned int), 10, false, Temp); 
        break;
        case 'X':
        case 'x':
            StrU((size_t)va_arg(Args, unsigned int), 16, Format[*i] == 'X', Temp); 
        break;
        case 'c':
            Temp[0] = (char)va_arg(Args, int);
        break;
        case 'n':
            *va_arg(Args, int*) = (int)(WrittenThusFar);
        break;
        case 'p':
            StrU((size_t)va_arg(Args, void*), 16, false, Temp);
        break;
        case '%':
        default:
            Temp[0] = '%';
        break;
    }

    {
        size_t i = 0;
        for (; Temp[i]; i += 1) {
            Dest[WrittenThusFar + i] = Temp[i];
        }
        WrittenCount += i;
    }
    return WrittenCount;
}

void FormatStringVaList(char *Out, const char *Format, va_list Args)
{
    size_t WrittenCount = 0;
    for (size_t i = 0; Format[i]; i += 1) {
        if (Format[i] == '%') {
            WrittenCount += ParseFmt(Out, WrittenCount, &i, Format, Args);
        }
        else {
            *Out = Format[i];
            WrittenCount += 1;
        }
    }
    return WrittenCount;
}

void _KernelPanic(int Line, const char* File, const char* Message, ...)
{
    va_list Args;
    va_start(Args, Message);
    char Buffer[4096] = { 0 };
    FormatStringVaList(Buffer, Message, Args);
    va_end(Args);

    Draw_RectEntry Rect;
    Rect.x = 0;
    Rect.y = 0;
    Rect.w = Draw_GetResX();
    Rect.x = Draw_GetResY();
    Rect.col = 0;
    Draw_Rect(Rect);
    Draw_TextEntry Text;
    Text.col = 0xFFFFFFFF;
    Text.x = 10;
    Text.y = 10;
    Text.text = Message;
    Draw_Text(Text);
    Text.x = 10;
    Text.y = 20;
    Text.text = File;
    Draw_Text(Text);
    asm volatile ("cli\nhlt");
}