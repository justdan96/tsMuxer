#include "bitStream.h"

#include <cstdint>

void updateBits(const BitStreamReader& bitReader, int bitOffset, int bitLen, int value)
{
    updateBits(bitReader.getBuffer(), bitOffset, bitLen, value);
}

void updateBits(const uint8_t* buffer, int bitOffset, int bitLen, int value)
{
    uint8_t* ptr = (uint8_t*)buffer + bitOffset / 8;
    BitStreamWriter bitWriter{};
    const int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    const uint8_t* ptr_end = (uint8_t*)buffer + (bitOffset + bitLen) / 8;
    const int endBitsPostfix = 8 - ((bitOffset + bitLen) % 8);

    if (byteOffset > 0)
    {
        const int prefix = *ptr >> (8 - byteOffset);
        bitWriter.putBits(byteOffset, prefix);
    }
    bitWriter.putBits(bitLen, value);

    if (endBitsPostfix < 8)
    {
        const int postfix = *ptr_end & (1 << endBitsPostfix) - 1;
        bitWriter.putBits(endBitsPostfix, postfix);
    }
    bitWriter.flushBits();
}

void moveBits(uint8_t* buffer, int oldBitOffset, int newBitOffset, int len)
{
    uint8_t* src = (uint8_t*)buffer + (oldBitOffset >> 3);
    BitStreamReader reader{};
    reader.setBuffer(src, src + len / 8 + 1);
    uint8_t* dst = (uint8_t*)buffer + (newBitOffset >> 3);
    BitStreamWriter writer{};
    writer.setBuffer(dst, dst + len / 8 + 1);
    writer.skipBits(newBitOffset % 8);
    if (oldBitOffset % 8)
    {
        reader.skipBits(oldBitOffset % 8);
        const int c = 8 - (oldBitOffset % 8);
        writer.putBits(c, reader.getBits(c));
        len -= c;
        src++;
    }
    for (; len >= 8 && (reinterpret_cast<std::uintptr_t>(src) % sizeof(unsigned)) != 0; len -= 8)
    {
        writer.putBits(8, *src);
        src++;
    }

    for (; len >= (int)INT_BIT; len -= INT_BIT)
    {
        writer.putBits(INT_BIT, *((unsigned*)src));
        src += sizeof(unsigned);
    }
    reader.setBuffer(src, src + sizeof(unsigned));
    writer.putBits(len, reader.getBits(len));
    writer.flushBits();
}
