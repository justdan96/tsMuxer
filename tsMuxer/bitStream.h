#ifndef __BITSTREAM_H
#define __BITSTREAM_H

#include <assert.h>
#include <limits.h>
#include <types/types.h>

constexpr static unsigned INT_BIT = CHAR_BIT * sizeof(unsigned);

class BitStreamException : public std::exception
{
   public:
    // BitStreamException(const char* str): std::exception(str) {}
    // BitStreamException(const std::string& str): std::exception(str.c_str()) {}
    BitStreamException() : std::exception() {}
};

//#define THROW_BITSTREAM_ERR throw BitStreamException(std::string(__FILE__) + std::string(" ") +
// std::string(__FUNCTION__) + std::string(" at line ") + int32ToStr(__LINE__))
#define THROW_BITSTREAM_ERR throw BitStreamException()

class BitStream
{
   public:
    inline uint8_t* getBuffer() const { return (uint8_t*)m_initBuffer; }
    inline unsigned getBitsLeft() const { return m_totalBits; }

   protected:
    inline void setBuffer(uint8_t* buffer, uint8_t* end)
    {
        if (buffer >= end)
            THROW_BITSTREAM_ERR;
        m_totalBits = (unsigned)(end - buffer) * 8;
        m_initBuffer = m_buffer = (unsigned*)buffer;
    }
    unsigned m_totalBits;
    unsigned* m_buffer;
    unsigned* m_initBuffer;

    constexpr static unsigned int m_masks[] = {
        0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
        0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff,
        0x0003ffff, 0x0007ffff, 0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 0x01ffffff, 0x03ffffff,
        0x07ffffff, 0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, UINT_MAX};
};

class BitStreamReader : public BitStream
{
   private:
    inline unsigned getCurVal(unsigned* buff)
    {
        auto tmpBuf = (uint8_t*)buff;
        if (m_totalBits - m_bitLeft >= 32)
            return my_ntohl(*buff);
        else if (m_totalBits - m_bitLeft >= 24)
            return (tmpBuf[0] << 24) + (tmpBuf[1] << 16) + (tmpBuf[2] << 8);
        else if (m_totalBits - m_bitLeft >= 16)
            return (tmpBuf[0] << 24) + (tmpBuf[1] << 16);
        else if (m_totalBits - m_bitLeft >= 8)
            return tmpBuf[0] << 24;
        else
            THROW_BITSTREAM_ERR;
    }

   public:
    inline void setBuffer(uint8_t* buffer, uint8_t* end)
    {
        BitStream::setBuffer(buffer, end);
        m_bitLeft = 0;
        m_curVal = getCurVal(m_buffer);
        m_bitLeft = INT_BIT;
    }

    inline unsigned getBits(unsigned num)
    {
        if (num > INT_BIT)
            THROW_BITSTREAM_ERR;
        if (m_totalBits < num)
            THROW_BITSTREAM_ERR;
        unsigned prevVal = 0;
        if (num <= m_bitLeft)
            m_bitLeft -= num;
        else
        {
            if (!(num == INT_BIT && m_bitLeft == 0))
            {
                prevVal = (m_curVal & m_masks[m_bitLeft]) << (num - m_bitLeft);
                m_bitLeft += INT_BIT - num;
            }
            m_buffer++;
            m_curVal = getCurVal(m_buffer);
        }
        m_totalBits -= num;
        return prevVal + (m_curVal >> m_bitLeft) & m_masks[num];
    }

    inline unsigned showBits(unsigned num)
    {
        assert(num <= INT_BIT);
        if (m_totalBits < num)
            THROW_BITSTREAM_ERR;
        unsigned prevVal = 0;
        unsigned bitLeft = m_bitLeft;
        unsigned curVal = m_curVal;
        if (num <= bitLeft)
            bitLeft -= num;
        else
        {
            if (!(num == INT_BIT && bitLeft == 0))
            {
                prevVal = (curVal & m_masks[bitLeft]) << (num - bitLeft);
                bitLeft += INT_BIT - num;
            }
            curVal = getCurVal(m_buffer + 1);
        }
        return prevVal + (curVal >> bitLeft) & m_masks[num];
    }

    inline unsigned getBit()
    {
        if (m_totalBits < 1)
            THROW_BITSTREAM_ERR;
        if (m_bitLeft > 0)
            m_bitLeft--;
        else
        {
            m_buffer++;
            m_curVal = getCurVal(m_buffer);
            m_bitLeft = INT_BIT - 1;
        }
        m_totalBits--;
        return (m_curVal >> m_bitLeft) & 1;
    }

    inline void skipBits(unsigned num)
    {
        if (m_totalBits < num)
            THROW_BITSTREAM_ERR;
        assert(num <= INT_BIT);
        if (num <= m_bitLeft)
            m_bitLeft -= num;
        else
        {
            m_buffer++;
            m_curVal = getCurVal(m_buffer);
            m_bitLeft += INT_BIT - num;
        }
        m_totalBits -= num;
    }

    inline void skipBit()
    {
        if (m_totalBits < 1)
            THROW_BITSTREAM_ERR;
        if (m_bitLeft > 0)
            m_bitLeft--;
        else
        {
            m_buffer++;
            m_curVal = getCurVal(m_buffer);
            m_bitLeft = INT_BIT - 1;
        }
        m_totalBits--;
    }

    inline unsigned getBitsCount() const { return (unsigned)(m_buffer - m_initBuffer) * INT_BIT + INT_BIT - m_bitLeft; }

   private:
    unsigned m_curVal;
    unsigned m_bitLeft;
};

class BitStreamWriter : public BitStream
{
   public:
    inline void setBuffer(uint8_t* buffer, uint8_t* end)
    {
        BitStream::setBuffer(buffer, end);
        m_curVal = 0;
        m_bitWrited = 0;
    }

    inline void skipBits(unsigned cnt)
    {
        assert(m_bitWrited % INT_BIT == 0);
        BitStreamReader reader{};
        reader.setBuffer((uint8_t*)m_buffer, (uint8_t*)(m_buffer + 1));
        putBits(cnt, reader.getBits(cnt));
    }

    inline void putBits(unsigned num, unsigned value)
    {
        if (m_totalBits < num)
            THROW_BITSTREAM_ERR;
        value &= m_masks[num];
        if (m_bitWrited + num < INT_BIT)
        {
            m_bitWrited += num;
            m_curVal <<= num;
            m_curVal += value;
        }
        else
        {
            if (m_bitWrited != 0)
            {
                m_curVal <<= (INT_BIT - m_bitWrited);
            }
            m_bitWrited = m_bitWrited + num - INT_BIT;
            m_curVal += value >> m_bitWrited;
            *m_buffer++ = my_htonl(m_curVal);
            m_curVal = value & m_masks[m_bitWrited];
        }
        m_totalBits -= num;
    }

    inline void putBit(unsigned value) { putBits(1, value); }
    
    inline void flushBits()
    {
        if (m_bitWrited != 0)
        {
            m_curVal <<= (INT_BIT - m_bitWrited);
        }
        unsigned prevVal = my_ntohl(*m_buffer);
        prevVal &= m_masks[INT_BIT - m_bitWrited];
        prevVal |= m_curVal;
        *m_buffer = my_htonl(prevVal);
    }

    inline unsigned getBitsCount() { return (unsigned)(m_buffer - m_initBuffer) * INT_BIT + m_bitWrited; }

   private:
    unsigned m_curVal;
    unsigned m_bitWrited;
};

void updateBits(const BitStreamReader& bitReader, int bitOffset, int bitLen, int value);
void updateBits(const uint8_t* buffer, int bitOffset, int bitLen, int value);

// move len bits from oldBitOffset position to newBitOffset
void moveBits(uint8_t* buffer, int oldBitOffset, int newBitOffset, int len);

#endif
