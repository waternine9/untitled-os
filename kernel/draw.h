#include "include.h"

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    uint32_t col;
} Draw_RectEntry;

typedef struct {
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    uint32_t col;
} Draw_LineEntry;

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t col;
} Draw_PointEntry;

typedef struct {
    const char* text;
    int32_t x;
    int32_t y;
    uint32_t col;
} Draw_TextEntry;

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t* image; // ARGB
} Draw_ImageEntry;

typedef struct {
    Draw_RectEntry* rects;
    size_t rectsSize;
    Draw_LineEntry* lines;
    size_t linesSize;
    Draw_PointEntry* points;
    size_t pointsSize;
    Draw_TextEntry* texts;
    size_t textsSize;
    Draw_ImageEntry* images;
    size_t imagesSize;
} Draw_Packet;

typedef struct {
    uint64_t Width;
    uint64_t Height;
} Draw_Info;

