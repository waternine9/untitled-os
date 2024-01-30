#ifndef IOMEM_H
#define IOMEM_H
#include <stdint.h>

#define IOMEM_READ_VALUE(dest, src)                                            \
  Iomem_ReadAligned32String((dest), (src), sizeof(*(dest)))

#define IOMEM_WRITE_VALUE(dest, src)                                           \
  Iomem_WriteAligned32String((dest), (src), sizeof(*(dest)))

static inline uint8_t Iomem_ReadAligned32(void *base) {
  size_t addr = (size_t)base;
  size_t offset = addr & 0b11;

  return ((*(uint32_t *)(addr & (~0b11))) >> (offset * 8)) & 0xFF;
}

static inline void Iomem_WriteAligned32(void *base, uint8_t value) {
  size_t addr = (size_t)base;
  size_t offset = addr & 0b11;

  uint32_t existing = *(uint32_t *)(addr & (~0b11));
  ((uint8_t *)&existing)[offset] = value;
  *(uint32_t *)(addr & (~0b11)) = existing;
}

static inline uint8_t Iomem_ReadAligned32String(void *out, void *in,
                                                size_t size) {
  for (size_t i = 0; i < size; i++) {
    ((uint8_t *)out)[i] = Iomem_ReadAligned32((uint8_t *)in + i);
  }
}

static inline void Iomem_WriteAligned32String(void *out, void *in,
                                              size_t size) {
  for (size_t i = 0; i < size; i++) {
    Iomem_WriteAligned32((uint8_t *)out + i, ((uint8_t *)in)[i]);
  }
}

#endif