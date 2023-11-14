#include "../include.h"
#include "vmalloc.h"
#include "idt.h"
#include "pic.h"
#include "apic.h"
#include "softtss.h"
#include "../vbe.h"
#include "pci.h"

// DRIVER INCLUDES
#include "drivers/ps2/ps2.h"
#include "drivers/fs/fs.h"
#include "drivers/ide/ide.h"
#include "drivers/nvme/nvme.h"


// I know, putting the example code here isnt the best idea

// static const char* ExampleCode = "int:cos(int:x)"
// "{"
// "    int:modX = x % (1 << 10);"
// "    int:modX2 = x % (2 << 10);"
// "    int:result = 4 * (modX - ((modX * modX) >> 10));"
// "    if (modX2 > (1 << 10))"
// "    {"
// "        return(result * ~1);"
// "    };"
// "    return(result);"
// "}"
// ""
// "int:sin(int:x)"
// "{"
// "    x = x + (3 << 9);"
// "    return(cos(x));"
// "}"
// ""
// "int:Main()"
// "{"
// "    int[1600]:framebuff;"
// "    int[1600]:depthbuff;"
// "    int:B = 0;"
// "    while (1 < 2)"
// "    {"
// "        for (int:__Count = 0;__Count < 1600;__Count = __Count + 1)"
// "        {"
// "            framebuff[__Count] = 0;"
// "            depthbuff[__Count] = ~1000000;"
// "        };"
// "        for (int:theta = 0;theta < 2000;theta = theta + 4)"
// "        {"
// "            for (int:phi = 0;phi < 2000;phi = phi + 4)"
// "            {"
// "                "
// "                int:nx1 = cos(phi);"
// "                int:ny1 = sin(phi);"
// "                int:y1 = (ny1 * 5) + (10 << 10);"
// "                int:z1 = nx1 * 5;"
// "                int:x1 = 0;"
// ""
// "                int:rx1 = ((x1 * cos(theta)) >> 10) - ((y1 * sin(theta)) >> 10); "
// "                int:ry1 = ((x1 * sin(theta)) >> 10) + ((y1 * cos(theta)) >> 10); "
// "                ny1 = ((ny1 * cos(theta)) >> 10); "
// ""
// "                int:ry2 = ((z1 * sin(A)) >> 10) + ((ry1 * cos(A)) >> 10); "
// "                int:rz2 = ((z1 * cos(A)) >> 10) - ((ry1 * sin(A)) >> 10); "
// "                "
// "                int:ry3 = ((rx1 * sin(B)) >> 10) + ((ry2 * cos(B)) >> 10); "
// "                int:rx3 = ((rx1 * cos(B)) >> 10) - ((ry2 * sin(B)) >> 10); "
// ""
// "                if (depthbuff[(20 + rx3 >> 10) + 40 * (20 + ry3 >> 10)] < rz2)"
// "                {"
// "                    depthbuff[(20 + rx3 >> 10) + 40 * (20 + ry3 >> 10)] = rz2;"
// "                    if (ny1 < 0)"
// "                    {"
// "                        ny1 = 1 << 7;"
// "                    };"
// "                    framebuff[(20 + rx3 >> 10) + 40 * (20 + ry3 >> 10)] = ny1 >> 7;"
// "                        "
// "                };"
// "            };"
// "        };"
// "        for (int:y = 0;y < 40;y = y + 1)"
// "        {"
// "            for (int:x = 0;x < 40;x = x + 1)"
// "            {"
// "                if (framebuff[x + y * 40] > 1)"
// "                {"
// "                    print(48 + framebuff[x + y * 40]);"
// "                };"
// "                if (framebuff[x + y * 40] == 0)"
// "                {"
// "                    print(32);"
// "                };"
// "                if (framebuff[x + y * 40] == 1)"
// "                {"
// "                    print(95);"
// "                };"
// "            };"
// "            print(10);"
// "        };"
// "        A = A + 40;"
// "        B = B + 16;"
// "        print(10);"
// "    };"
// "};";
static const char* ExampleCode = "int:Main()\n"
"{\n"
"    int:scale = 7;\n"
"    for (int:i = 1;i < scale + 1;i = i + 1)\n"
"    {\n"
"        for (int:j = 0;j < scale + 1;j = j + 1)\n"
"        {\n"
"            print(48 + j);\n"
"        };\n"
"        print(10);\n"
"    };\n"
"}\n";
extern SoftTSS* SchedRing;
extern int SchedRingSize;
extern int SchedRingIdx;

extern int SuspendPIT;

extern void LoadIDT();

volatile uint64_t __attribute__((section(".main64"))) main64()
{
    for (uint64_t i = 0;i < 0x800000;i += 1)
    {
        *(uint8_t*)(0xFFFFFFFFC1200000 + i) = 0;
    }

    AllocInit();


    VesaVbeModeInfo* VbeModeInfo = VBE_INFO_LOC;

    if (!AllocMap(0xFFFFFFFF90000000, VbeModeInfo->Framebuffer, (VbeModeInfo->Bpp / 8) * VbeModeInfo->Width * VbeModeInfo->Height, (1 << MALLOC_READWRITE_BIT) | (1 << MALLOC_WRITE_THROUGH_BIT)));
    //AllocMap(0xFFFFFFFF90000000, 0xb8000, 80 * 25 * 2, (1 << MALLOC_READWRITE_BIT));

    *(uint32_t*)0xFFFFFFFF9000000E = 0xFFFFFFFF;

    AllocIdMap(0xB00000, 0x100000, (1ULL << MALLOC_READWRITE_BIT) | (1ULL << MALLOC_USER_SUPERVISOR_BIT));

    SchedRing = AllocVM(sizeof(SoftTSS) * 128);
    SchedRing[0].Privilege = 1;
    SchedRing[0].Suspended = 0;
    SchedRing[0].SuspendIdx = 0;
    SchedRingSize = 1;
    SchedRingIdx = 0;

    PicInit();
    PicSetMask(0xFFFF);
    SuspendPIT = 1;
    IdtInit();
    LoadIDT();
    ApicInit();
    NVMEInit();

    FSFormat();

    FSMkdir("home");
    FSMkdir("bin");
    FSMkdir("etc");

    FSMkfile("home/helloworld.bf");

    char* Buf = AllocPhys(0x1000);

    for (int i = 0;i < 0x1000;i++) Buf[i] = 0;
    for (int i = 0;ExampleCode[i];i++) Buf[i] = ExampleCode[i];

    FSWriteFile("home/helloworld.bf", Buf, 0xa00);

    if (AllocVMAtStack(0xC00000, 0x100000) == 0) asm volatile ("cli\nhlt" :: "a"(0x2454));
    
    

    SuspendPIT = 0;
    return 0xB00000;
}