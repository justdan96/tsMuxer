
#include "tsDemuxer.h"

#include "aac.h"
#include "abstractStreamReader.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;

bool isM2TSExt(const std::string& streamName)
{
    const string sName = strToLowerCase(unquoteStr(streamName));
    return strEndWith(sName, ".m2ts") || strEndWith(sName, ".mts") || strEndWith(sName, ".ssif");
}

TSDemuxer::TSDemuxer(const BufferedReaderManager& readManager, const char* streamName)
    : m_readManager(readManager),
      m_curPos(nullptr),
      m_pmtPid(0),
      m_codecReady(false),
      m_readCnt(0),
      m_dataProcessed(0),
      m_notificated(false),
      m_tmpBuffer{}
{
    m_firstPCRTime = -1;
    m_bufferedReader = m_readManager.getReader(streamName);
    m_readerID = m_bufferedReader->createReader(TS_FRAME_SIZE);
    if (m_bufferedReader == nullptr)
        THROW(ERR_COMMON,
              "TS demuxer can't accept reader because this reader does not support BufferedReader interface");
    m_scale = 1;
    m_nptPos = 0;
    m_tmpBufferLen = 0;
    m_m2tsMode = false;
    m_lastReadRez = 0;
    m_m2tsHdrDiscarded = false;
    m_firstPTS = -1;
    m_lastPTS = -1;
    m_firstVideoPTS = -1;
    m_lastVideoPTS = -1;
    m_lastVideoDTS = -1;
    m_videoDtsGap = -1;
    m_firstCall = true;
    m_prevFileLen = 0;
    m_curFileNum = 0;
    m_lastPCRVal = -1;
    m_nonMVCVideoFound = false;
    m_firstDemuxCall = true;
    memset(m_acceptedPidCache, 0, sizeof(m_acceptedPidCache));
}

bool TSDemuxer::mvcContinueExpected() const { return !m_nonMVCVideoFound && strEndWith(m_streamNameLow, "ssif"); }

void TSDemuxer::getTrackList(std::map<uint32_t, TrackInfo>& trackList)
{
    uint8_t pmtBuffer[4096]{0};
    int pmtBufferLen = 0;
    uint32_t tmpBufferLen = 0;
    uint32_t readedBytes;
    uint32_t totalReadedBytes = 0;
    m_lastReadRez = 0;
    bool firstPAT = true;
    std::map<int, int> nonProcPMTPid;

    bool m2tsHdrDiscarded = false;
    int lastReadRez = 0;
    uint8_t* curPos = nullptr;
    while (totalReadedBytes < DETECT_STREAM_BUFFER_SIZE && lastReadRez != BufferedReader::DATA_EOF)
    {
        lastReadRez = 0;
        uint8_t* data = m_bufferedReader->readBlock(m_readerID, readedBytes, lastReadRez);
        totalReadedBytes += readedBytes;
        data += TS_FRAME_SIZE;
        if (tmpBufferLen > 0)
        {
            memcpy(data - tmpBufferLen, m_tmpBuffer, tmpBufferLen);
            data -= tmpBufferLen;
            readedBytes += tmpBufferLen;
            tmpBufferLen = 0;
        }
        uint8_t* lastFrameAddr = data + readedBytes - TS_FRAME_SIZE;
        TS_program_association_section pat;

        if (m_firstCall && !m_m2tsMode)
        {
            m_m2tsMode = checkForRealM2ts(data, lastFrameAddr);
            m_firstCall = false;
        }

        for (curPos = data; curPos <= lastFrameAddr; curPos += TS_FRAME_SIZE)
        {
            if (!m2tsHdrDiscarded && m_m2tsMode)
                curPos += 4;
            while (*curPos != 0x47 && curPos <= lastFrameAddr) curPos++;
            if (curPos > lastFrameAddr)
            {
                m2tsHdrDiscarded = true;
                break;
            }
            m2tsHdrDiscarded = false;
            auto tsPacket = reinterpret_cast<TSPacket*>(curPos);
            int pid = tsPacket->getPID();

            if (pid == 0)
            {  // PAT
                pat.deserialize(curPos + tsPacket->getHeaderSize(), TS_FRAME_SIZE - tsPacket->getHeaderSize());
                if (firstPAT)
                {
                    firstPAT = false;
                    nonProcPMTPid = pat.pmtPids;
                }
            }
            else if (pat.pmtPids.find(pid) != pat.pmtPids.end())
            {  // PMT
                if (tsPacket->payloadStart || pmtBufferLen > 0)
                {
                    memcpy(pmtBuffer + pmtBufferLen, curPos + tsPacket->getHeaderSize(),
                           TS_FRAME_SIZE - tsPacket->getHeaderSize());
                    pmtBufferLen += TS_FRAME_SIZE - tsPacket->getHeaderSize();
                    if (TS_program_map_section::isFullBuff(pmtBuffer, pmtBufferLen))
                    {
                        m_pmt.deserialize(pmtBuffer, pmtBufferLen);
                        if (m_pmt.video_type != static_cast<int>(StreamType::VIDEO_MVC))
                            m_nonMVCVideoFound = true;
                        pmtBufferLen = 0;
                        for (const auto& [fst, snd] : m_pmt.pidList)
                            trackList.insert(std::make_pair(
                                snd.m_pid, TrackInfo(static_cast<int>(snd.m_streamType), snd.m_lang, 0)));
                        nonProcPMTPid.erase(pid);
                        if (nonProcPMTPid.empty() && !mvcContinueExpected())
                        {  // all pmt pids processed
                            auto br = dynamic_cast<BufferedFileReader*>(m_bufferedReader);
                            if (br)
                                br->incSeek(m_readerID, -static_cast<int64_t>(totalReadedBytes));
                            else
                                THROW(ERR_COMMON, "Function TSDemuxer::getTrackList required bufferedReader!");
                            return;
                        }
                    }
                }
            }
        }
        if (curPos < data + readedBytes)
        {
            tmpBufferLen = static_cast<uint32_t>(data + readedBytes - curPos);
            memmove(m_tmpBuffer, curPos, tmpBufferLen);
        }
    }

    auto br = dynamic_cast<BufferedFileReader*>(m_bufferedReader);
    if (br)
        br->incSeek(m_readerID, -static_cast<int64_t>(totalReadedBytes));
    else
        THROW(ERR_COMMON, "Function TSDemuxer::getTrackList required bufferedReader!");
    return;
}

// static int64_t prevPCR = -1;
// static int pcrFrames = 0;

bool TSDemuxer::isVideoPID(const StreamType streamType)
{
    switch (streamType)
    {
    case StreamType::VIDEO_MPEG1:
    case StreamType::VIDEO_MPEG2:
    case StreamType::VIDEO_MPEG4:
    case StreamType::VIDEO_H264:
    case StreamType::VIDEO_MVC:
    case StreamType::VIDEO_VC1:
    case StreamType::VIDEO_H265:
    case StreamType::VIDEO_H266:
        return true;
    default:
        return false;
    }
}

// static uint64_t prevDts = 0;
// static int ggCnt = 0;

bool TSDemuxer::checkForRealM2ts(uint8_t* buffer, uint8_t* end) const
{
    for (const uint8_t* cur = buffer; cur < end; cur += 192)
        if (cur[4] != 0x47)
            return false;
    for (const uint8_t* cur = buffer; cur < end; cur += 188)
        if (*cur != 0x47)
        {
            LTRACE(LT_WARN, 2, "Warning! The file " << m_streamName << " has a M2TS format.");
            return true;
        }
    return false;
}

int TSDemuxer::simpleDemuxBlock(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, int64_t& discardSize)
{
    if (m_firstDemuxCall)
    {
        for (const unsigned int acceptedPID : acceptedPIDs) m_acceptedPidCache[acceptedPID] = 1;
        m_firstDemuxCall = false;
    }

    uint8_t pmtBuffer[4096]{0};
    int pmtBufferLen = 0;
    MemoryBlock* vect = nullptr;
    int lastPid = -1;

    for (unsigned int acceptedPID : acceptedPIDs) demuxedData[acceptedPID];

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
    if (readedBytes + m_tmpBufferLen == 0 || (readedBytes == 0 && m_lastReadRez == BufferedReader::DATA_EOF))
    {
        m_lastReadRez = readRez;
        return BufferedReader::DATA_EOF;
    }
    if (readedBytes > 0)
        m_bufferedReader->notify(m_readerID, readedBytes);
    m_lastReadRez = readRez;
    data += TS_FRAME_SIZE;
    if (m_tmpBufferLen > 0)
    {
        memcpy(data - m_tmpBufferLen, m_tmpBuffer, m_tmpBufferLen);
        data -= m_tmpBufferLen;
        readedBytes += static_cast<uint32_t>(m_tmpBufferLen);
        m_tmpBufferLen = 0;
    }

    if (isFirstBlock)
    {
        if (m_curFileNum < m_mplsInfo.size())
            m_prevFileLen +=
                static_cast<int64_t>(m_mplsInfo[m_curFileNum].OUT_time - m_mplsInfo[m_curFileNum].IN_time) *
                2;  // in 90Khz clock
        else
        {
            if (m_firstVideoPTS != -1 && m_lastVideoPTS != -1)
                m_prevFileLen += (m_lastVideoPTS - m_firstVideoPTS + m_videoDtsGap);
            else  // no video file
                m_prevFileLen += (m_lastPTS - m_firstPTS);
            ;
        }
        m_firstPTS = -1;
        m_lastPTS = -1;
        m_firstVideoPTS = -1;
        m_lastVideoPTS = -1;
        m_curFileNum++;
    }

    discardSize = 0;

    if (readedBytes < TS_FRAME_SIZE)
    {
        discardSize += readedBytes;
        return 0;
    }

    uint8_t* lastFrameAddr = data + readedBytes - TS_FRAME_SIZE;

    if (m_firstCall && !m_m2tsMode)
    {
        m_m2tsMode = checkForRealM2ts(data, lastFrameAddr);
        m_firstCall = false;
    }

    bool forpmtm2tsHdrDiscarded = m_m2tsHdrDiscarded;
    if (m_pmt.pidList.empty() || mvcContinueExpected())
    {
        TS_program_association_section pat;
        for (m_curPos = data; m_curPos <= lastFrameAddr; m_curPos += TS_FRAME_SIZE)
        {
            if (!forpmtm2tsHdrDiscarded && m_m2tsMode)
                m_curPos += 4;
            while (*m_curPos != 0x47 && m_curPos <= lastFrameAddr) m_curPos++;
            if (m_curPos > lastFrameAddr)
                break;
            forpmtm2tsHdrDiscarded = false;

            const auto tsPacket = reinterpret_cast<TSPacket*>(m_curPos);
            int pid = tsPacket->getPID();
            if (pid == 0)
            {  // PAT
                pat.deserialize(m_curPos + tsPacket->getHeaderSize(), TS_FRAME_SIZE - tsPacket->getHeaderSize());
            }
            else if (pat.pmtPids.find(pid) != pat.pmtPids.end())
            {  // PMT
                if (tsPacket->payloadStart || pmtBufferLen > 0)
                {
                    memcpy(pmtBuffer + pmtBufferLen, m_curPos + tsPacket->getHeaderSize(),
                           TS_FRAME_SIZE - tsPacket->getHeaderSize());
                    pmtBufferLen += TS_FRAME_SIZE - tsPacket->getHeaderSize();
                    if (TS_program_map_section::isFullBuff(pmtBuffer, pmtBufferLen))
                    {
                        m_pmt.deserialize(pmtBuffer, pmtBufferLen);
                        if (m_pmt.video_type != static_cast<int>(StreamType::VIDEO_MVC))
                            m_nonMVCVideoFound = true;
                        pmtBufferLen = 0;
                    }
                }
            }
        }
    }

    for (m_curPos = data; m_curPos <= lastFrameAddr; m_curPos += TS_FRAME_SIZE)
    {
        if (!m_m2tsHdrDiscarded && m_m2tsMode)
        {
            discardSize += 4;
            m_curPos += 4;
        }
        while (m_curPos <= lastFrameAddr && *m_curPos != 0x47)
        {
            discardSize++;
            m_curPos++;
        }
        if (m_curPos > lastFrameAddr)
        {
            m_m2tsHdrDiscarded = true;
            break;
        }
        m_m2tsHdrDiscarded = false;

        const auto tsPacket = reinterpret_cast<TSPacket*>(m_curPos);
        int pid = tsPacket->getPID();
        discardSize += TS_FRAME_SIZE;

        // pcrFrames++;

        if (tsPacket->afExists)
        {
            if (tsPacket->adaptiveField.pcrExist)
            {
                // if (prevPCR == -1)
                //	prevPCR = tsPacket->adaptiveField.getPCR33();

                // prevPCR = tsPacket->adaptiveField.getPCR33();
                // LTRACE(LT_INFO, 2, "pcr: " << prevPCR - 55729419ll);
                // pcrFrames = 0;
            }
        }

        uint8_t* frameData = m_curPos + tsPacket->getHeaderSize();
        const bool pesStartCode = frameData[0] == 0 && frameData[1] == 0 && frameData[2] == 1 && tsPacket->payloadStart;
        if (pesStartCode)
        {
            const auto pesPacket = reinterpret_cast<PESPacket*>(frameData);
            auto streamInfo = m_pmt.pidList.find(pid);

            if ((pesPacket->flagsLo & 0x80) == 0x80)
            {
                const int64_t curPts = pesPacket->getPts();
                int64_t curDts = curPts;

                if ((pesPacket->flagsLo & 0xc0) == 0xc0)
                    curDts = pesPacket->getDts();

                if (m_lastPTS == -1 || curPts > m_lastPTS)
                    m_lastPTS = curPts;

                if (m_firstPTS == -1 || curPts < m_firstPTS)
                    m_firstPTS = curPts;

                if (streamInfo != m_pmt.pidList.end() && isVideoPID(streamInfo->second.m_streamType))
                {
                    if (m_firstVideoPTS == -1 || curPts < m_firstVideoPTS)
                        m_firstVideoPTS = curPts;
                    if (curPts > m_lastVideoPTS)
                        m_lastVideoPTS = curPts;
                    if (m_lastVideoDTS == -1)
                        m_lastVideoDTS = curDts;
                    if (m_videoDtsGap == -1 && curDts > m_lastVideoDTS)
                        m_videoDtsGap = curDts - m_lastVideoDTS;
                }

                if (m_firstPtsTime.find(pid) == m_firstPtsTime.end())
                    m_firstPtsTime[pid] = curPts;

                else if (m_curFileNum == 0 && curPts < m_firstPtsTime[pid])
                    m_firstPtsTime[pid] = curPts;
            }

            if (streamInfo != m_pmt.pidList.end() &&
                streamInfo->second.m_streamType != StreamType::SUB_PGS)  // demux PGS with PES headers
                frameData += pesPacket->getHeaderLength();
            else
            {
                const int64_t ptsBase = m_firstVideoPTS != -1 ? m_firstVideoPTS : m_firstPTS;
                if ((pesPacket->flagsLo & 0xc0) == 0xc0)
                {
                    const int64_t pts = pesPacket->getPts() - ptsBase + m_prevFileLen;
                    const int64_t dts = pesPacket->getDts() - ptsBase + m_prevFileLen;
                    pesPacket->setPtsAndDts(pts, dts);
                }
                else if ((pesPacket->flagsLo & 0x80) == 0x80)
                {
                    const int64_t pts = pesPacket->getPts() - ptsBase + m_prevFileLen;
                    pesPacket->setPts(pts);
                }
            }
        }

        // if (acceptedPIDs.find(pid) == acceptedPIDs.end())
        if (!m_acceptedPidCache[pid])
            continue;

        const int64_t payloadLen = TS_FRAME_SIZE - (frameData - m_curPos);
        if (payloadLen > 0)
        {
            if (pid != lastPid)
            {
                vect = &demuxedData[pid];
                lastPid = pid;
            }
            if (vect != nullptr)
            {
                vect->grow(payloadLen);
                uint8_t* dst = vect->data() + vect->size() - payloadLen;
                memcpy(dst, frameData, payloadLen);
            }
        }
        discardSize -= payloadLen;
    }
    if (m_curPos < data + readedBytes)
    {
        m_tmpBufferLen = data + readedBytes - m_curPos;
        memmove(m_tmpBuffer, m_curPos, m_tmpBufferLen);
    }

    return 0;
}

void TSDemuxer::openFile(const std::string& streamName)
{
    m_streamName = streamName;
    m_streamNameLow = strToLowerCase(unquoteStr(streamName));
    readClose();
    m_lastPesPts.clear();
    m_lastPesDts.clear();
    m_fullPtsTime.clear();
    m_fullDtsTime.clear();
    m_firstPtsTime.clear();
    m_firstDtsTime.clear();
    m_firstPCRTime = -1;
    m_lastPCRVal = -1;

    m_m2tsMode = isM2TSExt(streamName);

    if (!m_bufferedReader->openStream(m_readerID, m_streamName.c_str()))
        THROW(ERR_FILE_NOT_FOUND, "Can't open stream " << m_streamName);

    m_pmtPid = -1;
    m_codecReady = false;
    m_readCnt = 0;
    m_dataProcessed = 0;
    m_notificated = false;
}

void TSDemuxer::readClose() {}

TSDemuxer::~TSDemuxer() { m_bufferedReader->deleteReader(m_readerID); }

uint64_t TSDemuxer::getDemuxedSize() { return m_dataProcessed; }

void TSDemuxer::setFileIterator(FileNameIterator* itr)
{
    const auto br = dynamic_cast<BufferedFileReader*>(m_bufferedReader);
    if (br)
        br->setFileIterator(itr, m_readerID);
    else if (itr != nullptr)
        THROW(ERR_COMMON, "Can not set file iterator. Reader does not support bufferedReader interface.");
}

int64_t getLastPCR(File& file, const int bufferSize, const int frameSize, const int64_t fileSize)
{
    // pcr from end of file
    const auto tmpBuffer = new uint8_t[bufferSize];
    file.seek(FFMAX(fileSize - fileSize % frameSize - bufferSize, 0), File::SeekMethod::smBegin);
    const int len = file.read(tmpBuffer, bufferSize);
    if (len < 1)
        return -2;  // read error
    uint8_t* curPtr = tmpBuffer;
    const uint8_t* bufferEnd = tmpBuffer + len;
    int64_t lastPcrVal = -1;
    while (curPtr <= bufferEnd - frameSize)
    {
        const auto tsPacket = reinterpret_cast<TSPacket*>(curPtr + frameSize - 188);
        if (tsPacket->afExists && tsPacket->adaptiveField.length && tsPacket->adaptiveField.pcrExist)
        {
            lastPcrVal = tsPacket->adaptiveField.getPCR33();
        }
        curPtr += frameSize;
    }
    delete[] tmpBuffer;
    return lastPcrVal;
}

int64_t getTSDuration(const char* fileName)
{
    try
    {
        int frameSize = 188;
        if (isM2TSExt(fileName))
            frameSize = 192;
        uint64_t fileSize;
        File file(fileName, File::ofRead);
        if (!file.size(&fileSize))
            return -1;
        int bufferSize = 1024 * 256;
        bufferSize -= bufferSize % frameSize;
        const auto tmpBuffer = new uint8_t[bufferSize];
        // pcr from start of file
        const int len = file.read(tmpBuffer, bufferSize);
        if (len < 1)
        {
            delete[] tmpBuffer;
            return 0;
        }
        uint8_t* curPtr = tmpBuffer;
        const uint8_t* bufferEnd = tmpBuffer + len;
        int64_t firstPcrVal = 0;
        while (curPtr <= bufferEnd - frameSize)
        {
            const auto tsPacket = reinterpret_cast<TSPacket*>(curPtr + frameSize - 188);
            if (tsPacket->afExists && tsPacket->adaptiveField.length && tsPacket->adaptiveField.pcrExist)
            {
                firstPcrVal = tsPacket->adaptiveField.getPCR33();
                break;
            }
            curPtr += frameSize;
        }
        delete[] tmpBuffer;

        int64_t lastPcrVal{};
        bufferSize = 1024 * 256;
        bufferSize -= bufferSize % frameSize;
        do
        {
            lastPcrVal = getLastPCR(file, bufferSize, frameSize, fileSize);
            bufferSize *= 4;
        } while (lastPcrVal == -1 && bufferSize <= 1024 * 1024);

        file.close();
        if (lastPcrVal < 0)
            return 0;
        return lastPcrVal != -1 ? lastPcrVal - firstPcrVal : 0;
    }
    catch (...)
    {
        return 0;
    }
}

int64_t TSDemuxer::getFileDurationNano() const { return getTSDuration(m_streamName.c_str()) * 1000000000ll / 90000ll; }
