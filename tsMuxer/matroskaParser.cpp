
#include "matroskaParser.h"

#include <fs/systemlog.h>

#include "bitStream.h"
#include "hevc.h"
#include "nalUnits.h"
#include "vodCoreException.h"
#include "vod_common.h"
#include "vvc.h"
#include "wave.h"

using namespace wave_format;

// ------------ H-264 ---------------

ParsedH264TrackData::ParsedH264TrackData(uint8_t* buff, const int size) : ParsedTrackPrivData(buff, size), m_nalSize(0)
{
    m_firstExtract = true;
    if (buff == nullptr)
        return;
    BitStreamReader bitReader{};
    try
    {
        bitReader.setBuffer(buff, buff + size);
        bitReader.skipBits(24);  // reserved 8, profile 8, reserved 8
        bitReader.skipBits(14);  // level 8, reserved 6
        m_nalSize = bitReader.getBits(2) + 1;
        bitReader.skipBits(3);  // reserved
        const int spsCnt = bitReader.getBits(5);
        for (int i = 0; i < spsCnt; i++)
        {
            const int spsLen = bitReader.getBits(16);
            if (spsLen > 0)
            {
                m_spsPpsList.emplace_back();
                std::vector<uint8_t>& curData = m_spsPpsList[m_spsPpsList.size() - 1];
                for (int j = 0; j < spsLen; j++) curData.push_back(bitReader.getBits(8) & 0xff);
            }
        }
        const int ppsCnt = bitReader.getBits(8);
        for (int i = 0; i < ppsCnt; i++)
        {
            const int ppsLen = bitReader.getBits(16);
            if (ppsLen > 0)
            {
                m_spsPpsList.emplace_back();
                std::vector<uint8_t>& curData = m_spsPpsList[m_spsPpsList.size() - 1];
                for (int j = 0; j < ppsLen; j++) curData.push_back(bitReader.getBits(8) & 0xff);
            }
        }
    }
    catch (BitStreamException& e)
    {
        (void)e;
        THROW(ERR_MATROSKA_PARSE, "Can't parse H.264 private data");
    }
};

void ParsedH264TrackData::writeNalHeader(uint8_t*& dst)
{
    for (int i = 0; i < 3; i++) *dst++ = 0;
    *dst++ = 1;
}
size_t ParsedH264TrackData::getSPSPPSLen() const
{
    size_t rez = 0;
    for (auto& i : m_spsPpsList) rez += i.size() + 4;
    return rez;
}

int ParsedH264TrackData::writeSPSPPS(uint8_t* dst) const
{
    const uint8_t* start = dst;
    for (auto& i : m_spsPpsList)
    {
        writeNalHeader(dst);
        memcpy(dst, i.data(), i.size());
        dst += i.size();
    }
    return static_cast<int>(dst - start);
}

bool ParsedH264TrackData::spsppsExists(uint8_t* buff, const int size)
{
    uint8_t* curPos = buff;
    const uint8_t* end = buff + size;
    bool spsFound = false;
    bool ppsFound = false;
    while (curPos < end - m_nalSize)
    {
        uint32_t elSize;
        if (m_nalSize == 4)
        {
            const auto cur32 = reinterpret_cast<uint32_t*>(curPos);
            elSize = my_ntohl(*cur32);
        }
        else
            elSize = (curPos[0] << 16l) + (curPos[1] << 8l) + curPos[2];
        const auto nalUnitType = static_cast<NALUnit::NALType>(curPos[m_nalSize] & 0x1f);
        if (nalUnitType == NALUnit::NALType::nuSPS)
            spsFound = true;
        else if (nalUnitType == NALUnit::NALType::nuPPS)
            ppsFound = true;
        curPos += elSize + m_nalSize;
    }
    return spsFound && ppsFound;
}

void ParsedH264TrackData::extractData(AVPacket* pkt, uint8_t* buff, const int size)
{
    int newBufSize = size;
    uint8_t* curPos = buff;
    const uint8_t* end = buff + size;
    int elements = 0;

    if (m_firstExtract && spsppsExists(buff, size))
        m_firstExtract = false;

    if (m_firstExtract)
        newBufSize += static_cast<int>(getSPSPPSLen());

    while (curPos <= end - m_nalSize)
    {
        uint32_t elSize = 0;
        if (m_nalSize == 4)
        {
            const auto cur32 = reinterpret_cast<uint32_t*>(curPos);
            elSize = my_ntohl(*cur32);
        }
        else if (m_nalSize == 3)
            elSize = (curPos[0] << 16l) + (curPos[1] << 8l) + curPos[2];
        else if (m_nalSize == 2)
            elSize = (curPos[0] << 8l) + curPos[1];
        else
            THROW(ERR_COMMON, "Unsupported nal unit size " << elSize);
        elements++;
        curPos += elSize + m_nalSize;
    }
    assert(curPos == end);
    if (curPos > end)
    {
        LTRACE(LT_ERROR, 2, "Matroska parse error: invalid H264 NAL unit size. NAL unit truncated.");
    }
    newBufSize += elements * (4 - m_nalSize);
    pkt->data = new uint8_t[newBufSize];
    pkt->size = newBufSize;

    uint8_t* dst = pkt->data;
    if (m_firstExtract)
    {
        dst += writeSPSPPS(dst);
    }
    curPos = buff;
    while (curPos <= end - m_nalSize)
    {
        uint32_t elSize = 0;
        if (m_nalSize == 4)
        {
            const auto cur32 = reinterpret_cast<uint32_t*>(curPos);
            elSize = my_ntohl(*cur32);
        }
        else if (m_nalSize == 3)
            elSize = (curPos[0] << 16l) + (curPos[1] << 8l) + curPos[2];
        else if (m_nalSize == 2)
            elSize = (curPos[0] << 8l) + curPos[1];
        else
            THROW(ERR_COMMON, "Unsupported nal unit size " << elSize);
        writeNalHeader(dst);
        assert((curPos[m_nalSize] & 0x80) == 0);
        memcpy(dst, curPos + m_nalSize, FFMIN(elSize, (uint32_t)(end - curPos)));
        curPos += elSize + m_nalSize;
        dst += elSize;
    }
    m_firstExtract = false;
}

// ----------- H.265 -----------------
ParsedH265TrackData::ParsedH265TrackData(uint8_t* buff, const int size) : ParsedH264TrackData(nullptr, 0)
{
    m_spsPpsList = hevc_extract_priv_data(buff, size, &m_nalSize);
}

bool ParsedH265TrackData::spsppsExists(uint8_t* buff, const int size)
{
    uint8_t* curPos = buff;
    const uint8_t* end = buff + size;
    bool vpsFound = false;
    bool spsFound = false;
    bool ppsFound = false;
    while (curPos < end - m_nalSize)
    {
        uint32_t elSize;
        if (m_nalSize == 4)
        {
            const auto cur32 = reinterpret_cast<uint32_t*>(curPos);
            elSize = my_ntohl(*cur32);
        }
        else
            elSize = (curPos[0] << 16l) + (curPos[1] << 8l) + curPos[2];
        const auto nalUnitType = static_cast<HevcUnit::NalType>((curPos[m_nalSize] >> 1) & 0x3f);
        if (nalUnitType == HevcUnit::NalType::VPS)
            vpsFound = true;
        else if (nalUnitType == HevcUnit::NalType::SPS)
            spsFound = true;
        else if (nalUnitType == HevcUnit::NalType::PPS)
            ppsFound = true;
        curPos += elSize + m_nalSize;
    }
    return vpsFound && spsFound && ppsFound;
}

// ----------- H.266 -----------------
ParsedH266TrackData::ParsedH266TrackData(uint8_t* buff, const int size) : ParsedH264TrackData(nullptr, 0)
{
    m_spsPpsList = vvc_extract_priv_data(buff, size, &m_nalSize);
}

bool ParsedH266TrackData::spsppsExists(uint8_t* buff, const int size)
{
    uint8_t* curPos = buff;
    const uint8_t* end = buff + size;
    bool vpsFound = false;
    bool spsFound = false;
    bool ppsFound = false;
    while (curPos < end - m_nalSize)
    {
        uint32_t elSize;
        if (m_nalSize == 4)
        {
            const auto cur32 = reinterpret_cast<uint32_t*>(curPos);
            elSize = my_ntohl(*cur32);
        }
        else
            elSize = (curPos[0] << 16l) + (curPos[1] << 8l) + curPos[2];
        const auto nalUnitType = static_cast<VvcUnit::NalType>(curPos[m_nalSize + 1] >> 3);
        if (nalUnitType == VvcUnit::NalType::VPS)
            vpsFound = true;
        else if (nalUnitType == VvcUnit::NalType::SPS)
            spsFound = true;
        else if (nalUnitType == VvcUnit::NalType::PPS)
            ppsFound = true;
        curPos += elSize + m_nalSize;
    }
    return vpsFound && spsFound && ppsFound;
}

// ------------ VC-1 ---------------

static constexpr int MS_BIT_MAP_HEADER_SIZE = 40;
ParsedVC1TrackData::ParsedVC1TrackData(uint8_t* buff, const int size) : ParsedTrackPrivData(buff, size)
{
    if (size < MS_BIT_MAP_HEADER_SIZE)
        THROW(ERR_MATROSKA_PARSE, "Matroska parse error: Invalid or unsupported VC-1 stream");
    const uint8_t* curBuf = buff + MS_BIT_MAP_HEADER_SIZE;
    const uint8_t dataLen = *curBuf++;
    for (int i = 0; i < dataLen; i++) m_seqHeader.push_back(*curBuf++);
    m_firstPacket = true;
}

void ParsedVC1TrackData::extractData(AVPacket* pkt, uint8_t* buff, const int size)
{
    pkt->size = size + (m_firstPacket ? static_cast<int>(m_seqHeader.size()) : 0);
    const bool addFrameHdr = !(size >= 4 && buff[0] == 0 && buff[1] == 0 && buff[2] == 1);
    if (addFrameHdr)
        pkt->size += 4;
    pkt->data = new uint8_t[pkt->size];
    uint8_t* dst = pkt->data;
    if (m_firstPacket && !m_seqHeader.empty())
    {
        memcpy(dst, m_seqHeader.data(), m_seqHeader.size());
        dst += m_seqHeader.size();
    }
    if (addFrameHdr)
    {
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 1;
        *dst++ = 0x0d;
    }
    // TODO: check compiler warning C6386 : Buffer overrun while writing to 'dst' :
    // the writable size is 'size*1' bytes, but '4' bytes might be written.
    memcpy(dst, buff, size);
    m_firstPacket = false;
}

// ------------ AAC --------------

ParsedAACTrackData::ParsedAACTrackData(uint8_t* buff, const int size) : ParsedTrackPrivData(buff, size)
{
    m_aacRaw.m_id = 1;  // MPEG2
    m_aacRaw.m_layer = 0;
    m_aacRaw.m_rdb = 0;
    m_aacRaw.readConfig(buff, size);
}

void ParsedAACTrackData::extractData(AVPacket* pkt, uint8_t* buff, const int size)
{
    pkt->size = size + AAC_HEADER_LEN;
    pkt->data = new uint8_t[pkt->size];
    m_aacRaw.buildADTSHeader(pkt->data, size + AAC_HEADER_LEN);
    memcpy(pkt->data + AAC_HEADER_LEN, buff, size);
}

// ------------ LPCM --------------
ParsedLPCMTrackData::ParsedLPCMTrackData(MatroskaTrack* track)
    : ParsedTrackPrivData(track->codec_priv, track->codec_priv_size)
{
    m_convertBytes = strEndWith(track->codec_id, "/BIG");
    const auto audiotrack = reinterpret_cast<MatroskaAudioTrack*>(track);
    m_channels = audiotrack->channels;
    m_bitdepth = audiotrack->bitdepth;

    buildWaveHeader(m_waveBuffer, audiotrack->samplerate, m_channels, m_channels >= 6, m_bitdepth);

    if (track->codec_priv_size == sizeof(WAVEFORMATPCMEX))
        memcpy(m_waveBuffer.data() + 20, track->codec_priv, track->codec_priv_size);
}

void ParsedLPCMTrackData::extractData(AVPacket* pkt, uint8_t* buff, const int size)
{
    pkt->size = size + static_cast<int>(m_waveBuffer.size());
    pkt->data = new uint8_t[pkt->size];
    uint8_t* dst = pkt->data;
    if (!m_waveBuffer.isEmpty())
    {
        memcpy(dst, m_waveBuffer.data(), m_waveBuffer.size());
        dst += m_waveBuffer.size();
        m_waveBuffer.clear();
    }
    if (m_convertBytes)
        toLittleEndian(dst, buff, size, m_bitdepth);
    else
        memcpy(dst, buff, size);
}

// ------------ AC3 ---------------

ParsedAC3TrackData::ParsedAC3TrackData(uint8_t* buff, const int size) : ParsedTrackPrivData(buff, size)
{
    m_firstPacket = true;
    m_shortHeaderMode = false;
}

void ParsedAC3TrackData::extractData(AVPacket* pkt, uint8_t* buff, const int size)
{
    if (m_firstPacket && size > 2)
    {
        if (AV_RB16(buff) != 0x0b77)
            m_shortHeaderMode = true;
    }
    m_firstPacket = false;
    pkt->size = size + (m_shortHeaderMode ? 2 : 0);
    pkt->data = new uint8_t[pkt->size];
    uint8_t* dst = pkt->data;
    if (m_shortHeaderMode)
    {
        *dst++ = 0x0b;
        *dst++ = 0x77;
    }
    memcpy(dst, buff, size);
}

// ------------ SRT ---------------

ParsedSRTTrackData::ParsedSRTTrackData(uint8_t* buff, const int size) : ParsedTrackPrivData(buff, size)
{
    m_packetCnt = 0;
}

void ParsedSRTTrackData::extractData(AVPacket* pkt, uint8_t* buff, const int size)
{
    std::string prefix;
    if (m_packetCnt == 0)
        prefix = "\xEF\xBB\xBF";  // UTF-8 header
    prefix += int32ToStr(++m_packetCnt);
    prefix += "\n";
    prefix += floatToTime(static_cast<double>(pkt->pts) / INTERNAL_PTS_FREQ, ',');
    prefix += " --> ";
    prefix += floatToTime(static_cast<double>(pkt->pts + pkt->duration) / INTERNAL_PTS_FREQ, ',');
    prefix += '\n';
    const std::string postfix = "\n\n";
    pkt->size = static_cast<int>(size + prefix.length() + postfix.length());
    pkt->data = new uint8_t[pkt->size];
    memcpy(pkt->data, prefix.c_str(), prefix.length());
    memcpy(pkt->data + prefix.length(), buff, size);
    memcpy(pkt->data + prefix.length() + size, postfix.c_str(), postfix.length());
}

// ------------ PG ---------------
void ParsedPGTrackData::extractData(AVPacket* pkt, uint8_t* buff, const int size)
{
    static constexpr int PG_HEADER_SIZE = 10;

    const uint8_t* curPtr = buff;
    const uint8_t* end = buff + size;
    int blocks = 0;
    while (curPtr <= end - 3)
    {
        const uint16_t blockSize = AV_RB16(curPtr + 1) + 3;
        if (blockSize == 0)
            break;
        curPtr += blockSize;
        blocks++;
    }
    // assert(curPtr == end);
    if (curPtr != end)
    {
        pkt->size = 0;
        pkt->data = nullptr;
        return;  // ignore invalid packet
    }

    pkt->size = size + PG_HEADER_SIZE * blocks;
    pkt->data = new uint8_t[pkt->size];
    curPtr = buff;
    uint8_t* dst = pkt->data;
    while (curPtr <= end - 3)
    {
        const uint16_t blockSize = AV_RB16(curPtr + 1) + 3;

        dst[0] = 'P';
        dst[1] = 'G';
        const auto ptsDts = reinterpret_cast<uint32_t*>(dst + 2);
        ptsDts[0] = my_htonl(static_cast<uint32_t>(internalClockToPts(pkt->pts)));
        ptsDts[1] = 0;
        dst += PG_HEADER_SIZE;
        memcpy(dst, curPtr, blockSize);
        curPtr += blockSize;
        dst += blockSize;
    }
    assert(dst == pkt->data + pkt->size);
}
