
#include "mpegStreamReader.h"

#include <fs/systemlog.h>

#include <iostream>

#include <cmath>
#include "nalUnits.h"
#include "vodCoreException.h"
#include "vod_common.h"

static constexpr double EPSILON = 5e-5;
static constexpr int64_t MAX_PULLDOWN_ASYNC = 100000000ll;
static constexpr int UNIT_SKIPPED = 5;

using namespace std;

void MPEGStreamReader::setBuffer(uint8_t* data, const int dataLen, const bool lastBlock)
{
    if (lastBlock)
        m_eof = true;

    if (m_tmpBufferLen + dataLen > TMP_BUFFER_SIZE)
        THROW(ERR_COMMON_SMALL_BUFFER,
              "Not enough buffer for parse video stream. Current frame num " << m_totalFrameNum);
    memcpy(m_tmpBuffer + m_tmpBufferLen, data + MAX_AV_PACKET_SIZE, dataLen);
    m_tmpBufferLen += dataLen;

    m_curPos = m_buffer = m_tmpBuffer;
    m_bufEnd = m_buffer + m_tmpBufferLen;
    m_tmpBufferLen = 0;
}

int MPEGStreamReader::flushPacket(AVPacket& avPacket)
{
    m_eof = true;
    avPacket.codec = this;
    avPacket.duration = 0;
    avPacket.data = nullptr;
    avPacket.size = 0;
    avPacket.flags = m_flags;
    avPacket.stream_index = m_streamIndex;
    avPacket.codecID = getCodecInfo().codecID;

    if (m_tmpBufferLen > 0)
    {
        const uint8_t* prevPos = m_curPos;
        m_curPos = m_tmpBuffer;
        m_bufEnd = m_tmpBuffer + m_tmpBufferLen;
        const int isNal = bufFromNAL();
        int decodeRez = 0;
        if (isNal)
        {
            m_shortStartCodes = isNal < 4;
            if ((prevPos + isNal) > m_lastDecodedPos)
            {
                m_lastDecodedPos = nullptr;
                decodeRez = decodeNal(m_curPos + isNal);
            }
        }
        if (decodeRez == 0)
        {
            avPacket.data = m_tmpBuffer;
            avPacket.size = static_cast<int>(m_tmpBufferLen);
        }
    }

    avPacket.pts = m_curPts + m_timeOffset;
    avPacket.dts = m_curDts + m_timeOffset - m_pcrIncPerFrame * getFrameDepth();  // shift dts back

    string message = "Processed ";
    message += int32ToStr(m_totalFrameNum);
    message += " video frames";
    LTRACE(LT_DEBUG, 0, message);
    LTRACE(LT_INFO, 2, message);
    m_processedBytes += avPacket.size;
    return static_cast<int>(m_tmpBufferLen);
}

void MPEGStreamReader::onShiftBuffer(int offset) {}

void MPEGStreamReader::storeBufferRest()
{
    onShiftBuffer(static_cast<int>(m_curPos - m_tmpBuffer));
    memmove(m_tmpBuffer, m_curPos, m_bufEnd - m_curPos);
    m_tmpBufferLen = static_cast<int>(m_bufEnd - m_curPos);
    if (m_lastDecodedPos > m_curPos)
        m_lastDecodedPos = m_tmpBuffer + (m_lastDecodedPos - m_curPos);
    else
        m_lastDecodedPos = nullptr;
    m_curPos = m_bufEnd;
}

int MPEGStreamReader::readPacket(AVPacket& avPacket)
{
    // LTRACE(LT_DEBUG, 0, "MPEGStreamReader::readPacket");
    avPacket.codec = this;
    avPacket.flags = m_flags;
    avPacket.stream_index = m_streamIndex;
    avPacket.codecID = getCodecInfo().codecID;

    avPacket.duration = 0;
    avPacket.data = nullptr;
    avPacket.size = 0;
    avPacket.pts = m_curPts + m_timeOffset;
    avPacket.dts = m_curDts + m_timeOffset - m_pcrIncPerFrame * getFrameDepth();  // shift dts back
    const uint8_t* prevPos = m_curPos;
    if (!m_syncToStream)
    {
        uint8_t* nal = NALUnit::findNALWithStartCode(m_curPos, m_bufEnd, m_longCodesAllowed);
        if (nal != m_bufEnd)
        {
            m_syncToStream = true;
            m_curPos = nal;
        }
        else
            m_curPos = m_bufEnd;
        const int bytesProcessed = static_cast<int>(m_curPos - prevPos);
        m_processedBytes += bytesProcessed;
        prevPos = m_curPos;
        if (!m_syncToStream)
            return NEED_MORE_DATA;
    }

    const uint8_t* nextNal =
        NALUnit::findNALWithStartCode((std::min)(m_curPos + 3, m_bufEnd), m_bufEnd, m_longCodesAllowed);
    if (nextNal == m_bufEnd)
    {
        storeBufferRest();
        return NEED_MORE_DATA;
    }

    int isNal = bufFromNAL();
    if (isNal)
    {
        const int64_t prevDts = m_curDts;
        m_shortStartCodes = isNal < 4;
        int rez = 0;
        while (true)
        {
            rez = decodeNal(m_curPos + isNal);
            if (rez != UNIT_SKIPPED)
                break;
            uint8_t* nal = NALUnit::findNALWithStartCode(m_curPos + isNal, m_bufEnd, m_longCodesAllowed);
            // assert(nal < findEnd); // if unit is skipped, next unit MUST be in buffer

            m_processedBytes += nal - m_curPos;
            prevPos = m_curPos = nal;
            if (nal == m_bufEnd)
            {
                rez = NOT_ENOUGH_BUFFER;
                m_syncToStream = false;
                break;
            }
            isNal = bufFromNAL();
        }

        if (rez == NOT_ENOUGH_BUFFER)
        {
            storeBufferRest();
            m_lastDecodedPos = nullptr;
            return NEED_MORE_DATA;
        }
        if (prevDts != m_curDts)
        {
            // stream interleaving is allowed during one stream packet

            avPacket.pts = m_curPts + m_timeOffset;
            avPacket.dts = m_curDts + m_timeOffset - m_pcrIncPerFrame * getFrameDepth();  // shift dts back to 1 frame;
            if (isIFrame())
                avPacket.flags |= AVPacket::IS_IFRAME;
            if (isPriorityData(&avPacket))
                avPacket.flags |= AVPacket::PRIORITY_DATA;
            if (m_spsPpsFound)
                avPacket.flags |= AVPacket::IS_SPS_PPS_IN_GOP;

            return 0;  // return zero AV packet for new frame
        }
    }
    uint8_t* findEnd = (std::min)(m_bufEnd, m_curPos + MAX_AV_PACKET_SIZE);
    uint8_t* nal = NALUnit::findNALWithStartCode(m_curPos + isNal, findEnd, m_longCodesAllowed);

    if (nal == findEnd)
    {
        if (nal[0] == 1 && nal[-1] == 0 && nal[-2] == 0)
        {
            nal -= 2;
            if (nal[-1] == 0)
                nal--;
        }
        else if (nal[0] == 0 && nal[-1] == 0)
        {
            nal--;
            if (nal[-1] == 0)
                nal--;
        }
    }

    const int bytesProcessed = static_cast<int>(nal - prevPos);
    avPacket.data = m_curPos;
    avPacket.size = bytesProcessed;
    avPacket.pts = m_curPts + m_timeOffset;
    avPacket.dts = m_curDts + m_timeOffset - m_pcrIncPerFrame * getFrameDepth();  // shift dts back
    if (isIFrame())
        avPacket.flags |= AVPacket::IS_IFRAME;
    if (isPriorityData(&avPacket))
        avPacket.flags |= AVPacket::PRIORITY_DATA;
    if (m_spsPpsFound)
        avPacket.flags |= AVPacket::IS_SPS_PPS_IN_GOP;

    m_curPos = nal;

    m_tmpBufferLen = 0;
    m_processedBytes += avPacket.size;

    return 0;
}

int MPEGStreamReader::bufFromNAL() const
{
    if (m_bufEnd - m_curPos < (3 + (m_longCodesAllowed ? 1 : 0)))
        return 0;
    if (m_curPos[0] == 0 && m_curPos[1] == 0)
    {
        if (m_longCodesAllowed && m_curPos[2] == 0 && m_curPos[3] == 1)
            return 4;
        else if (m_curPos[2] == 1)
            return 3;
        else
            return 0;
    }
    else
        return 0;
}

/*
int MPEGStreamReader::bufFromNAL(const uint8_t* buff, const uint8_t* bufEnd, bool longCodesAllowed) {
        if (bufEnd - buff < 4)
                return false;
        if (buff[0] == 0 && buff[1] == 0) {
                if (longCodesAllowed && buff[2] == 0 && buff[3] == 1 )
                        return 4;
                else if (buff[2] == 1)
                        return 3;
                else
                        return 0;
        }
        else
                return 0;
}
*/

uint64_t MPEGStreamReader::getProcessedSize() { return m_processedBytes; }

int MPEGStreamReader::decodeNal(uint8_t* buff)
{
    int rez = 0;
    if (buff > m_lastDecodedPos)
    {
        rez = intDecodeNAL(buff);
        if (rez != 0)
        {
            // m_skippedNal.clear();
            return rez;
        }
        if (m_lastDecodedPos < buff)
            m_lastDecodedPos = buff;
    }

    // if (!m_skippedNal.empty() && buff == m_skippedNal[0])
    if (skipNal(buff))
    {
        // m_skippedNal.erase(m_skippedNal.begin());
        return UNIT_SKIPPED;
    }
    else
        return rez;
}

#define abs_(a, b) ((a) >= (b) ? (a) - (b) : (b) - (a))
void MPEGStreamReader::updateFPS(void* curNALUnit, uint8_t* buff, uint8_t* nextNal, const int oldSPSLen)
{
    double spsFps = getStreamFPS(curNALUnit);
    spsFps = correctFps(spsFps);
    if (spsFps == 0.0 && m_fps == 0.0)
    {
        setFPS(25.0);
        LTRACE(LT_INFO, 2,
               "This " << getCodecInfo().displayName
                       << " stream doesn't contain fps value. Muxing fps is absent too. Set muxing FPS to default 25.0 "
                          "value.");
    }
    else if (m_fps == 0.0)
    {
        setFPS(spsFps);
        LTRACE(LT_INFO, 2,
               getCodecInfo().displayName << " muxing fps is not set. Get fps from stream. Value: " << spsFps);
    }
    else if (spsFps != 0.0 && abs_(m_fps, spsFps) > EPSILON)
    {
        if (m_isFirstFpsWarn)
        {
            LTRACE(LT_INFO, 2,
                   getCodecInfo().displayName << " manual defined fps doesn't equal to stream fps. Change "
                                              << getCodecInfo().displayName << " fps from " << spsFps << " to "
                                              << m_fps);
            m_isFirstFpsWarn = false;
        }
        updateStreamFps(curNALUnit, buff, nextNal, oldSPSLen);
    }
    else if (spsFps == 0.0)
    {
        if (m_isFirstFpsWarn)
        {
            LTRACE(LT_INFO, 2, getCodecInfo().displayName << " stream doesn't contain fps field. Muxing fps=" << m_fps);
            m_isFirstFpsWarn = false;
        }
    }
    updateStreamAR(curNALUnit, buff, nextNal, oldSPSLen);
}

void MPEGStreamReader::checkPulldownSync()
{
    int64_t asyncValue = m_curDts * 5 - m_testPulldownDts * 4;
    if (asyncValue < 0)
        asyncValue = -asyncValue;

    if (m_testPulldownDts != 0 && asyncValue > 5 * MAX_PULLDOWN_ASYNC * m_pulldownWarnCnt)
    {
        LTRACE(LT_ERROR, 2,
               "Warning! Source stream contain irregular pulldown marks. Mistiming between original fps and "
               "fps/1.25 (without pulldown) exceeds "
                   << (int64_t)(asyncValue / 5000000ll) << "ms.");
        m_pulldownWarnCnt *= 2;
    }
}

void MPEGStreamReader::fillAspectBySAR(const double sar)
{
    if (m_streamAR == VideoAspectRatio::AR_KEEP_DEFAULT)
    {
        const double ar = getStreamWidth() * sar / static_cast<double>(getStreamHeight());
        static constexpr double base_ar[] = {0.0, 1.0, 4.0 / 3.0, 16.0 / 9.0, 221.0 / 100.0};
        double minEps = INT_MAX;
        m_streamAR = VideoAspectRatio::AR_KEEP_DEFAULT;
        for (int i = 0; i < sizeof(base_ar) / sizeof(double); ++i)
        {
            if (fabs(ar - base_ar[i]) < minEps)
            {
                minEps = fabs(ar - base_ar[i]);
                m_streamAR = static_cast<VideoAspectRatio>(i);
            }
        }
    }
}
