
#include "matroskaParser.h"

#include <fs/systemlog.h>

#include "bitStream.h"
#include "hevc.h"
#include "nalUnits.h"
#include "vodCoreException.h"
#include "vod_common.h"
#include "wave.h"

static const int LPCM_HEADER_LEN = 4;

using namespace wave_format;

// ------------ H-264 ---------------

ParsedH264TrackData::ParsedH264TrackData(uint8_t* buff, int size) : ParsedTrackPrivData(buff, size)
{
    m_firstExtract = true;
    if (buff == 0)
        return;
    BitStreamReader bitReader;
    try
    {
        bitReader.setBuffer(buff, buff + size);
        bitReader.skipBits(8);  // reserved
        int profile = bitReader.getBits(8);
        bitReader.skipBits(8);  // reserved
        int level = bitReader.getBits(8);
        bitReader.skipBits(6);  // reserved
        m_nalSize = bitReader.getBits(2) + 1;
        bitReader.skipBits(3);  // reserved
        int spsCnt = bitReader.getBits(5);
        for (int i = 0; i < spsCnt; i++)
        {
            int spsLen = bitReader.getBits(16);
            if (spsLen > 0)
            {
                m_spsPpsList.push_back(std::vector<uint8_t>());
                std::vector<uint8_t>& curData = m_spsPpsList[m_spsPpsList.size() - 1];
                for (int j = 0; j < spsLen; j++) curData.push_back(bitReader.getBits(8));
            }
        }
        int ppsCnt = bitReader.getBits(8);
        for (int i = 0; i < ppsCnt; i++)
        {
            int ppsLen = bitReader.getBits(16);
            if (ppsLen > 0)
            {
                m_spsPpsList.push_back(std::vector<uint8_t>());
                std::vector<uint8_t>& curData = m_spsPpsList[m_spsPpsList.size() - 1];
                for (int j = 0; j < ppsLen; j++) curData.push_back(bitReader.getBits(8));
            }
        }
    }
    catch (BitStreamException)
    {
        THROW(ERR_MATROSKA_PARSE, "Can't parse H.264 private data");
    }
};

void ParsedH264TrackData::writeNalHeader(uint8_t*& dst)
{
    for (int i = 0; i < 3; i++) *dst++ = 0;
    *dst++ = 1;
}
int ParsedH264TrackData::getSPSPPSLen()
{
    int rez = 0;
    for (int i = 0; i < m_spsPpsList.size(); i++) rez += m_spsPpsList[i].size() + 4;
    return rez;
}

int ParsedH264TrackData::writeSPSPPS(uint8_t* dst)
{
    uint8_t* start = dst;
    for (int i = 0; i < m_spsPpsList.size(); i++)
    {
        writeNalHeader(dst);
        memcpy(dst, &m_spsPpsList[i][0], m_spsPpsList[i].size());
        dst += m_spsPpsList[i].size();
    }
    return dst - start;
}

bool ParsedH264TrackData::spsppsExists(uint8_t* buff, int size)
{
    uint8_t* curPos = buff;
    uint8_t* end = buff + size;
    bool spsFound = false;
    bool ppsFound = false;
    while (curPos < end - m_nalSize)
    {
        uint32_t elSize = 0;
        if (m_nalSize == 4)
        {
            uint32_t* cur32 = (uint32_t*)curPos;
            elSize = my_ntohl(*cur32);
        }
        else
            elSize = (curPos[0] << 16l) + (curPos[1] << 8l) + curPos[2];
        int nalUnitType = curPos[m_nalSize] & 0x1f;
        if (nalUnitType == nuSPS)
            spsFound = true;
        else if (nalUnitType == nuPPS)
            ppsFound = true;
        curPos += elSize + m_nalSize;
    }
    return spsFound && ppsFound;
}

void ParsedH264TrackData::extractData(AVPacket* pkt, uint8_t* buff, int size)
{
    int newBufSize = size;
    uint8_t* curPos = buff;
    uint8_t* end = buff + size;
    int elements = 0;

    if (m_firstExtract && spsppsExists(buff, size))
        m_firstExtract = false;

    if (m_firstExtract)
        newBufSize += getSPSPPSLen();

    while (curPos <= end - m_nalSize)
    {
        uint32_t elSize = 0;
        if (m_nalSize == 4)
        {
            uint32_t* cur32 = (uint32_t*)curPos;
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
        curPos = end;
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
            uint32_t* cur32 = (uint32_t*)curPos;
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
        memcpy(dst, curPos + m_nalSize, FFMIN(elSize, end - curPos));
        curPos += elSize + m_nalSize;
        dst += elSize;
    }
    m_firstExtract = false;
}

// ----------- H.265 -----------------
ParsedH265TrackData::ParsedH265TrackData(uint8_t* buff, int size) : ParsedH264TrackData(0, 0)
{
    m_spsPpsList = hevc_extract_priv_data(buff, size, &m_nalSize);
}

bool ParsedH265TrackData::spsppsExists(uint8_t* buff, int size)
{
    uint8_t* curPos = buff;
    uint8_t* end = buff + size;
    bool vpsFound = false;
    bool spsFound = false;
    bool ppsFound = false;
    while (curPos < end - m_nalSize)
    {
        uint32_t elSize = 0;
        if (m_nalSize == 4)
        {
            uint32_t* cur32 = (uint32_t*)curPos;
            elSize = my_ntohl(*cur32);
        }
        else
            elSize = (curPos[0] << 16l) + (curPos[1] << 8l) + curPos[2];
        int nalUnitType = (curPos[m_nalSize] >> 1) & 0x3f;
        if (nalUnitType == NAL_VPS)
            vpsFound = true;
        else if (nalUnitType == NAL_SPS)
            spsFound = true;
        else if (nalUnitType == NAL_PPS)
            ppsFound = true;
        curPos += elSize + m_nalSize;
    }
    return vpsFound && spsFound && ppsFound;
}

// ------------ VC-1 ---------------

const static int MS_BIT_MAP_HEADER_SIZE = 40;
ParsedVC1TrackData::ParsedVC1TrackData(uint8_t* buff, int size) : ParsedTrackPrivData(buff, size)
{
    if (size < MS_BIT_MAP_HEADER_SIZE)
        THROW(ERR_MATROSKA_PARSE, "Matroska parse error: Invalid or unsupported VC-1 stream");
    uint8_t* curBuf = buff + MS_BIT_MAP_HEADER_SIZE;
    uint8_t dataLen = *curBuf++;
    for (int i = 0; i < dataLen; i++) m_seqHeader.push_back(*curBuf++);
    m_firstPacket = true;
}

void ParsedVC1TrackData::extractData(AVPacket* pkt, uint8_t* buff, int size)
{
    // pkt->size = size + 4 + (m_firstPacket ? m_seqHeader.size() : 0);
    pkt->size = size + (m_firstPacket ? m_seqHeader.size() : 0);
    bool addFrameHdr = !(size >= 4 && buff[0] == 0 && buff[1] == 0 && buff[2] == 1);
    if (addFrameHdr)
        pkt->size += 4;
    pkt->data = new uint8_t[pkt->size];
    uint8_t* dst = pkt->data;
    if (m_firstPacket && m_seqHeader.size() > 0)
    {
        memcpy(dst, &m_seqHeader[0], m_seqHeader.size());
        dst += m_seqHeader.size();
    }
    if (addFrameHdr)
    {
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 1;
        *dst++ = 0x0d;
    }
    memcpy(dst, buff, size);
    m_firstPacket = false;
}

// ------------ AAC --------------

ParsedAACTrackData::ParsedAACTrackData(uint8_t* buff, int size) : ParsedTrackPrivData(buff, size)
{
    m_aacRaw.m_id = 1;  // MPEG2
    m_aacRaw.m_layer = 0;
    m_aacRaw.m_rdb = 0;
    m_aacRaw.readConfig(buff, size);
}

void ParsedAACTrackData::extractData(AVPacket* pkt, uint8_t* buff, int size)
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
    MatroskaAudioTrack* audiotrack = (MatroskaAudioTrack*)track;
    m_channels = audiotrack->channels;
    m_bitdepth = audiotrack->bitdepth;

    buildWaveHeader(m_waveBuffer, audiotrack->samplerate, m_channels, m_channels >= 6, m_bitdepth);

    if (track->codec_priv_size == sizeof(WAVEFORMATPCMEX))
        memcpy(m_waveBuffer.data() + 20, track->codec_priv, track->codec_priv_size);
}

void ParsedLPCMTrackData::extractData(AVPacket* pkt, uint8_t* buff, int size)
{
    pkt->size = size + m_waveBuffer.size();
    pkt->data = new uint8_t[pkt->size];
    uint8_t* dst = pkt->data;
    if (!m_waveBuffer.isEmpty())
    {
        memcpy(dst, m_waveBuffer.data(), m_waveBuffer.size());
        dst += m_waveBuffer.size();
        m_waveBuffer.clear();
    }
    if (m_convertBytes)
        wave_format::toLittleEndian(dst, buff, size, m_bitdepth);
    else
        memcpy(dst, buff, size);
}

// ------------ AC3 ---------------

ParsedAC3TrackData::ParsedAC3TrackData(uint8_t* buff, int size) : ParsedTrackPrivData(buff, size)
{
    m_firstPacket = true;
    m_shortHeaderMode = false;
}

void ParsedAC3TrackData::extractData(AVPacket* pkt, uint8_t* buff, int size)
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

ParsedSRTTrackData::ParsedSRTTrackData(uint8_t* buff, int size) : ParsedTrackPrivData(buff, size) { m_packetCnt = 0; }

void ParsedSRTTrackData::extractData(AVPacket* pkt, uint8_t* buff, int size)
{
    std::string prefix;
    if (m_packetCnt == 0)
        prefix = "\xEF\xBB\xBF";  // UTF-8 header
    prefix += int32ToStr(++m_packetCnt);
    prefix += "\n";
    prefix += floatToTime(pkt->pts / 1e9, ',');
    prefix += " --> ";
    prefix += floatToTime((pkt->pts + pkt->duration) / 1e9, ',');
    prefix += '\n';
    std::string postfix = "\n\n";
    pkt->size = size + prefix.length() + postfix.length();
    pkt->data = new uint8_t[pkt->size];
    memcpy(pkt->data, prefix.c_str(), prefix.length());
    memcpy(pkt->data + prefix.length(), buff, size);
    memcpy(pkt->data + prefix.length() + size, postfix.c_str(), postfix.length());
}

// ------------ PG ---------------
void ParsedPGTrackData::extractData(AVPacket* pkt, uint8_t* buff, int size)
{
    static const int PG_HEADER_SIZE = 10;

    uint8_t* curPtr = buff;
    uint8_t* end = buff + size;
    int blocks = 0;
    while (curPtr <= end - 3)
    {
        uint16_t blockSize = AV_RB16(curPtr + 1) + 3;
        curPtr += blockSize;
        blocks++;
    }
    assert(curPtr == end);
    if (curPtr != end)
    {
        pkt->size = 0;
        pkt->data = 0;
        return;  // ignore invalid packet
    }

    pkt->size = size + PG_HEADER_SIZE * blocks;
    pkt->data = new uint8_t[pkt->size];
    curPtr = buff;
    uint8_t* dst = pkt->data;
    while (curPtr <= end - 3)
    {
        uint16_t blockSize = AV_RB16(curPtr + 1) + 3;

        dst[0] = 'P';
        dst[1] = 'G';
        uint32_t* ptsDts = (uint32_t*)(dst + 2);
        ptsDts[0] = my_htonl((pkt->pts * 90000) / 1000000000ll);
        ptsDts[1] = 0;
        dst += PG_HEADER_SIZE;
        memcpy(dst, curPtr, blockSize);
        curPtr += blockSize;
        dst += blockSize;
    }
    assert(dst == pkt->data + pkt->size);
}
