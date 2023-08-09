#include "lpcmStreamReader.h"

#include <fs/systemlog.h>

#include <sstream>

#include "ioContextDemuxer.h"
#include "tsPacket.h"
#include "vodCoreException.h"
#include "vod_common.h"
#include "wave.h"

static constexpr int m2tsFreqs[] = {0, 48000, 0, 0, 96000, 192000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static constexpr int MAX_HEADER_SIZE = 192;

static uint32_t FOUR_CC(const uint8_t a, const uint8_t b, const uint8_t c, const uint8_t d)
{
    return my_ntohl((uint32_t(a) << 24) + (uint32_t(b) << 16) + (uint32_t(c) << 8) + uint32_t(d));
}

static const uint32_t RIFF_SMALL = FOUR_CC('r', 'i', 'f', 'f');
static const uint32_t RIFF_LARGE = FOUR_CC('R', 'I', 'F', 'F');
static const uint32_t FMT_FOURCC = FOUR_CC('f', 'm', 't', ' ');

using namespace wave_format;

int LPCMStreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    if (m_headerType == LPCMHeaderType::htNone)
        if (!detectLPCMType(m_buffer, m_bufEnd - m_buffer))
            return 0;
    uint8_t* frame = findFrame(m_buffer, m_bufEnd);
    if (frame == nullptr)
        return 0;
    int skipBytes = 0;
    int skipBeforeBytes = 0;
    const int len = decodeFrame(frame, m_bufEnd, skipBytes, skipBeforeBytes);
    if (len < 1)
        return 0;

    // Blu-ray core specifications Table 9-11 - HDMV LPCM audio registration descriptor
    uint8_t* curPos = dstBuff;
    *curPos++ = (uint8_t)TSDescriptorTag::REGISTRATION;  // descriptor tag
    *curPos++ = 8;                                       // descriptor length
    *curPos++ = 'H';
    *curPos++ = 'D';
    *curPos++ = 'M';
    *curPos++ = 'V';
    *curPos++ = 0xff;  // stuffing_bits

    *curPos++ = (uint8_t)TSDescriptorTag::LPCM;  // descriptor tag
    const int audio_presentation_type = (m_channels > 2) ? 6 : (m_channels == 2) ? 3 : 1;
    const int sampling_frequency = (m_freq == 192000) ? 5 : (m_freq == 96000) ? 4 : 1;
    *curPos++ = (audio_presentation_type << 4) + sampling_frequency;
    *curPos++ = ((m_bitsPerSample - 12) << 4) + 0x3f;  // bits_per_sample (2 bits), stuffing_bits

    return (int)(curPos - dstBuff);
}

int LPCMStreamReader::decodeLPCMHeader(uint8_t* buff)
{
    const int audio_data_payload_size = AV_RB16(buff);
    const int channelsIndex = buff[2] >> 4;
    m_lfeExists = false;
    switch (channelsIndex)
    {
    case 0:
    case 1:
        m_channels = 1;
        break;
    case 2:
    case 3:
        m_channels = 2;
        break;
    case 4:
        m_channels = 3;
        break;
    case 5:
        m_channels = 3;
        m_lfeExists = true;
        break;
    case 6:
        m_channels = 4;
        m_lfeExists = true;
        break;
    case 7:
        m_channels = 4;
        break;
    case 8:
        m_channels = 5;
        break;
    case 9:
        m_channels = 6;
        m_lfeExists = true;
        break;
    case 10:
        m_channels = 7;
        break;
    case 11:
        m_channels = 8;
        m_lfeExists = true;
        break;
    }
    const int sampling_index = buff[2] & 0x0f;
    m_freq = m2tsFreqs[sampling_index];
    const int bits_per_sample = buff[3] >> 6;
    if (bits_per_sample > 0)
        m_bitsPerSample = 12 + 4 * bits_per_sample;
    return audio_data_payload_size;
}

void LPCMStreamReader::storeChannelData(uint8_t* start, uint8_t* end, const int chNum, uint8_t* tmpData, const int mch) const
{
    const int ch1SampleSize = (m_bitsPerSample == 20 ? 3 : m_bitsPerSample / 8);
    const int fullSampleSize = mch * ch1SampleSize;
    const uint8_t* curPos = start + ch1SampleSize * (chNum - 1);
    uint8_t* dst = tmpData;
    for (; curPos < end; curPos += fullSampleSize)
    {
        for (int j = 0; j < ch1SampleSize; j++) *dst++ = curPos[j];
    }
}

void LPCMStreamReader::restoreChannelData(uint8_t* start, uint8_t* end, const int chNum, uint8_t* tmpData, const int mch) const
{
    const int ch1SampleSize = (m_bitsPerSample == 20 ? 3 : m_bitsPerSample / 8);
    const int fullSampleSize = mch * ch1SampleSize;
    uint8_t* curPos = start + ch1SampleSize * (chNum - 1);
    const uint8_t* src = tmpData;
    for (; curPos < end; curPos += fullSampleSize)
    {
        for (int j = 0; j < ch1SampleSize; j++) curPos[j] = *src++;
    }
}

void LPCMStreamReader::copyChannelData(uint8_t* start, uint8_t* end, const int chFrom, const int chTo, const int mch) const
{
    // int mch = m_channels + (m_channels%2==1 ? 1 : 0);
    const int ch1SampleSize = (m_bitsPerSample == 20 ? 3 : m_bitsPerSample / 8);
    const int fullSampleSize = mch * ch1SampleSize;
    const uint8_t* src = start + ch1SampleSize * (chFrom - 1);
    uint8_t* dst = start + ch1SampleSize * (chTo - 1);
    for (; src < end; src += fullSampleSize)
    {
        for (int j = 0; j < ch1SampleSize; j++) dst[j] = src[j];
        dst += fullSampleSize;
    }
}

void LPCMStreamReader::removeChannel(uint8_t* start, uint8_t* end, const int cnNum, const int mch) const
{
    // int mch = m_channels + (m_channels%2==1 ? 1 : 0);
    assert(mch == cnNum);
    const int ch1SampleSize = (m_bitsPerSample == 20 ? 3 : m_bitsPerSample / 8);
    const int fullSampleSize = mch * ch1SampleSize;
    uint8_t* dst = start;
    for (const uint8_t* curPos = start; curPos < end; curPos += fullSampleSize)
    {
        memmove(dst, curPos, fullSampleSize);
        dst += fullSampleSize - ch1SampleSize;
    }
}

uint32_t LPCMStreamReader::convertWavToPCM(uint8_t* start, uint8_t* end)
{
    // int mch = m_channels; // + (m_channels%2==1 ? 1 : 0);
    const int ch1SampleSize = (m_bitsPerSample == 20 ? 3 : m_bitsPerSample / 8);
    const int fullSampleSize = m_channels * ch1SampleSize;
    if (end - start < fullSampleSize)
        return 0;
    int64_t cLen = end - start;
    cLen -= cLen % fullSampleSize;
    end = start + cLen;

    const int64_t ch1FullSize = (end - start) / m_channels;
    // 1. convert byte order to little endian
    if (m_bitsPerSample == 16)
    {
        for (auto curPos = (uint16_t*)start; curPos < (uint16_t*)end; ++curPos) *curPos = my_htons(*curPos);
    }
    else if (m_bitsPerSample > 16)
    {
        for (uint8_t* curPos = start; curPos < end - 2; curPos += 3)
        {
            const uint8_t tmp = curPos[0];
            curPos[0] = curPos[2];
            curPos[2] = tmp;
        }
    }
    // 2. Remap channels to Blu-ray standard
    if (m_channels == 6)
    {
        const auto tmpData = new uint8_t[ch1FullSize];
        storeChannelData(start, end, 4, tmpData, m_channels);    // copy channel 6(LFE) to tmpData
        copyChannelData(start, end, 5, 4, m_channels);           // Shift RS
        copyChannelData(start, end, 6, 5, m_channels);           // Shift LS
        restoreChannelData(start, end, 6, tmpData, m_channels);  // copy LFE to channel #4
        delete[] tmpData;
    }
    else if (m_channels == 7)
    {
        const auto tmpData = new uint8_t[ch1FullSize];

        storeChannelData(start, end, 4, tmpData, m_channels);  // copy channel 8(LFE) to tmpData
        copyChannelData(start, end, 6, 4, m_channels);
        copyChannelData(start, end, 5, 6, m_channels);
        restoreChannelData(start, end, 5, tmpData, m_channels);

        delete[] tmpData;
    }
    else if (m_channels == 8)
    {
        const auto lfeData = new uint8_t[ch1FullSize];

        storeChannelData(start, end, 4, lfeData, m_channels);  // copy channel 8(LFE) to tmpData
        copyChannelData(start, end, 7, 4, m_channels);
        copyChannelData(start, end, 8, 7, m_channels);
        restoreChannelData(start, end, 8, lfeData, m_channels);

        delete[] lfeData;
    }
    // 3. transfer to frame buffer and Add X channel (zero channel for padding if needed)
    uint8_t* dst = m_tmpFrameBuffer + (m_needPCMHdr ? 4 : 0);
    const uint8_t* src = start;
    for (; src < end; src += fullSampleSize)
    {
        memcpy(dst, src, fullSampleSize);
        dst += fullSampleSize;
        if (m_channels % 2 == 1)
        {
            memset(dst, 0, ch1SampleSize);
            dst += ch1SampleSize;
        }
    }
    assert(dst < m_tmpFrameBuffer + sizeof(m_tmpFrameBuffer));
    // 4. add LPCM frame header
    if (m_needPCMHdr)
    {
        const int audio_data_payload_size = (m_bitsPerSample == 20 ? 24 : m_bitsPerSample) * m_freq *
                                            ((m_channels + 1) & 0xfe) / 8 / 200;  // 5 ms frame len. 1000/5 = 200
        // int audio_data_payload_size = dst - (m_tmpFrameBuffer + 4);
        m_tmpFrameBuffer[0] = audio_data_payload_size >> 8;
        m_tmpFrameBuffer[1] = audio_data_payload_size & 0xff;
        int channelsIndex = 1;
        switch (m_channels)
        {
        case 2:
            channelsIndex = 3;
            break;
        case 3:
            channelsIndex = 4 + (m_lfeExists ? 1 : 0);
            break;
        case 4:
            channelsIndex = 6 + (m_lfeExists ? 0 : 1);
            break;
        case 5:
            channelsIndex = 8;
            break;
        case 6:
            channelsIndex = 9;
            break;
        case 7:
            channelsIndex = 10;
            break;
        case 8:
            channelsIndex = 11;
            break;
        }
        int sampling_index = 1;
        if (m_freq == 96000)
            sampling_index = 4;
        else if (m_freq == 192000)
            sampling_index = 5;
        m_tmpFrameBuffer[2] = (channelsIndex << 4) + sampling_index;
        const int bits_per_sample = (m_bitsPerSample - 12) / 4;
        m_tmpFrameBuffer[3] = (bits_per_sample << 6) + (m_firstFrame << 5);
    }
    return (int)(dst - m_tmpFrameBuffer);
}

uint32_t LPCMStreamReader::convertLPCMToWAV(uint8_t* start, uint8_t* end)
{
    const int mch = m_channels + (m_channels % 2 == 1 ? 1 : 0);
    const int64_t ch1FullSize = (end - start) / mch;

    if (m_lastChannelRemapPos != start)
    {
        m_lastChannelRemapPos = start;

        // 1. convert byte order to little endian
        if (m_bitsPerSample == 16)
        {
            for (auto curPos = (uint16_t*)start; curPos < (uint16_t*)end; ++curPos) *curPos = my_ntohs(*curPos);
        }
        else if (m_bitsPerSample > 16)
        {
            for (uint8_t* curPos = start; curPos < end; curPos += 3)
            {
                const uint8_t tmp = curPos[0];
                curPos[0] = curPos[2];
                curPos[2] = tmp;
            }
        }
        // 2. Remap channels to WAV standard
        if (m_channels == 1)
        {
            removeChannel(start, end, 2, mch);
        }
        else if (m_channels == 3)
        {
            removeChannel(start, end, 4, mch);
        }
        else if (m_channels == 5)
        {
            removeChannel(start, end, 6, mch);
        }
        else if (m_channels == 6)
        {
            const auto tmpData = new uint8_t[ch1FullSize];
            storeChannelData(start, end, 6, tmpData, mch);    // copy channel 6(LFE) to tmpData
            copyChannelData(start, end, 5, 6, mch);           // Shift RS
            copyChannelData(start, end, 4, 5, mch);           // Shift LS
            restoreChannelData(start, end, 4, tmpData, mch);  // copy LFE to channel #4
            delete[] tmpData;
        }
        else if (m_channels == 7)
        {
            const auto tmpData = new uint8_t[ch1FullSize];

            storeChannelData(start, end, 4, tmpData, mch);  // copy channel 8(LFE) to tmpData
            copyChannelData(start, end, 5, 4, mch);
            copyChannelData(start, end, 6, 5, mch);
            restoreChannelData(start, end, 6, tmpData, mch);

            delete[] tmpData;
        }
        else if (m_channels == 8)
        {
            const auto lfeData = new uint8_t[ch1FullSize];

            storeChannelData(start, end, 8, lfeData, mch);
            copyChannelData(start, end, 7, 8, mch);
            copyChannelData(start, end, 4, 7, mch);
            restoreChannelData(start, end, 4, lfeData, mch);

            delete[] lfeData;
        }
        if (m_channels == 7)
            removeChannel(start, end, 8, mch);
    }

    return (int)(end - start - (m_channels % 2 == 1 ? ch1FullSize : 0));
}

uint8_t* LPCMStreamReader::findSubstr(const char* pattern, uint8_t* buff, uint8_t* end)
{
    const size_t patternLen = strlen(pattern);
    for (uint8_t* curPos = buff; curPos < end - patternLen; curPos++)
        for (size_t j = 0; j < patternLen; j++)
            if (curPos[j] != pattern[j])
                break;
            else if (j == patternLen - 1)
                return curPos;
    return nullptr;
}

int LPCMStreamReader::decodeWaveHeader(uint8_t* buff, uint8_t* end)
{
    if (end - buff < 20)
        return NOT_ENOUGH_BUFFER;
    uint8_t* curPos = buff;
    if (m_channels == 0)
    {
        WAVEFORMATPCMEX* waveFormatPCMEx;
        uint64_t fmtSize;
        if (m_headerType == LPCMHeaderType::htWAVE64)
        {
            curPos = findSubstr("fmt ", buff, end);
            if (curPos == nullptr || curPos + sizeof(wave_format::GUID) + 8 >= end)
                return NOT_ENOUGH_BUFFER;
            uint8_t* tmpPos = curPos + sizeof(wave_format::GUID);
            fmtSize = *((uint64_t*)tmpPos);
            if (curPos + fmtSize >= end)
                return NOT_ENOUGH_BUFFER;
            waveFormatPCMEx = (WAVEFORMATPCMEX*)(tmpPos + 8);
        }
        else
        {
            curPos = findSubstr("fmt ", buff, end);
            if (curPos == nullptr || curPos + 8 >= end)
                return NOT_ENOUGH_BUFFER;
            fmtSize = *((uint32_t*)(curPos + 4));
            if (curPos + 8 + fmtSize >= end)
                return NOT_ENOUGH_BUFFER;
            curPos += 8;
            waveFormatPCMEx = (WAVEFORMATPCMEX*)curPos;
        }

        m_channels = waveFormatPCMEx->nChannels;
        if (m_channels > 8)
            THROW(ERR_COMMON, "Too many channels: " << m_channels << ". Maximum supported value is 8(7.1)");
        if (m_channels == 0)
            THROW(ERR_COMMON, "Invalid channels count: 0. WAVE header is invalid.");
        m_freq = waveFormatPCMEx->nSamplesPerSec;
        if (m_freq != 48000 && m_freq != 96000 && m_freq != 192000)
            THROW(ERR_COMMON, "Sample rate "
                                  << m_freq
                                  << " is not supported for LPCM format. Allowed values: 48000, 96000, 192000");
        m_bitsPerSample = waveFormatPCMEx->wBitsPerSample;
        if (m_bitsPerSample != 16 && m_bitsPerSample != 20 && m_bitsPerSample != 24)
            THROW(ERR_COMMON, "Bit depth " << m_bitsPerSample
                                           << " is not supported for LPCM format. Allowed values: 16bit, 20bit, 24bit");
        m_lfeExists = false;
        if (waveFormatPCMEx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            if (waveFormatPCMEx->Samples.wValidBitsPerSample)
                m_bitsPerSample = waveFormatPCMEx->Samples.wValidBitsPerSample;
            m_lfeExists = waveFormatPCMEx->dwChannelMask & SPEAKER_LOW_FREQUENCY;
            if (!(waveFormatPCMEx->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))
                THROW(ERR_COMMON, "Unsupported WAVE format. Only PCM audio is supported.");
        }
        else if (waveFormatPCMEx->wFormatTag == 0x01)
        {  // standard format
            if (m_channels > 2)
            {
                if (m_channels == 3)
                {
                    LTRACE(LT_WARN, 2,
                           "Warning! Multi channels WAVE file for stream "
                               << m_streamIndex
                               << " do not contain channels configuration info. Applying default value: L R LFE");
                    m_lfeExists = true;
                }
                else if (m_channels == 4)
                {
                    LTRACE(LT_WARN, 2,
                           "Warning! Multi channels WAVE file for stream "
                               << m_streamIndex
                               << " do not contain channels configuration info. Applying default value: L R BL BR");
                }
                else if (m_channels == 5)
                {
                    LTRACE(LT_WARN, 2,
                           "Warning! Multi channels WAVE file for stream "
                               << m_streamIndex
                               << " do not contain channels configuration info. Applying default value: L R C BL BR");
                }
                else if (m_channels == 6)
                {
                    LTRACE(
                        LT_WARN, 2,
                        "Warning! Multi channels WAVE file for stream "
                            << m_streamIndex
                            << " do not contain channels configuration info. Applying default value: L R C LFE BL BR");
                    m_lfeExists = true;
                }
                else if (m_channels == 7)
                {
                    LTRACE(LT_WARN, 2,
                           "Warning! Multi channels WAVE file for stream "
                               << m_streamIndex
                               << " do not contain channels configuration info. Applying default value: L R C BL BR SL "
                                  "SR");
                }
                else if (m_channels == 8)
                {
                    LTRACE(LT_WARN, 2,
                           "Warning! Multi channels WAVE file for stream "
                               << m_streamIndex
                               << " do not contain channels configuration info. Applying default value: L R C LFE BL "
                                  "BR SL SR");
                    m_lfeExists = true;
                }
            }
        }
        else
            THROW(ERR_COMMON, "Unsupported WAVE format. wFormatTag: " << waveFormatPCMEx->wFormatTag)
        curPos += fmtSize;
    }

    curPos = findSubstr("data", curPos, FFMIN(curPos + MAX_HEADER_SIZE, end));
    if (curPos == nullptr)
    {
        if (end < curPos + MAX_HEADER_SIZE)
            return NOT_ENOUGH_BUFFER;
        else
            return 0;  // 'riff' header was wrong detected, it is just data
    }

    if (m_headerType == LPCMHeaderType::htWAVE)
    {
        if (curPos + 8 >= end)
            return NOT_ENOUGH_BUFFER;
        curPos += 4;  // skip 'data' identifier
        m_curChunkLen = *((uint32_t*)curPos);
        if (m_curChunkLen == 0)
            m_openSizeWaveFormat = true;
        curPos += 4;
    }
    else
    {
        if (curPos + sizeof(wave_format::GUID) + 8 >= end)
            return NOT_ENOUGH_BUFFER;
        curPos += sizeof(wave_format::GUID);
        // For w64, data length includes data metadata (16 bytes) and size (8 bytes)
        m_curChunkLen = *((uint64_t*)curPos) - 24;
        curPos += 8;
    }
    return (int)(curPos - buff);
}

int LPCMStreamReader::decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes)
{
    skipBeforeBytes = skipBytes = 0;
    if (m_headerType == LPCMHeaderType::htM2TS)
    {
        const int audio_data_payload_size = decodeLPCMHeader(buff);
        if (end - buff < audio_data_payload_size + 4)
            return NOT_ENOUGH_BUFFER;
        if (m_demuxMode)
        {
            const uint32_t newSize = convertLPCMToWAV(buff + 4, buff + 4 + audio_data_payload_size);
            skipBytes = audio_data_payload_size - newSize;
            skipBeforeBytes = 4;
            return newSize;
        }
        else
        {
            return 4 + audio_data_payload_size;  // 4 byte header + payload
        }
    }
    else if (m_headerType == LPCMHeaderType::htWAVE || m_headerType == LPCMHeaderType::htWAVE64)
    {
        int hdrSize = 0;
        if (end - buff < 4)
            return NOT_ENOUGH_BUFFER;

        const auto curPtr32 = (const uint32_t*)buff;
        if (m_curChunkLen == 0 && (*curPtr32 == RIFF_SMALL || *curPtr32 == RIFF_LARGE))
        {
            if (end - buff < 8)
                return NOT_ENOUGH_BUFFER;
            if (!m_openSizeWaveFormat || curPtr32[1] == 0)
            {
                hdrSize = decodeWaveHeader(buff, end);
                if (hdrSize == NOT_ENOUGH_BUFFER)
                    return NOT_ENOUGH_BUFFER;
                skipBeforeBytes = hdrSize;
                if (m_needSync)
                    m_curChunkLen = 0;  // decode header again
            }
        }
        if (m_frameRest == 0)
        {
            if (m_bitsPerSample == 0)
                return 0;  // can't decode frame

            // Assume 5ms frames = 1s / 200
            const int reqFrameLen = (m_bitsPerSample == 20 ? 3 : m_bitsPerSample / 8) * m_channels * m_freq / 200;
            int frameLen = reqFrameLen;
            // a chunk can contain several frames, but a frame canot be bigger than a chunck
            if (m_curChunkLen)
                frameLen = (int)FFMIN(reqFrameLen, m_curChunkLen);
            if (end - buff < frameLen + hdrSize)
                return NOT_ENOUGH_BUFFER;
            const int sampleSize = (m_bitsPerSample == 20 ? 3 : m_bitsPerSample / 8) * m_channels;
            m_frameRest = reqFrameLen - (frameLen / sampleSize * sampleSize);
            if (m_frameRest == reqFrameLen)
                m_frameRest = 0;
            else
                m_needPCMHdr = true;
            return frameLen;
        }
        else
        {
            if (end - buff < m_frameRest + hdrSize)
                return NOT_ENOUGH_BUFFER;
            const int frameLen = (int)m_frameRest;
            m_frameRest = 0;
            m_needPCMHdr = false;
            return frameLen;
        }
    }
    else
        return 0;
}

uint8_t* LPCMStreamReader::findFrame(uint8_t* buff, uint8_t* end)
{
    if (m_headerType == LPCMHeaderType::htNone)
        if (!detectLPCMType(buff, end - buff))
            return nullptr;
    return buff;
}

double LPCMStreamReader::getFrameDuration()
{
    if (m_frameRest == 0)
        return 5 * INTERNAL_PTS_FREQ / 1000.0;  // 5 ms frames
    else
        return 0;
}

const std::string LPCMStreamReader::getStreamInfo()
{
    std::ostringstream str;
    const int mch = m_channels + (m_channels % 2 == 1 ? 1 : 0);
    const int bitrate = (m_bitsPerSample == 20 ? 24 : m_bitsPerSample) * m_freq * mch;
    str << "Bitrate: " << bitrate / 1000 << "Kbps  ";
    str << "Sample Rate: " << m_freq / 1000 << "KHz  ";
    str << "Channels: ";
    if (m_lfeExists)
        str << (int)m_channels - 1 << ".1";
    else
        str << (int)m_channels;
    str << "  Bits per sample: " << m_bitsPerSample << "bit";
    return str.str();
}

void LPCMStreamReader::writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket) {}

int LPCMStreamReader::writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                                        PriorityDataInfo* priorityData)
{
    uint8_t* curPos = dstBuffer;
    if (m_demuxMode)
    {
        if (m_firstFrame)
        {
            if (dstEnd - dstBuffer < sizeof(WAVEFORMATPCMEX) + 8)
                THROW(ERR_COMMON, "LPCM stream error: Not enough buffer for writing headers");
            // write wave header
            // memcpy(curPos, "RIFF\x00\x00\x00\x00WAVEfmt_\x00\x00\x00\x00", 20);
            memcpy(curPos, "RIFF\xff\xff\xff\xffWAVEfmt ", 16);
            curPos += 16;
            const auto fmtSize = (uint32_t*)curPos;
            *fmtSize = sizeof(WAVEFORMATPCMEX);
            curPos += 4;
            const auto waveFormatPCMEx = (WAVEFORMATPCMEX*)curPos;
            waveFormatPCMEx->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            waveFormatPCMEx->nChannels = m_channels;
            waveFormatPCMEx->nSamplesPerSec = m_freq;
            waveFormatPCMEx->nAvgBytesPerSec = m_channels * m_freq * ((m_bitsPerSample + 4) >> 3);
            const int bitsPerSample = m_bitsPerSample == 20 ? 24 : m_bitsPerSample;
            waveFormatPCMEx->nBlockAlign = m_channels * bitsPerSample / 8;
            waveFormatPCMEx->wBitsPerSample = bitsPerSample;
            waveFormatPCMEx->cbSize = 22;  // After this to GUID
            waveFormatPCMEx->Samples.wValidBitsPerSample = m_bitsPerSample;
            waveFormatPCMEx->dwChannelMask = getWaveChannelMask(m_channels, m_lfeExists);  // Specify PCM
            waveFormatPCMEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            curPos += sizeof(WAVEFORMATPCMEX);
            memcpy(curPos, "data\xff\xff\xff\xff", 8);
            curPos += 8;
        }
    }
    /*
    if (m_headerType == htM2TS) {
            m_firstFrame = false;
            return curPos - dstBuffer; // frames already has right LPCM headers
    }
    */
    m_firstFrame = false;
    return (int)(curPos - dstBuffer);
}

void LPCMStreamReader::setHeadersType(const LPCMStreamReader::LPCMHeaderType value) { m_headerType = value; }

bool LPCMStreamReader::detectLPCMType(uint8_t* buffer, const int64_t len)
{
    if (len == 0)
        return false;

    if (m_containerDataType == TRACKTYPE_WAV)
    {
        m_headerType = LPCMHeaderType::htWAVE;
        return true;
    }

    const uint8_t* end = buffer + len;
    // 1. test for WAVEHeader
    uint8_t* curPos = buffer;
    if ((curPos[0] == 'R' && curPos[1] == 'I' && curPos[2] == 'F' && curPos[3] == 'F') ||
        (curPos[0] == 'r' && curPos[1] == 'i' && curPos[2] == 'f' && curPos[3] == 'f'))
    {
        m_headerType = LPCMHeaderType::htWAVE;
        const auto testWave64 = (wave_format::GUID*)curPos;
        if (*testWave64 == WAVE64GUID)
            m_headerType = LPCMHeaderType::htWAVE64;
        return true;
    }

    if (m_testMode && m_containerType != ContainerType::ctLPCM && m_containerDataType != TRACKTYPE_PCM)
        return false;  // LPCM definition has too few bytes. We can't detect LPCM without hints from source container

    // 2. test for M2TS LPCM headers
    curPos = buffer;
    while (curPos < end)
    {
        const uint16_t frameLen = AV_RB16(curPos);
        switch (frameLen)
        {
        case 960:
        case 1440:
        case 1920:
        case 2880:
        case 3840:
        case 4320:
        case 5760:
        case 7680:
        case 8640:
        case 11520:
        case 17280:
            curPos += frameLen + 4;
            break;
        default:
            return false;
        }
    }
    m_headerType = LPCMHeaderType::htM2TS;
    return true;
}

bool LPCMStreamReader::beforeFileCloseEvent(File& file)
{
    file.sync();
    uint64_t fileSize = 0;
    file.size(&fileSize);
    if (fileSize <= 0xfffffffful)
    {
        uint32_t dataSize = (uint32_t)fileSize - 8;
        if (file.seek(4, File::SeekMethod::smBegin) == (int64_t)-1)
            return false;
        if (file.write(&dataSize, 4) != 4)
            return false;
        if (file.seek(64, File::SeekMethod::smBegin) == (int64_t)-1)
            return false;
        dataSize = (uint32_t)fileSize - 68;
        if (file.write(&dataSize, 4) != 4)
            return false;
    }
    return true;
}

int LPCMStreamReader::readPacket(AVPacket& avPacket)
{
    avPacket.flags = m_flags;  // + AVPacket::IS_COMPLETE_FRAME | AVPacket::FORCE_NEW_FRAME;
    avPacket.stream_index = m_streamIndex;
    avPacket.codecID = getCodecInfo().codecID;
    avPacket.codec = this;
    avPacket.data = nullptr;
    avPacket.size = 0;
    avPacket.duration = 0;
    avPacket.dts = avPacket.pts = (int64_t)(m_curPts * m_stretch) + m_timeOffset;
    assert(m_curPos <= m_bufEnd);
    if (m_curPos == m_bufEnd)
        return NEED_MORE_DATA;
    int skipBytes = 0;
    int skipBeforeBytes = 0;
    if (m_needSync)
    {
        uint8_t* frame = findFrame(m_curPos, m_bufEnd);
        if (frame == nullptr)
        {
            m_processedBytes += m_bufEnd - m_curPos;
            return NEED_MORE_DATA;
        }
        const int decodeRez = decodeFrame(frame, m_bufEnd, skipBytes, skipBeforeBytes);
        if (decodeRez == NOT_ENOUGH_BUFFER)
        {
            memcpy(&m_tmpBuffer[0], m_curPos, m_bufEnd - m_curPos);
            m_tmpBufferLen = m_bufEnd - m_curPos;
            m_curPos = m_bufEnd;
            return NEED_MORE_DATA;
        }
        else if (decodeRez + skipBytes + skipBeforeBytes <= 0)
        {
            m_curPos++;
            m_processedBytes++;
            return 0;
        }
        m_processedBytes += frame - m_curPos;
        LTRACE(LT_INFO, 2,
               "Decoding " << getCodecInfo().displayName << " stream (track " << m_streamIndex
                           << "): " << getStreamInfo());
        m_curPos = frame;
        m_needSync = false;
    }
    avPacket.dts = avPacket.pts = (int64_t)(m_curPts * m_stretch) + m_timeOffset;
    if (m_bufEnd - m_curPos < getHeaderLen())
    {
        memmove(&m_tmpBuffer[0], m_curPos, m_bufEnd - m_curPos);
        m_tmpBufferLen = m_bufEnd - m_curPos;
        m_curPos = m_bufEnd;
        return NEED_MORE_DATA;
    }
    skipBytes = 0;
    skipBeforeBytes = 0;
    const int frameLen = decodeFrame(m_curPos, m_bufEnd, skipBytes, skipBeforeBytes);
    if (frameLen == NOT_ENOUGH_BUFFER)
    {
        memmove(&m_tmpBuffer[0], m_curPos, m_bufEnd - m_curPos);
        m_tmpBufferLen = m_bufEnd - m_curPos;
        m_curPos = m_bufEnd;
        return NEED_MORE_DATA;
    }
    else if (frameLen + skipBytes + skipBeforeBytes <= 0)
    {
        LTRACE(LT_INFO, 2, getCodecInfo().displayName << " bad frame detected. Resync stream.");
        m_needSync = true;
        return 0;
    }
    if (m_bufEnd - m_curPos < frameLen + skipBytes + skipBeforeBytes)
    {
        memmove(&m_tmpBuffer[0], m_curPos, m_bufEnd - m_curPos);
        m_tmpBufferLen = m_bufEnd - m_curPos;
        m_curPos = m_bufEnd;
        return NEED_MORE_DATA;
    }
    avPacket.data = m_curPos;
    avPacket.data += skipBeforeBytes;
    if (m_curChunkLen)
        m_curChunkLen -= frameLen;
    if (frameLen > MAX_AV_PACKET_SIZE)
        THROW(ERR_AV_FRAME_TOO_LARGE, "AV frame too large (" << frameLen << " bytes). Increase AV buffer.");
    avPacket.size = frameLen;
    avPacket.duration = (int64_t)getFrameDuration();  // m_ptsIncPerFrame;
    m_curPts += avPacket.duration;
    doMplsCorrection();
    if ((m_headerType == LPCMHeaderType::htWAVE || m_headerType == LPCMHeaderType::htWAVE64) && !m_demuxMode)
    {
        avPacket.size = convertWavToPCM(avPacket.data, avPacket.data + avPacket.size);
        avPacket.data = m_tmpFrameBuffer;
    }

    m_frameNum++;
    if (m_frameNum % 1000 == 0)
        LTRACE(LT_DEBUG, 0, "Processed " << m_frameNum << " " << getCodecInfo().displayName << " frames");
    m_curPos += frameLen + skipBytes + skipBeforeBytes;
    m_processedBytes += frameLen + skipBytes + skipBeforeBytes;
    if (m_frameRest == 0)
        avPacket.flags = m_flags + (AVPacket::IS_COMPLETE_FRAME | AVPacket::FORCE_NEW_FRAME);
    return 0;
}

int LPCMStreamReader::flushPacket(AVPacket& avPacket)
{
    avPacket.duration = 0;
    avPacket.data = nullptr;
    avPacket.size = 0;
    avPacket.stream_index = m_streamIndex;
    avPacket.flags = m_flags + AVPacket::IS_COMPLETE_FRAME;
    avPacket.codecID = getCodecInfo().codecID;
    avPacket.codec = this;
    int skipBytes = 0;
    int skipBeforeBytes = 0;
    if (m_tmpBufferLen >= getHeaderLen())
    {
        const int size = decodeFrame(&m_tmpBuffer[0], &m_tmpBuffer[0] + m_tmpBufferLen, skipBytes, skipBeforeBytes);
        if (size + skipBytes + skipBeforeBytes <= 0 && size != NOT_ENOUGH_BUFFER)
            return 0;
    }
    avPacket.dts = avPacket.pts = (int64_t)(m_curPts * m_stretch) + m_timeOffset;
    if (m_tmpBuffer.size() > 0)
        avPacket.data = &m_tmpBuffer[0];
    else
        avPacket.data = nullptr;
    avPacket.data += skipBeforeBytes;
    if (m_tmpBufferLen > 0)
    {
        avPacket.size = (int)m_tmpBufferLen;
    }
    if ((m_headerType == LPCMHeaderType::htWAVE || m_headerType == LPCMHeaderType::htWAVE64) && !m_demuxMode)
    {
        if (m_frameRest > 0)
            m_needPCMHdr = false;
        if (avPacket.size > 0)
            avPacket.size = convertWavToPCM(avPacket.data, avPacket.data + avPacket.size);
        avPacket.data = m_tmpFrameBuffer;
    }

    if (m_frameRest > 0 && !m_demuxMode)
    {
        const int64_t samplesRest = m_frameRest / m_channels;
        m_frameRest = samplesRest * ((m_channels + 1) & 0xfe);
        m_frameRest -= avPacket.size - (m_needPCMHdr ? 4 : 0);
        if (avPacket.data != nullptr && m_frameRest > 0)
        {
            memset(avPacket.data + avPacket.size, 0, m_frameRest);
            avPacket.size += (int)m_frameRest;
            m_processedBytes -= m_frameRest;
        }
    }
    LTRACE(LT_DEBUG, 0, "Processed " << m_frameNum << " " << getCodecInfo().displayName << " frames");
    m_processedBytes += avPacket.size + skipBytes + skipBeforeBytes;
    return avPacket.size;
}

const CodecInfo& LPCMStreamReader::getCodecInfo() { return lpcmCodecInfo; }

void LPCMStreamReader::setBuffer(uint8_t* data, const int dataLen, const bool lastBlock)
{
    m_lastChannelRemapPos = nullptr;
    SimplePacketizerReader::setBuffer(data, dataLen, lastBlock);
}
