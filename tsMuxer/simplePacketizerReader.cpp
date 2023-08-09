#include "simplePacketizerReader.h"

#include <fs/systemlog.h>

#include <iostream>

#include "avCodecs.h"
#include "vodCoreException.h"
#include "vod_common.h"

SimplePacketizerReader::SimplePacketizerReader()
{
    m_needSync = true;
    m_tmpBufferLen = 0;
    m_curPts = PTS_CONST_OFFSET;
    m_frameNum = 0;
    m_processedBytes = 0;
    setTestMode(false);
    m_containerDataType = 0;
    m_containerStreamIndex = 0;
    m_stretch = 1.0;
    m_curMplsIndex = -1;
    m_lastMplsTime = 0;
    m_mplsOffset = 0;
    m_halfFrameLen = 0;
}

static constexpr double mplsEps = INTERNAL_PTS_FREQ / 45000.0 / 2.0;

void SimplePacketizerReader::doMplsCorrection()
{
    if (m_curMplsIndex == -1)
        return;
    if (m_curPts >= (m_lastMplsTime - mplsEps) && m_curMplsIndex < m_mplsInfo.size() - 1)
    {
        m_curMplsIndex++;
        if (m_mplsInfo[m_curMplsIndex].connection_condition == 5)
        {
            m_mplsOffset += m_curPts - m_lastMplsTime;
            // m_curPts = m_lastMplsTime; // fix PTS up
        }
        m_lastMplsTime += static_cast<int64_t>(
            (m_mplsInfo[m_curMplsIndex].OUT_time - m_mplsInfo[m_curMplsIndex].IN_time) * (INTERNAL_PTS_FREQ / 45000.0));
    }
}

void SimplePacketizerReader::setBuffer(uint8_t* data, const int dataLen, bool lastBlock)
{
    if (static_cast<size_t>(m_tmpBufferLen + dataLen) > m_tmpBuffer.size())
        m_tmpBuffer.resize(m_tmpBufferLen + dataLen);

    if (!m_tmpBuffer.empty())
        memcpy(m_tmpBuffer.data() + m_tmpBufferLen, data + MAX_AV_PACKET_SIZE, dataLen);
    m_tmpBufferLen += dataLen;

    if (!m_tmpBuffer.empty())
        m_curPos = m_buffer = m_tmpBuffer.data();
    else
        m_curPos = m_buffer = nullptr;
    m_bufEnd = m_buffer + m_tmpBufferLen;
    m_tmpBufferLen = 0;
}

uint64_t SimplePacketizerReader::getProcessedSize() { return m_processedBytes; }

int SimplePacketizerReader::flushPacket(AVPacket& avPacket)
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
        const int size =
            decodeFrame(m_tmpBuffer.data(), m_tmpBuffer.data() + m_tmpBufferLen, skipBytes, skipBeforeBytes);
        if (size + skipBytes + skipBeforeBytes <= 0 && size != NOT_ENOUGH_BUFFER)
            return 0;
    }
    avPacket.dts = avPacket.pts = static_cast<int64_t>(m_curPts * m_stretch) + m_timeOffset;
    if (m_tmpBufferLen > 0)
    {
        avPacket.data = m_tmpBuffer.data();
        avPacket.data += skipBeforeBytes;
        avPacket.size = static_cast<int>(m_tmpBufferLen);
        if (isPriorityData(&avPacket))
            avPacket.flags |= AVPacket::PRIORITY_DATA;
        if (isIFrame(&avPacket))
            avPacket.flags |= AVPacket::IS_IFRAME;  // can be used in split points
    }
    LTRACE(LT_DEBUG, 0, "Processed " << m_frameNum << " " << getCodecInfo().displayName << " frames");
    m_processedBytes += avPacket.size + skipBytes + skipBeforeBytes;
    return static_cast<int>(m_tmpBufferLen);
}

int SimplePacketizerReader::readPacket(AVPacket& avPacket)
{
    do
    {
        avPacket.flags = m_flags + AVPacket::IS_COMPLETE_FRAME | AVPacket::FORCE_NEW_FRAME;
        avPacket.stream_index = m_streamIndex;
        avPacket.codecID = getCodecInfo().codecID;
        avPacket.codec = this;
        avPacket.data = nullptr;
        avPacket.size = 0;
        avPacket.duration = 0;
        avPacket.dts = avPacket.pts = static_cast<int64_t>(m_curPts * m_stretch) + m_timeOffset;
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
            int decodeRez = decodeFrame(frame, m_bufEnd, skipBytes, skipBeforeBytes);
            if (decodeRez == NOT_ENOUGH_BUFFER)
            {
                if (m_bufEnd - frame > DEFAULT_FILE_BLOCK_SIZE)
                    THROW(ERR_COMMON,
                          getCodecInfo().displayName << " stream (track " << m_streamIndex << "): invalid stream.");
                memmove(m_tmpBuffer.data(), m_curPos, m_bufEnd - m_curPos);
                m_tmpBufferLen = m_bufEnd - m_curPos;
                m_curPos = m_bufEnd;
                return NEED_MORE_DATA;
            }
            if (decodeRez + skipBytes + skipBeforeBytes <= 0)
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
        avPacket.dts = avPacket.pts = static_cast<int64_t>(m_curPts * m_stretch) + m_timeOffset;
        if (m_bufEnd - m_curPos < getHeaderLen())
        {
            memmove(m_tmpBuffer.data(), m_curPos, m_bufEnd - m_curPos);
            m_tmpBufferLen = m_bufEnd - m_curPos;
            m_curPos = m_bufEnd;
            return NEED_MORE_DATA;
        }
        skipBytes = 0;
        skipBeforeBytes = 0;
        int frameLen = decodeFrame(m_curPos, m_bufEnd, skipBytes, skipBeforeBytes);
        if (frameLen == NOT_ENOUGH_BUFFER)
        {
            if (m_bufEnd - m_curPos > DEFAULT_FILE_BLOCK_SIZE)
                THROW(ERR_COMMON,
                      getCodecInfo().displayName << " stream (track " << m_streamIndex << "): invalid stream.");
            memmove(m_tmpBuffer.data(), m_curPos, m_bufEnd - m_curPos);
            m_tmpBufferLen = m_bufEnd - m_curPos;
            m_curPos = m_bufEnd;
            return NEED_MORE_DATA;
        }
        if (frameLen + skipBytes + skipBeforeBytes <= 0)
        {
            LTRACE(LT_INFO, 2,
                   getCodecInfo().displayName
                       << " stream (track " << m_streamIndex << "): bad frame detected at position"
                       << floatToTime((avPacket.pts - PTS_CONST_OFFSET) / (double)INTERNAL_PTS_FREQ, ',')
                       << ". Resync stream.");
            m_needSync = true;
            return 0;
        }
        if (m_bufEnd - m_curPos < frameLen + skipBytes + skipBeforeBytes)
        {
            memmove(m_tmpBuffer.data(), m_curPos, m_bufEnd - m_curPos);
            m_tmpBufferLen = m_bufEnd - m_curPos;
            m_curPos = m_bufEnd;
            return NEED_MORE_DATA;
        }
        avPacket.data = m_curPos;
        avPacket.data += skipBeforeBytes;
        if (frameLen > MAX_AV_PACKET_SIZE)
            THROW(ERR_AV_FRAME_TOO_LARGE, "AV frame too large (" << frameLen << " bytes). Increase AV buffer.");
        avPacket.size = frameLen;
        if (isPriorityData(&avPacket))
            avPacket.flags |= AVPacket::PRIORITY_DATA;
        if (isIFrame(&avPacket))
            avPacket.flags |= AVPacket::IS_IFRAME;  // can be used in split points

        if (m_halfFrameLen == 0.0)
            m_halfFrameLen = getFrameDuration() / 2.0;
        m_curPts += getFrameDuration();
        int64_t nextDts = static_cast<int64_t>(m_curPts * m_stretch) + m_timeOffset;
        avPacket.duration = nextDts - avPacket.dts;
        // doMplsCorrection();
        m_frameNum++;
        if (m_frameNum % 1000 == 0)
            LTRACE(LT_DEBUG, 0, "Processed " << m_frameNum << " " << getCodecInfo().displayName << " frames");
        m_curPos += frameLen + skipBytes + skipBeforeBytes;
        m_processedBytes += frameLen + skipBytes + skipBeforeBytes;

        if (needMPLSCorrection())
        {
            if (/*m_demuxMode && */ m_mplsOffset > m_halfFrameLen)
            {  // overlap frame detected. skip frame
                if (avPacket.duration)
                    LTRACE(LT_INFO, 2,
                           getCodecInfo().displayName
                               << " stream (track " << m_streamIndex << "): overlapped frame detected at position "
                               << floatToTime((avPacket.pts - PTS_CONST_OFFSET) / (double)INTERNAL_PTS_FREQ, ',')
                               << ". Remove frame.");
                m_mplsOffset -= getFrameDuration();
                m_curPts -= getFrameDuration();
                return readPacket(avPacket);  // ignore overlapped packet, get next one
            }
            doMplsCorrection();
        }

        if (needSkipFrame(avPacket))
            continue;

        return 0;

    } while (true);
}

static constexpr int CHECK_FRAMES_COUNT = 10;

CheckStreamRez SimplePacketizerReader::checkStream(uint8_t* buffer, const int len, const ContainerType containerType,
                                                   const int containerDataType, const int containerStreamIndex)
{
    m_containerType = containerType;
    m_containerDataType = containerDataType;
    m_containerStreamIndex = containerStreamIndex;
    setTestMode(true);

    CheckStreamRez rez;
    uint8_t* end = buffer + len;
    uint8_t* frame = findFrame(buffer, end);
    if (frame == nullptr)
    {
        setTestMode(false);
        return rez;
    }
    int skipBytes = 0;
    int skipBeforeBytes = 0;
    for (int i = 0; i < 5; ++i)
    {
        if (decodeFrame(frame, end, skipBytes, skipBeforeBytes) <= 0)
        {
            // setTestMode(false);
            // return rez;
            frame = findFrame(frame + 2, end);

            if (frame == nullptr)
            {
                setTestMode(false);
                return rez;
            }
        }
        else
            break;
    }
    const int freq = getFreq();
    bool firstStep = true;
    for (int i = 0; i < CHECK_FRAMES_COUNT && frame < end;)
    {
        const int frameLen = decodeFrame(frame, end, skipBytes, skipBeforeBytes);
        if (frameLen <= 0 || getFreq() != freq || (firstStep && frameLen > end - frame))
        {
            setTestMode(false);
            return rez;
        }
        firstStep = false;
        frame += frameLen + skipBytes + skipBeforeBytes;
        if (getFrameDuration() > 0)
            i++;
    }
    setTestMode(false);
    rez.codecInfo = getCodecInfo();
    rez.streamDescr = getStreamInfo();
    return rez;
}
