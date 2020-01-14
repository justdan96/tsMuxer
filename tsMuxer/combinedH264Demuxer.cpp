
#include "combinedH264Demuxer.h"

#include "abstractStreamReader.h"
#include "avCodecs.h"
#include "nalUnits.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;

static const int MVC_STREAM_INDEX = 1;
static const int AVC_STREAM_INDEX = 2;
static const int MAX_TMP_BUFFER_SIZE = 128;

// ------------------------ CombinedH264Reader -------------------------------

CombinedH264Reader::CombinedH264Reader()
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
                                          int64_t& discardSize)
{
    if (m_avcStreamIndex >= 0)
        demuxedData[m_avcStreamIndex].append(data, dataEnd - data);
    else
        discardSize += dataEnd - data;
}

void CombinedH264Reader::addDataToSecondary(const uint8_t* data, const uint8_t* dataEnd, DemuxedData& demuxedData,
                                            int64_t& discardSize)
{
    if (m_mvcStreamIndex >= 0)
        demuxedData[m_mvcStreamIndex].append(data, dataEnd - data);
    else
        discardSize += dataEnd - data;
}

CombinedH264Reader::ReadState CombinedH264Reader::detectStreamByNal(const uint8_t* data, const uint8_t* dataEnd)
{
    uint8_t nalType = *data & 0x1f;
    if (nalType == nuEOSeq)
    {
        return ReadState_Both;
    }
    else if (nalType == nuDRD || nalType == nuSliceExt)
    {
        return ReadState_Secondary;
    }
    else if (nalType == nuSubSPS)
    {
        SPSUnit sps;
        sps.decodeBuffer(data, dataEnd);
        if (sps.deserialize() == NOT_ENOUGH_BUFFER)
            return ReadState_NeedMoreData;
        m_mvcSPS = sps.seq_parameter_set_id;
        return ReadState_Secondary;
    }
    else if (nalType == nuPPS)
    {
        PPSUnit pps;
        pps.decodeBuffer(data, dataEnd);
        if (pps.deserialize() == NOT_ENOUGH_BUFFER)
            return ReadState_NeedMoreData;
        if (pps.seq_parameter_set_id == m_mvcSPS)
            return ReadState_Secondary;
        else
            return ReadState_Primary;
    }
    else if (nalType == nuSEI)
    {
        SEIUnit sei;
        sei.decodeBuffer(data, dataEnd);
        int rez = sei.isMVCSEI();
        if (rez == NOT_ENOUGH_BUFFER)
            return ReadState_NeedMoreData;
        else if (rez == 0)
            return ReadState_Primary;
        else
            return ReadState_Secondary;
    }
    else
        return ReadState_Primary;
    // return ReadState_Unknown;
}

void CombinedH264Reader::fillPids(const PIDSet& acceptedPIDs, int pid)
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
    : CombinedH264Reader(), AbstractDemuxer(), m_readManager(readManager)
{
    m_bufferedReader = (const_cast<BufferedReaderManager&>(m_readManager)).getReader(streamName);
    m_readerID = m_bufferedReader->createReader(MAX_TMP_BUFFER_SIZE);
    if (m_bufferedReader == 0)
        THROW(ERR_COMMON,
              "TS demuxer can't accept reader because this reader does not support BufferedReader interface");
    m_lastReadRez = 0;
    m_dataProcessed = 0;
}

CombinedH264Demuxer::~CombinedH264Demuxer() { m_bufferedReader->deleteReader(m_readerID); }

void CombinedH264Demuxer::getTrackList(std::map<uint32_t, TrackInfo>& trackList)
{
    trackList.insert(std::make_pair(MVC_STREAM_INDEX, TrackInfo(CODEC_V_MPEG4_H264_DEP, "", 0)));
    trackList.insert(std::make_pair(AVC_STREAM_INDEX, TrackInfo(CODEC_V_MPEG4_H264, "", 0)));
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
    const uint8_t* curNal = data;

    const uint8_t* nextNal = NALUnit::findNALWithStartCode(curNal + 3, dataEnd, true);
    while (curNal < dataEnd - 4)
    {
        int prefixLen = getPrefixLen(curNal, dataEnd);
        if (prefixLen != 0)
        {
            m_state = detectStreamByNal(curNal + prefixLen, nextNal);
            if (m_state == ReadState_NeedMoreData)
            {
                if (dataEnd - curNal < MAX_TMP_BUFFER_SIZE)
                {
                    m_tmpBuffer.append(curNal, dataEnd - curNal);
                    return 0;
                }
                else
                {
                    // some error in a stream, just ignore
                    m_state = ReadState_Primary;
                }
            }
        }
        if (m_state == ReadState_Both)
        {
            addDataToPrimary(curNal, nextNal, demuxedData, discardSize);
            addDataToSecondary(curNal, nextNal, demuxedData, discardSize);
        }
        else if (m_state == ReadState_Primary)
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
            if (m_state == ReadState_Primary)
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

    BufferedFileReader* fileReader = dynamic_cast<BufferedFileReader*>(m_bufferedReader);

    if (!m_bufferedReader->openStream(m_readerID, streamName.c_str()))
        THROW(ERR_FILE_NOT_FOUND, "Can't open stream " << streamName);

    m_dataProcessed = 0;
}

void CombinedH264Demuxer::readClose() {}

uint64_t CombinedH264Demuxer::getDemuxedSize() { return m_dataProcessed; }

void CombinedH264Demuxer::setFileIterator(FileNameIterator* itr)
{
    BufferedFileReader* br = dynamic_cast<BufferedFileReader*>(m_bufferedReader);
    if (br)
        br->setFileIterator(itr, m_readerID);
    else if (itr != 0)
        THROW(ERR_COMMON, "Can not set file iterator. Reader does not support bufferedReader interface.");
}

// ------------------------------ CombinedH264Filter -----------------------------------

CombinedH264Filter::CombinedH264Filter(int demuxedPID) : CombinedH264Reader(), SubTrackFilter(demuxedPID) {}

int CombinedH264Filter::demuxPacket(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, AVPacket& avPacket)
{
    if (m_firstDemuxCall)
    {
        fillPids(acceptedPIDs, m_srcPID);
        m_firstDemuxCall = false;
    }

    const uint8_t* curNal = avPacket.data;
    const uint8_t* dataEnd = avPacket.data + avPacket.size;
    const uint8_t* nextNal = NALUnit::findNALWithStartCode(curNal + 3, dataEnd, true);
    int64_t discardSize = 0;
    while (curNal < dataEnd - 4)
    {
        int prefixLen = getPrefixLen(curNal, dataEnd);
        if (prefixLen != 0)
            m_state = detectStreamByNal(curNal + prefixLen, nextNal);
        if (m_state == ReadState_Both)
        {
            addDataToPrimary(curNal, nextNal, demuxedData, discardSize);
            addDataToSecondary(curNal, nextNal, demuxedData, discardSize);
        }
        else if (m_state == ReadState_Primary)
            addDataToPrimary(curNal, nextNal, demuxedData, discardSize);
        else
            addDataToSecondary(curNal, nextNal, demuxedData, discardSize);
        curNal = nextNal;
        nextNal = NALUnit::findNALWithStartCode(curNal + 3, dataEnd, true);
    }

    return avPacket.size - discardSize;
}
