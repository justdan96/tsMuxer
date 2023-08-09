
#include "combinedH264Demuxer.h"

#include "abstractStreamReader.h"
#include "avCodecs.h"
#include "nalUnits.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;

static constexpr int MVC_STREAM_INDEX = 1;
static constexpr int AVC_STREAM_INDEX = 2;
static constexpr int MAX_TMP_BUFFER_SIZE = 128;

// ------------------------ CombinedH264Reader -------------------------------

CombinedH264Reader::CombinedH264Reader() : m_state(ReadState::Primary), m_demuxedPID(0)
{
    m_firstDemuxCall = true;
    m_mvcSPS = -1;
    m_mvcStreamIndex = -1;
    m_avcStreamIndex = -1;
}

int CombinedH264Reader::getPrefixLen(const uint8_t* pos, const uint8_t* end)
{
    if (end - pos < 4)
        return 0;
    if (pos[0] == 0 && pos[1] == 0)
    {
        if (pos[2] == 0 && pos[3] == 1)
            return 4;
        else if (pos[2] == 1)
            return 3;
        else
            return 0;
    }
    else
        return 0;
}

void CombinedH264Reader::addDataToPrimary(const uint8_t* data, const uint8_t* dataEnd, DemuxedData& demuxedData,
                                          int64_t& discardSize) const
{
    if (m_avcStreamIndex >= 0)
        demuxedData[m_avcStreamIndex].append(data, dataEnd - data);
    else
        discardSize += dataEnd - data;
}

void CombinedH264Reader::addDataToSecondary(const uint8_t* data, const uint8_t* dataEnd, DemuxedData& demuxedData,
                                            int64_t& discardSize) const
{
    if (m_mvcStreamIndex >= 0)
        demuxedData[m_mvcStreamIndex].append(data, dataEnd - data);
    else
        discardSize += dataEnd - data;
}

CombinedH264Reader::ReadState CombinedH264Reader::detectStreamByNal(const uint8_t* data, const uint8_t* dataEnd)
{
    const auto nalType = static_cast<NALUnit::NALType>(*data & 0x1f);

    switch (nalType)
    {
    case NALUnit::NALType::nuEOSeq:
        return ReadState::Both;
    case NALUnit::NALType::nuDRD:
    case NALUnit::NALType::nuSliceExt:
        return ReadState::Secondary;

    case NALUnit::NALType::nuSubSPS:
    {
        SPSUnit sps;
        sps.decodeBuffer(data, dataEnd);
        if (sps.deserialize() == NOT_ENOUGH_BUFFER)
            return ReadState::NeedMoreData;
        m_mvcSPS = sps.seq_parameter_set_id;
        return ReadState::Secondary;
    }
    case NALUnit::NALType::nuPPS:
    {
        PPSUnit pps;
        pps.decodeBuffer(data, dataEnd);
        if (pps.deserialize() == NOT_ENOUGH_BUFFER)
            return ReadState::NeedMoreData;
        if (pps.seq_parameter_set_id == m_mvcSPS)
            return ReadState::Secondary;
        else
            return ReadState::Primary;
    }
    case NALUnit::NALType::nuSEI:
    {
        SEIUnit sei;
        sei.decodeBuffer(data, dataEnd);
        const int rez = sei.isMVCSEI();
        if (rez == NOT_ENOUGH_BUFFER)
            return ReadState::NeedMoreData;
        else if (rez == 0)
            return ReadState::Primary;
        else
            return ReadState::Secondary;
    }
    default:
        return ReadState::Primary;
    }
}

void CombinedH264Reader::fillPids(const PIDSet& acceptedPIDs, const int pid)
{
    m_mvcStreamIndex = SubTrackFilter::pidToSubPid(pid, MVC_STREAM_INDEX);
    m_avcStreamIndex = SubTrackFilter::pidToSubPid(pid, AVC_STREAM_INDEX);
    if (acceptedPIDs.find(m_mvcStreamIndex) == acceptedPIDs.end())
        m_mvcStreamIndex = -1;
    if (acceptedPIDs.find(m_avcStreamIndex) == acceptedPIDs.end())
        m_avcStreamIndex = -1;
}

// --------------------------------------------- CombinedH264Demuxer ---------------------------

CombinedH264Demuxer::CombinedH264Demuxer(const BufferedReaderManager& readManager, const char* streamName)
    : AbstractDemuxer(), CombinedH264Reader(), m_readManager(readManager)
{
    m_bufferedReader = m_readManager.getReader(streamName);
    m_readerID = m_bufferedReader->createReader(MAX_TMP_BUFFER_SIZE);
    if (m_bufferedReader == nullptr)
        THROW(ERR_COMMON,
              "TS demuxer can't accept reader because this reader does not support BufferedReader interface");
    m_lastReadRez = 0;
    m_dataProcessed = 0;
}

CombinedH264Demuxer::~CombinedH264Demuxer() { m_bufferedReader->deleteReader(m_readerID); }

void CombinedH264Demuxer::getTrackList(std::map<uint32_t, TrackInfo>& trackList)
{
    trackList[MVC_STREAM_INDEX] = TrackInfo(CODEC_V_MPEG4_H264_DEP, "", 0);
    trackList[AVC_STREAM_INDEX] = TrackInfo(CODEC_V_MPEG4_H264, "", 0);
}

int CombinedH264Demuxer::simpleDemuxBlock(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, int64_t& discardSize)
{
    if (m_firstDemuxCall)
    {
        fillPids(acceptedPIDs, 0);
        m_firstDemuxCall = false;
    }

    discardSize = 0;

    uint32_t readedBytes;
    int readRez = 0;
    bool isFirstBlock = false;
    uint8_t* data = m_bufferedReader->readBlock(m_readerID, readedBytes, readRez, &isFirstBlock);  // blocked read mode
    if (readRez == BufferedFileReader::DATA_NOT_READY)
    {
        m_lastReadRez = readRez;
        return BufferedFileReader::DATA_NOT_READY;
    }

    if (readedBytes + m_tmpBuffer.size() == 0 || (readedBytes == 0 && m_lastReadRez == BufferedReader::DATA_EOF))
    {
        m_lastReadRez = readRez;
        return BufferedReader::DATA_EOF;
    }
    if (readedBytes > 0)
        m_bufferedReader->notify(m_readerID, readedBytes);
    m_lastReadRez = readRez;
    data += MAX_TMP_BUFFER_SIZE;
    uint8_t* dataEnd = data + readedBytes;
    if (m_tmpBuffer.size() > 0)
    {
        data -= m_tmpBuffer.size();
        memcpy(data, m_tmpBuffer.data(), m_tmpBuffer.size());
        m_tmpBuffer.clear();
    }
    uint8_t* curNal = data;

    uint8_t* nextNal = NALUnit::findNALWithStartCode(curNal + 3, dataEnd, true);
    while (curNal < dataEnd - 4)
    {
        const int prefixLen = getPrefixLen(curNal, dataEnd);
        if (prefixLen != 0)
        {
            m_state = detectStreamByNal(curNal + prefixLen, nextNal);
            if (m_state == ReadState::NeedMoreData)
            {
                if (dataEnd - curNal < MAX_TMP_BUFFER_SIZE)
                {
                    m_tmpBuffer.append(curNal, dataEnd - curNal);
                    return 0;
                }
                else
                {
                    // some error in a stream, just ignore
                    m_state = ReadState::Primary;
                }
            }
        }
        if (m_state == ReadState::Both)
        {
            addDataToPrimary(curNal, nextNal, demuxedData, discardSize);
            addDataToSecondary(curNal, nextNal, demuxedData, discardSize);
        }
        else if (m_state == ReadState::Primary)
            addDataToPrimary(curNal, nextNal, demuxedData, discardSize);
        else
            addDataToSecondary(curNal, nextNal, demuxedData, discardSize);
        curNal = nextNal;
        nextNal = NALUnit::findNALWithStartCode(curNal + 3, dataEnd, true);
    }

    if (curNal < dataEnd)
    {
        if (m_lastReadRez == BufferedReader::DATA_EOF)
        {
            if (m_state == ReadState::Primary)
                addDataToPrimary(curNal, dataEnd, demuxedData, discardSize);
            else
                addDataToSecondary(curNal, dataEnd, demuxedData, discardSize);
        }
        else
        {
            m_tmpBuffer.append(curNal, dataEnd - curNal);
        }
    }

    return 0;
}

void CombinedH264Demuxer::openFile(const std::string& streamName)
{
    readClose();

    if (!m_bufferedReader->openStream(m_readerID, streamName.c_str()))
        THROW(ERR_FILE_NOT_FOUND, "Can't open stream " << streamName);

    m_dataProcessed = 0;
}

void CombinedH264Demuxer::readClose() {}

uint64_t CombinedH264Demuxer::getDemuxedSize() { return m_dataProcessed; }

void CombinedH264Demuxer::setFileIterator(FileNameIterator* itr)
{
    const auto br = dynamic_cast<BufferedFileReader*>(m_bufferedReader);
    if (br)
        br->setFileIterator(itr, m_readerID);
    else if (itr != nullptr)
        THROW(ERR_COMMON, "Can not set file iterator. Reader does not support bufferedReader interface.");
}

// ------------------------------ CombinedH264Filter -----------------------------------

CombinedH264Filter::CombinedH264Filter(const int demuxedPID) : SubTrackFilter(demuxedPID), CombinedH264Reader() {}

int CombinedH264Filter::demuxPacket(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, AVPacket& avPacket)
{
    if (m_firstDemuxCall)
    {
        fillPids(acceptedPIDs, m_srcPID);
        m_firstDemuxCall = false;
    }

    uint8_t* curNal = avPacket.data;
    uint8_t* dataEnd = avPacket.data + avPacket.size;
    uint8_t* nextNal = NALUnit::findNALWithStartCode(curNal + 3, dataEnd, true);
    int64_t discardSize = 0;
    while (curNal < dataEnd - 4)
    {
        const int prefixLen = getPrefixLen(curNal, dataEnd);
        if (prefixLen != 0)
            m_state = detectStreamByNal(curNal + prefixLen, nextNal);
        if (m_state == ReadState::Both)
        {
            addDataToPrimary(curNal, nextNal, demuxedData, discardSize);
            addDataToSecondary(curNal, nextNal, demuxedData, discardSize);
        }
        else if (m_state == ReadState::Primary)
            addDataToPrimary(curNal, nextNal, demuxedData, discardSize);
        else
            addDataToSecondary(curNal, nextNal, demuxedData, discardSize);
        curNal = nextNal;
        nextNal = NALUnit::findNALWithStartCode(curNal + 3, dataEnd, true);
    }

    return avPacket.size - static_cast<int>(discardSize);
}
