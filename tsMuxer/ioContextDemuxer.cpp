#include "ioContextDemuxer.h"

#include <types/types.h>
#include <cmath>

#include "abstractStreamReader.h"
#include "vodCoreException.h"

using namespace std;

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
/////////////////////////////////////

double av_int2dbl(const uint64_t v)
{
    if (v + v > 0xFFEULL << 52)
        return 0;  // 0.0/0.0;
    return ldexp(static_cast<double>(((v & (1LL << 52) - 1) + (1LL << 52)) * (v >> 63 | 1)),
                 static_cast<int>(v >> 52 & 0x7FF) - 1075);
}

float av_int2flt(const uint32_t v)
{
    if (v + v > 0xFF000000U)
        return 0;  // 0.0/0.0;
    return ldexp(static_cast<float>(((v & 0x7FFFFF) + (1 << 23)) * (v >> 31 | 1)),
                 static_cast<int>(v >> 23 & 0xFF) - 150);
}

IOContextDemuxer::IOContextDemuxer(const BufferedReaderManager& readManager)
    : tracks(), m_readManager(readManager), m_lastReadRez(0)
{
    m_lastProcessedBytes = 0;
    m_bufferedReader = m_readManager.getReader("");
    m_readerID = m_bufferedReader->createReader(TS_FRAME_SIZE);
    m_curPos = m_bufEnd = nullptr;
    m_processedBytes = 0;
    m_isEOF = false;
    num_tracks = 0;
}

IOContextDemuxer::~IOContextDemuxer() { m_bufferedReader->deleteReader(m_readerID); }

int IOContextDemuxer::get_byte()
{
    uint32_t readedBytes = 0;
    int readRez = 0;
    if (m_curPos == m_bufEnd)
    {
        uint8_t* data = m_bufferedReader->readBlock(m_readerID, readedBytes, readRez);  // blocked read mode
        if (readedBytes > 0 && readRez == 0)
            m_bufferedReader->notify(m_readerID, readedBytes);
        m_lastReadRez = readRez;
        m_curPos = data + 188;
        m_bufEnd = m_curPos + readedBytes;
        if (m_curPos == m_bufEnd)
        {
            m_isEOF = true;
            return 0;
        }
    }
    m_processedBytes++;
    return *m_curPos++;
}

uint16_t IOContextDemuxer::get_be16() { return static_cast<uint16_t>(get_byte() << 8 | get_byte()); }

int IOContextDemuxer::get_be24() { return get_be16() << 8 | get_byte(); }

unsigned int IOContextDemuxer::get_be32() { return get_be16() << 16 | get_be16(); }

int64_t IOContextDemuxer::get_be64() { return static_cast<int64_t>(get_be32()) << 32 | get_be32(); }

bool IOContextDemuxer::url_fseek(const int64_t offset)
{
    m_curPos = m_bufEnd = nullptr;
    m_isEOF = false;
    m_processedBytes = offset;
    return dynamic_cast<BufferedFileReader*>(m_bufferedReader)->gotoByte(m_readerID, offset);
}

unsigned IOContextDemuxer::get_buffer(uint8_t* binary, unsigned size)
{
    uint32_t readedBytes = 0;
    int readRez = 0;
    uint8_t* dst = binary;
    const uint8_t* dstEnd = dst + size;
    if (m_curPos < m_bufEnd)
    {
        const unsigned copyLen = min((unsigned)(m_bufEnd - m_curPos), size);
        memcpy(dst, m_curPos, copyLen);
        dst += copyLen;
        m_curPos += copyLen;
        m_processedBytes += copyLen;
        size -= copyLen;
    }
    while (dst < dstEnd)
    {
        uint8_t* data = m_bufferedReader->readBlock(m_readerID, readedBytes, readRez);
        if (readedBytes > 0 && readRez == 0)
            m_bufferedReader->notify(m_readerID, readedBytes);

        m_curPos = data + 188;
        m_bufEnd = m_curPos + readedBytes;
        const unsigned copyLen = min((unsigned)(m_bufEnd - m_curPos), size);
        memcpy(dst, m_curPos, copyLen);
        dst += copyLen;
        m_curPos += copyLen;
        size -= copyLen;
        m_processedBytes += copyLen;
        if (readedBytes == 0)
            break;
    }
    return static_cast<uint32_t>(dst - binary);
}

void IOContextDemuxer::skip_bytes(const int64_t size)
{
    uint32_t readedBytes = 0;
    int readRez = 0;
    int64_t skipLeft = size;
    if (m_curPos < m_bufEnd)
    {
        const int64_t copyLen = min(m_bufEnd - m_curPos, skipLeft);
        skipLeft -= copyLen;
        m_curPos += copyLen;
        m_processedBytes += copyLen;
    }
    while (skipLeft > 0)
    {
        uint8_t* data = m_bufferedReader->readBlock(m_readerID, readedBytes, readRez);
        if (readedBytes > 0 && readRez == 0)
            m_bufferedReader->notify(m_readerID, readedBytes);
        m_curPos = data + 188;
        m_bufEnd = m_curPos + readedBytes;
        const int64_t copyLen = min(m_bufEnd - m_curPos, skipLeft);
        m_curPos += copyLen;
        m_processedBytes += copyLen;
        skipLeft -= copyLen;
        if (readedBytes == 0)
            break;
    }
}

void IOContextDemuxer::setFileIterator(FileNameIterator* itr)
{
    const auto br = dynamic_cast<BufferedReader*>(m_bufferedReader);
    if (br)
        br->setFileIterator(itr, m_readerID);
    else if (itr != nullptr)
        THROW(ERR_COMMON, "Can not set file iterator. Reader does not support bufferedReader interface.")
}

int64_t IOContextDemuxer::getDemuxedSize() { return 0; }

unsigned int IOContextDemuxer::get_le16()
{
    unsigned int val = get_byte();
    val |= get_byte() << 8;
    return val;
}

unsigned int IOContextDemuxer::get_le24()
{
    unsigned int val = get_le16();
    val |= get_byte() << 16;
    return val;
}

unsigned int IOContextDemuxer::get_le32()
{
    unsigned int val = get_le16();
    val |= get_le16() << 16;
    return val;
}
