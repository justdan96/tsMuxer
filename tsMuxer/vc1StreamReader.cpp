
#include "vc1StreamReader.h"

#include <fs/systemlog.h>

#include <iostream>

#include "avCodecs.h"
#include "nalUnits.h"
#include "pesPacket.h"
#include "tsPacket.h"
#include "vodCoreException.h"
#include "vod_common.h"

void VC1StreamReader::writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket)
{
    // 01 81 55

    pesPacket->flagsLo |= 1;  // enable PES extension for VC-1 stream
    uint8_t* data = (uint8_t*)(pesPacket) + pesPacket->getHeaderLength();
    *data++ = 0x01;
    *data++ = 0x81;
    *data++ = 0x55;  // VC-1 sub type id 0x55-0x5f
    pesPacket->m_pesHeaderLen += 3;
}

int VC1StreamReader::writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                                       PriorityDataInfo* priorityData)
{
    // uint8_t* afterPesData = (uint8_t*)pesPacket + pesPacket->getHeaderLength();
    uint8_t* curPtr = dstBuffer;  // afterPesData;
    if ((m_totalFrameNum > 1 && m_spsFound < 2 && m_frame.pict_type == VC1PictType::I_TYPE) ||
        (m_totalFrameNum > 1 && m_firstFileFrame && !m_decodedAfterSeq))
    {
        m_firstFileFrame = false;
        if (m_seqBuffer.size() > 0)
        {
            if ((size_t)(dstEnd - curPtr) < m_seqBuffer.size())
                THROW(ERR_COMMON, "VC1 stream error: Not enough buffer for write headers");
            memcpy(curPtr, &m_seqBuffer[0], m_seqBuffer.size());
            curPtr += m_seqBuffer.size();
        }
        if (m_entryPointBuffer.size() > 0)
        {
            if ((size_t)(dstEnd - curPtr) < m_entryPointBuffer.size())
                THROW(ERR_COMMON, "VC1 stream error: Not enough buffer for write headers");
            memcpy(curPtr, &m_entryPointBuffer[0], m_entryPointBuffer.size());
            curPtr += m_entryPointBuffer.size();
        }
    }
    return (int)(curPtr - dstBuffer);  // afterPesData;
}

int VC1StreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    for (uint8_t* nal = VC1Unit::findNextMarker(m_buffer, m_bufEnd); nal <= m_bufEnd - 32;
         nal = VC1Unit::findNextMarker(nal + 4, m_bufEnd))
    {
        auto unitType = (VC1Code)nal[3];

        if (unitType == VC1Code::SEQHDR)
        {
            uint8_t* nextNal = VC1Unit::findNextMarker(nal + 4, m_bufEnd);
            VC1SequenceHeader sequence;
            sequence.vc1_unescape_buffer(nal + 4, nextNal - nal - 4);
            if (sequence.decode_sequence_header() != 0)
                return 0;

            dstBuff[0] = (int)TSDescriptorTag::REGISTRATION;  // descriptor tag
            dstBuff[1] = 0x06;                                // descriptor len
            dstBuff[2] = 0x56;                                // "V"
            dstBuff[3] = 0x43;                                // "C"
            dstBuff[4] = 0x2D;                                // "-"
            dstBuff[5] = 0x31;                                // "1"
            dstBuff[6] = 0x01;                                // profile and level subdescriptor

            int profile = (int)sequence.profile << 4;
            switch (sequence.profile)
            {
            case Profile::SIMPLE:
                dstBuff[7] = profile + 0x11 + (sequence.level >> 1);
                break;
            case Profile::MAIN:
                dstBuff[7] = profile + 0x41 + (sequence.level >> 1);
                break;
            case Profile::ADVANCED:
                dstBuff[7] = profile + 0x61 + sequence.level;
                break;
            default:
                dstBuff[1] = 0x04;  // remove profile and level descriptor
                return 6;           // total descriptor length
            }
            return 8;
        }
    }
    return 0;
}

bool VC1StreamReader::skipNal(uint8_t* nal) { return !m_eof && nal[0] == (uint8_t)VC1Code::ENDOFSEQ; }

CheckStreamRez VC1StreamReader::checkStream(uint8_t* buffer, int len)
{
    CheckStreamRez rez;
    uint8_t* end = buffer + len;
    uint8_t* nextNal = 0;
    bool spsFound = false;
    bool iFrameFound = false;
    bool pulldown = false;
    for (uint8_t* nal = VC1Unit::findNextMarker(buffer, end); nal <= end - 32;
         nal = VC1Unit::findNextMarker(nal + 4, end))
    {
        auto unitType = (VC1Code)nal[3];
        switch (unitType)
        {
        case VC1Code::ENDOFSEQ:
            break;
        case VC1Code::SLICE:
        case VC1Code::USER_SLICE:
            break;
        case VC1Code::FIELD:
        case VC1Code::USER_FIELD:
            break;
        case VC1Code::FRAME:
        case VC1Code::USER_FRAME:
            nextNal = VC1Unit::findNextMarker(nal + 4, end);
            if (m_frame.decode_frame_direct(m_sequence, nal + 4, nextNal) != 0)
                break;
            if (m_sequence.pulldown)
            {
                if (!m_sequence.interlace || m_sequence.psf)
                {
                    pulldown |= m_frame.rptfrm > 0;
                }
                else
                {
                    pulldown |= (m_frame.rff > 0) && (m_frame.fcm != 2);
                }
            }
            if (m_frame.pict_type == VC1PictType::I_TYPE)
                iFrameFound = true;
            break;
        case VC1Code::ENTRYPOINT:
        case VC1Code::USER_ENTRYPOINT:
            nextNal = VC1Unit::findNextMarker(nal + 4, end);
            m_sequence.vc1_unescape_buffer(nal + 4, nextNal - nal - 4);
            if (m_sequence.decode_entry_point() != 0)
                break;
            break;
        case VC1Code::SEQHDR:
        case VC1Code::USER_SEQHDR:
            nextNal = VC1Unit::findNextMarker(nal + 4, end);
            m_sequence.vc1_unescape_buffer(nal + 4, nextNal - nal - 4);
            if (m_sequence.decode_sequence_header() != 0)
                break;
            spsFound = true;
            break;
        default:
            return rez;
        }
    }
    if (spsFound && iFrameFound)
    {
        rez.codecInfo = vc1CodecInfo;
        rez.streamDescr = m_sequence.getStreamDescr();
        if (pulldown)
            rez.streamDescr += " (pulldown)";
    }
    return rez;
}

int VC1StreamReader::intDecodeNAL(uint8_t* buff)
{
    m_spsPpsFound = false;

    int rez = 0;
    uint8_t* nextNal = 0;
    switch ((VC1Code)*buff)
    {
    case VC1Code::ENTRYPOINT:
        return decodeEntryPoint(buff);
        break;
    case VC1Code::ENDOFSEQ:
        nextNal = VC1Unit::findNextMarker(buff, m_bufEnd) + 3;
        if (!m_eof && nextNal >= m_bufEnd)
            return NOT_ENOUGH_BUFFER;
        break;
    case VC1Code::SEQHDR:
        m_spsPpsFound = true;
        rez = decodeSeqHeader(buff);
        if (rez != 0)
            return rez;
        nextNal = VC1Unit::findNextMarker(buff, m_bufEnd) + 3;
        while (1)
        {
            if (nextNal >= m_bufEnd)
                return NOT_ENOUGH_BUFFER;
            switch ((VC1Code)*nextNal)
            {
            case VC1Code::ENTRYPOINT:
                rez = decodeEntryPoint(nextNal);
                if (rez != 0)
                    return rez;
                break;
            case VC1Code::FRAME:
            case VC1Code::USER_FRAME:
                rez = decodeFrame(nextNal);
                if (rez == 0)
                    m_lastDecodedPos = nextNal;
                m_decodedAfterSeq = true;
                return rez;
            default:
                break;
            }
            nextNal = VC1Unit::findNextMarker(nextNal, m_bufEnd) + 3;
        }
        break;
    case VC1Code::FRAME:
    case VC1Code::USER_FRAME:
        m_decodedAfterSeq = false;
        rez = decodeFrame(buff);
        return rez;
    default:
        break;
    }
    return 0;
}

int VC1StreamReader::decodeSeqHeader(uint8_t* buff)
{
    uint8_t* nextNal = VC1Unit::findNextMarker(buff, m_bufEnd);
    if (nextNal == m_bufEnd)
    {
        return NOT_ENOUGH_BUFFER;
    }
    int64_t oldSpsLen = nextNal - buff - 1;
    m_sequence.vc1_unescape_buffer(buff + 1, oldSpsLen);
    int rez = m_sequence.decode_sequence_header();
    if (rez != 0)
        return rez;

    fillAspectBySAR(m_sequence.sample_aspect_ratio.num / (double)m_sequence.sample_aspect_ratio.den);

    updateFPS(0, buff, nextNal, (int)oldSpsLen);
    if (m_spsFound == 0)
    {
        LTRACE(LT_INFO, 2, "Decoding VC-1 stream (track " << m_streamIndex << "): " << m_sequence.getStreamDescr());
    }
    if (m_sequence.profile != Profile::ADVANCED)
        THROW(ERR_VC1_ERR_PROFILE,
              "Only ADVANCED profile are supported now. For feature request contat to: r_vasilenko@smlabs.net.");

    m_spsFound++;
    nextNal = VC1Unit::findNextMarker(buff, m_bufEnd);
    m_seqBuffer.clear();
    m_seqBuffer.push_back(0);
    m_seqBuffer.push_back(0);
    m_seqBuffer.push_back(1);
    for (uint8_t* cur = buff; cur < nextNal; cur++) m_seqBuffer.push_back(*cur);
    return 0;
}

int VC1StreamReader::decodeFrame(uint8_t* buff)
{
    if (!m_spsFound)
        return NALUnit::SPS_OR_PPS_NOT_READY;
    int rez = m_frame.decode_frame_direct(m_sequence, buff + 1, m_bufEnd);
    if (rez != 0)
        return rez;

    int nextBFrameCnt = 0;
    int64_t bTiming = 0;
    if (m_sequence.max_b_frames > 0 &&
        (m_frame.pict_type == VC1PictType::I_TYPE || m_frame.pict_type == VC1PictType::P_TYPE))
    {
        nextBFrameCnt = getNextBFrames(buff, bTiming);
        if (nextBFrameCnt == -1)
            return NOT_ENOUGH_BUFFER;
    }

    m_lastIFrame = m_frame.pict_type == VC1PictType::I_TYPE;
    m_totalFrameNum++;

    m_curDts += m_prevDtsInc;

    int64_t pcrIncVal = m_pcrIncPerFrame;
    // if (m_frame.fcm == 2) // coded field
    //	pcrIncVal = m_pcrIncPerField;

    if (m_sequence.pulldown)
    {
        if (!m_sequence.interlace || m_sequence.psf)
        {
            m_prevDtsInc = pcrIncVal * ((int64_t)m_frame.rptfrm + 1);
        }
        else
        {
            m_prevDtsInc = pcrIncVal + (m_frame.rff ? m_pcrIncPerField : 0);
        }
    }
    else
        m_prevDtsInc = pcrIncVal;

    if (m_removePulldown)
    {
        checkPulldownSync();
        m_testPulldownDts += m_prevDtsInc;

        pcrIncVal = m_prevDtsInc = m_pcrIncPerFrame;
        if (m_sequence.pulldown)
        {
            if (!m_sequence.interlace || m_sequence.psf)
            {
                if (m_frame.rptfrm > 0)
                    updateBits(m_frame.getBitReader(), m_frame.rptfrmBitPos, 2, 0);
                m_frame.rptfrm = 0;
            }
            else
            {
                if (m_frame.rff > 0)
                    updateBits(m_frame.getBitReader(), m_frame.rptfrmBitPos + 1, 1, 0);
                m_frame.rff = 0;
            }
        }
        /*
        if (m_frame.fcm == 2) {
                m_frame.fcm = 1;
                buff[1] &= 0xbf; // 1011 1111 set fcm to 1
        }
        */
    }

    if (m_frame.pict_type == VC1PictType::I_TYPE || m_frame.pict_type == VC1PictType::P_TYPE)
    {
        if (m_removePulldown)
            m_curPts = m_curDts + (nextBFrameCnt)*m_pcrIncPerFrame;
        else
            m_curPts = m_curDts + bTiming;
    }
    else
        m_curPts = m_curDts - m_pcrIncPerFrame;  // pcrInc;
    return 0;
}

int VC1StreamReader::decodeEntryPoint(uint8_t* buff)
{
    uint8_t* nextNal = VC1Unit::findNextMarker(buff, m_bufEnd);
    if (nextNal == m_bufEnd)
        return NOT_ENOUGH_BUFFER;
    m_entryPointBuffer.clear();
    m_entryPointBuffer.push_back(0);
    m_entryPointBuffer.push_back(0);
    m_entryPointBuffer.push_back(1);
    for (uint8_t* cur = buff; cur < nextNal; cur++) m_entryPointBuffer.push_back(*cur);
    return 0;
}

int VC1StreamReader::getNextBFrames(uint8_t* buffer, int64_t& bTiming)
{
    int bFrameCnt = 0;
    bTiming = 0;
    for (uint8_t* nal = VC1Unit::findNextMarker(buffer, m_bufEnd); nal < m_bufEnd - 4;
         nal = VC1Unit::findNextMarker(nal + 4, m_bufEnd))
    {
        auto vc1Code = (VC1Code)nal[3];
        if (vc1Code == VC1Code::FRAME || vc1Code == VC1Code::USER_FRAME)
        {
            VC1Frame frame;
            if (frame.decode_frame_direct(m_sequence, nal + 4, m_bufEnd) != 0)
                break;
            if (frame.pict_type == VC1PictType::I_TYPE || frame.pict_type == VC1PictType::P_TYPE)
                return bFrameCnt;
            bFrameCnt++;

            int64_t pcrIncVal = m_pcrIncPerFrame;

            if (m_sequence.pulldown)
            {
                if (!m_sequence.interlace || m_sequence.psf)
                {
                    pcrIncVal = pcrIncVal * ((int64_t)frame.rptfrm + 1);
                }
                else
                {
                    pcrIncVal += (frame.rff ? m_pcrIncPerField : 0);
                }
            }
            bTiming += pcrIncVal;

            if (bFrameCnt == m_sequence.max_b_frames)
                return bFrameCnt;
        }
    }
    if (m_eof)
    {
        return bFrameCnt;
    }
    else
    {
        return -1;
    }
}

uint8_t* VC1StreamReader::findNextFrame(uint8_t* buffer)
{
    for (uint8_t* nal = VC1Unit::findNextMarker(buffer, m_bufEnd); nal < m_bufEnd - 4;
         nal = VC1Unit::findNextMarker(nal + 4, m_bufEnd))
    {
        auto vc1Code = (VC1Code)nal[3];
        if (vc1Code != VC1Code::FIELD && vc1Code != VC1Code::USER_FIELD)
            return nal;
    }
    if (m_eof)
        return m_bufEnd;
    else
        return 0;
}

void VC1StreamReader::updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen)
{
    m_sequence.setFPS(m_fps);
    uint8_t* tmpBuffer = new uint8_t[oldSpsLen + 16];
    int64_t newSpsLen = m_sequence.vc1_escape_buffer(tmpBuffer);
    if (newSpsLen != oldSpsLen)
    {
        int64_t sizeDiff = newSpsLen - oldSpsLen;
        memmove(nextNal + sizeDiff, nextNal, m_bufEnd - nextNal);
        m_bufEnd += sizeDiff;
    }
    memcpy(buff + 1, tmpBuffer, newSpsLen);
    delete[] tmpBuffer;
}
