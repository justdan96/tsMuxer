#ifndef __CRC_H
#define __CRC_H

typedef uint32_t AVCRC;

int av_crc_init(AVCRC *ctx, int le, int bits, uint32_t poly, unsigned ctx_size);
uint32_t av_crc(const AVCRC *ctx, uint32_t crc, const uint8_t *buffer, size_t length);

#endif /* __CRC_H */

