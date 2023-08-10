#include "h264StreamReader.h"

#include <fs/systemlog.h>

#include "avCodecs.h"
#include "tsPacket.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;

H264StreamReader::H264StreamReader()
{
    m_frameNum = 0;
    m_lastMessageLen = -1;
    m_forceLsbDiv = 0;
    prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
    m_iFramePtsOffset = 0;
    m_isFirstFrame = true;
    m_forcedLevel = 0;
    m_delimiterFound = false;
    m_pict_type = 0;
    m_nextFrameFound = false;
    m_nextFrameIdr = false;
    m_firstAUDWarn = true;
    m_firstSPSWarn = true;
    m_firstSEIWarn = true;
    m_lastSlicePPS = -1;
    m_lastSliceSPS = -1;
    m_lastSliceIDR = false;
    m_h264SPSCont = false;
    m_spsCounter = 0;
    m_insertSEIMethod = SeiMethod::SEI_DoNotInsert;
    m_needSeiCorrection = false;
    m_firstDecodeNal = true;
    // m_cpb_removal_delay_baseaddr = 0;
    // m_cpb_removal_delay_bitpos = 0;
    orig_hrd_parameters_present_flag = false;
    orig_vcl_parameters_present_flag = false;
    m_lastPictStruct = 0;

    // for fixing thundberg streams
    // m_idrSliceFound = false;
    m_bSliceFound = false;
    m_picOrderOffset = 0;
    m_frameDepth = 1;
    m_lastIFrame = false;
    // m_openGOP = true;
    m_idrSliceCnt = 0;
    m_firstFileFrame = false;
    m_lastPicStruct = -1;
    m_lastDtsInc = 0;

    m_mvcSubStream = false;
    m_mvcPrimaryStream = false;
    m_blurayMode = true;
    number_of_offset_sequences = -1;
    m_bdRomMetaDataMsgPtsPos = 0;
    m_priorityNalAddr = nullptr;
    m_OffsetMetadataPtsAddr = nullptr;
    m_startPts = 0;
    m_decodedSliceHeader.resize(8 + 8);  // 8 for slice data, 8 - extra size for increase decoded data
    m_removalDelay = 0;
    m_spsChangeWarned = false;
}

H264StreamReader::~H264StreamReader()
{
    for (const auto &[index, sps] : m_spsMap) delete sps;
    for (const auto &[index, pps] : m_ppsMap) delete pps;
}

CheckStreamRez H264StreamReader::checkStream(uint8_t *buffer, int len)
{
    SEIUnit lastSEI;
    SliceUnit slice;
    CheckStreamRez rez;
    uint8_t *end = buffer + len;
    std::string tmpDescr;
    bool pulldownInserted = false;
    bool offsetsInserted = false;

    for (uint8_t *nal = NALUnit::findNextNAL(buffer, end); nal < end - 4; nal = NALUnit::findNextNAL(nal, end))
    {
        if (*nal & 0x80)
            return rez;
        auto nalType = static_cast<NALUnit::NALType>(*nal & 0x1f);
        const uint8_t *nextNal = NALUnit::findNALWithStartCode(nal, end, true);
        if (!m_eof && nextNal == end)
            break;

        switch (nalType)
        {
        case NALUnit::NALType::nuSubSPS:
            m_mvcSubStream = true;
            [[fallthrough]];
        case NALUnit::NALType::nuSPS:
        {
            if (nalType == NALUnit::NALType::nuSPS)
                m_mvcPrimaryStream = true;
            auto sps = new SPSUnit();
            sps->decodeBuffer(nal, nextNal);
            if (sps->deserialize() != 0)
            {
                delete sps;
                return rez;
            }
            m_spsMap.insert(make_pair(sps->seq_parameter_set_id, sps));
            if (tmpDescr.empty())
                tmpDescr = sps->getStreamDescr();
            break;
        }
        case NALUnit::NALType::nuPPS:
        {
            auto pps = new PPSUnit();
            pps->decodeBuffer(nal, nextNal);
            if (pps->deserialize() != 0)
            {
                delete pps;
                return rez;
            }
            if (m_spsMap.find(pps->seq_parameter_set_id) == m_spsMap.end())
            {
                delete pps;
                break;
            }
            m_ppsMap.insert(make_pair(pps->pic_parameter_set_id, pps));
            break;
        }
        case NALUnit::NALType::nuSEI:
            if (!m_spsMap.empty())
            {
                SPSUnit *sps = m_spsMap.begin()->second;
                lastSEI.decodeBuffer(nal, nextNal);
                lastSEI.deserialize(*sps, sps->nalHrdParams.isPresent || sps->vclHrdParams.isPresent);
                if (lastSEI.pic_struct == 5 || lastSEI.pic_struct == 6 || lastSEI.pic_struct == 7 ||
                    lastSEI.pic_struct == 8)
                {
                    if (!pulldownInserted)
                    {
                        pulldownInserted = true;
                        tmpDescr = sps->getStreamDescr();
                        tmpDescr += " (pulldown)";
                    }
                }
                if (!offsetsInserted && lastSEI.number_of_offset_sequences >= 0)
                {
                    offsetsInserted = true;
                    tmpDescr += "  3d-pg-planes: " + int32ToStr(lastSEI.number_of_offset_sequences);
                }
            }
            break;

        case NALUnit::NALType::nuSliceIDR:
            // m_openGOP = false;
        case NALUnit::NALType::nuSliceNonIDR:
        case NALUnit::NALType::nuSliceA:
        case NALUnit::NALType::nuSliceB:
        case NALUnit::NALType::nuSliceC:
        case NALUnit::NALType::nuSliceExt:
            if (m_ppsMap.empty() || m_spsMap.empty())
                break;
            try
            {
                uint8_t tmpBuffer[512];
                int toDecode = static_cast<int>(FFMIN(sizeof(tmpBuffer) - 8, nextNal - nal));
                int decodedLen = SliceUnit::decodeNAL(nal, nal + toDecode, tmpBuffer, sizeof(tmpBuffer));
                int nalRez = slice.deserialize(tmpBuffer, tmpBuffer + decodedLen, m_spsMap, m_ppsMap);
                if (nalRez != 0)
                    return rez;

                if (m_mvcSubStream)
                    rez.codecInfo = h264DepCodecInfo;
                else
                    rez.codecInfo = h264CodecInfo;
                rez.streamDescr = tmpDescr;
            }
            catch (BitStreamException& e)
            {
                (void)e;
                return rez;
            }
            break;
        case NALUnit::NALType::STAP_A:
            // unknown blu-ray nal before subSps
            break;
        default:
            break;
        }
    }
    if (m_mvcSubStream && m_mvcPrimaryStream)
        rez.multiSubStream = true;

    return rez;
}

const CodecInfo &H264StreamReader::getCodecInfo()
{
    if (m_mvcSubStream)
        return h264DepCodecInfo;
    return h264CodecInfo;
}

uint8_t *H264StreamReader::writeNalPrefix(uint8_t *curPos) const
{
    if (!m_shortStartCodes)
        *curPos++ = 0;
    *curPos++ = 0;
    *curPos++ = 0;
    *curPos++ = 1;
    return curPos;
}

int H264StreamReader::writeSEIMessage(uint8_t *dstBuffer, uint8_t *dstEnd, SEIUnit &sei,
                                      const uint8_t payloadType) const
{
    uint8_t *curPos = dstBuffer;

    if (dstEnd - curPos < 4)
        THROW(ERR_COMMON, "H264 stream error: Not enough buffer for write headers");

    uint8_t tmpBuffer[256];
    BitStreamWriter writer{};
    curPos = writeNalPrefix(curPos);
    writer.setBuffer(tmpBuffer, tmpBuffer + sizeof(tmpBuffer));

    uint8_t *sizeField = nullptr;
    int beforeMessageLen = 0;
    if (m_mvcSubStream)
    {
        writer.putBits(8, static_cast<int>(NALUnit::NALType::nuSEI));
        writer.putBits(8, SEI_MSG_MVC_SCALABLE_NESTING);

        sizeField = writer.getBuffer() + writer.getBitsCount() / 8;
        writer.putBits(8, 0);  // skip size field
        beforeMessageLen = writer.getBitsCount();

        static constexpr uint8_t DEFAULT_MVC_SEI_HEADER[] = {192, 16};
        if (!m_lastSeiMvcHeader.empty())
        {
            for (const auto &i : m_lastSeiMvcHeader) writer.putBits(8, i);
        }
        else
        {
            writer.putBits(8, DEFAULT_MVC_SEI_HEADER[0]);
            writer.putBits(8, DEFAULT_MVC_SEI_HEADER[1]);
        }

        writer.putBits(8, payloadType);
    }
    else
    {
        writer.putBits(8, static_cast<int>(NALUnit::NALType::nuSEI));
        writer.putBits(8, payloadType);
    }

    const SPSUnit *sps = m_spsMap.find(m_lastSliceSPS)->second;
    if (sps)
    {
        if (payloadType == 0)
            sei.serialize_buffering_period_message(*sps, writer, false);

        else
            sei.serialize_pic_timing_message(*sps, writer, false);

        if (sizeField)
        {
            const int msgLen = writer.getBitsCount() - beforeMessageLen;
            *sizeField = static_cast<uint8_t>(msgLen / 8);
        }
        SEIUnit::write_rbsp_trailing_bits(writer);
        writer.flushBits();

        const int sRez = SEIUnit::encodeNAL(tmpBuffer, tmpBuffer + writer.getBitsCount() / 8, curPos, dstEnd - curPos);
        if (sRez == -1)
            THROW(ERR_COMMON, "H264 stream error: Not enough buffer for write headers");
        curPos += sRez;
    }

    return static_cast<int>(curPos - dstBuffer);
}

int H264StreamReader::writeAdditionData(uint8_t *dstBuffer, uint8_t *dstEnd, AVPacket &avPacket,
                                        PriorityDataInfo *priorityData)
{
    uint8_t *curPos = dstBuffer;
    if (avPacket.size > 4 && avPacket.size < dstEnd - dstBuffer)
    {
        auto nalType = static_cast<NALUnit::NALType>(avPacket.data[4] & 0x1f);
        if (nalType == NALUnit::NALType::nuDelimiter || nalType == NALUnit::NALType::nuDRD)
        {
            // place delimiter at first place
            m_delimiterFound = true;
            memcpy(curPos, avPacket.data, avPacket.size);
            curPos += avPacket.size;
            avPacket.size = 0;
            avPacket.data = nullptr;
        }
    }

    if (!m_delimiterFound && m_lastSliceSPS != -1)
    {
        if (m_firstAUDWarn)
        {
            m_firstAUDWarn = false;
            LTRACE(LT_INFO, 2, "H264 bitstream changed: insert nal unit delimiters");
        }
        if (dstEnd - curPos < 6)
            THROW(ERR_COMMON, "H264 stream error: Not enough buffer for write headers");
        curPos = writeNalPrefix(curPos);
        if (m_mvcSubStream && m_blurayMode)
        {
            *curPos++ = static_cast<uint8_t>(NALUnit::NALType::nuDRD);
        }
        else
        {
            *curPos++ = static_cast<uint8_t>(NALUnit::NALType::nuDelimiter);
            *curPos++ = static_cast<uint8_t>(m_pict_type << 5 | 0x10);  // primary_pic_type << 5 + rbsp bits
        }
    }

    bool needInsTimingSEI = m_needSeiCorrection && m_lastSliceSPS != -1;
    bool srcSpsPpsFound = avPacket.flags & AVPacket::IS_SPS_PPS_IN_GOP;
    bool spsDiscontinue =
        m_h264SPSCont &&
        ((m_spsCounter < 2 && ((m_totalFrameNum > 1 && m_lastSliceIDR) || (m_totalFrameNum > 250 && isIFrame()))) ||
         (m_firstFileFrame && !srcSpsPpsFound));
    bool needInsSpsPps = false;
    if (isIFrame() && m_lastSliceSPS != -1 && m_lastSlicePPS != -1)
    {
        needInsSpsPps = spsDiscontinue || (replaceToOwnSPS() && srcSpsPpsFound);
    }

    if (needInsSpsPps)
    {
        avPacket.flags |= AVPacket::IS_SPS_PPS_IN_GOP;

        // Why the check for 250 : unless we see the 2nd IDR frame, we can only see that the stream runs with open GOPs
        // (open GOPs with one IDR at start for the whole movie do happen). That's why there's also a check that there's
        // a SPS before for the first I-frame in the first GOP in a normal stream. There's nothing scary about that - we
        // simply insert one more SPS - but in order to avoid outputting messages into the console, I left the check at
        // 250.
        if (m_firstSPSWarn && m_totalFrameNum > 250)
        {
            m_firstSPSWarn = false;
            if (spsDiscontinue)
            {
                LTRACE(LT_INFO, 2, "H264 bitstream changed: insert SPS/PPS units");
            }
        }
        SPSUnit *sps = m_spsMap.find(m_lastSliceSPS)->second;
        if (dstEnd - curPos < 4)
            THROW(ERR_COMMON, "H264 stream error: Not enough buffer for write headers");
        curPos = writeNalPrefix(curPos);
        int sRez = sps->serializeBuffer(curPos, dstEnd, false);
        if (sRez == -1)
            THROW(ERR_COMMON, "H264 stream error: Not enough buffer for write headers");
        curPos += sRez;

        // PPSUnit* pps = m_ppsMap.find(m_lastSlicePPS)->second;
        // curPos = writeNalPrefix(curPos);
        // curPos += pps->serializeBuffer(curPos, false);
        for (auto [index, pps] : m_ppsMap)
        {
            if (dstEnd - curPos < 4)
                THROW(ERR_COMMON, "H264 stream error: Not enough buffer for write headers");
            curPos = writeNalPrefix(curPos);
            sRez = pps->serializeBuffer(curPos, dstEnd, false);
            if (sRez == -1)
                THROW(ERR_COMMON, "H264 stream error: Not enough buffer for write headers");
            curPos += sRez;
        }
    }

    if (needInsTimingSEI)
    {
        if (m_firstSEIWarn)
        {
            m_firstSEIWarn = false;
            LTRACE(LT_INFO, 2, "H264 bitstream changed: insert pict timing and buffering period SEI units");
        }

        if (m_lastIFrame)
        {
            if (m_lastSliceIDR)
            {
                SEIUnit sei;
                sei.initial_cpb_removal_delay[0] = 855000;
                sei.initial_cpb_removal_delay_offset[0] = 0;
                curPos += writeSEIMessage(curPos, dstEnd, sei, SEI_MSG_BUFFERING_PERIOD);
            }

            if (m_mvcSubStream)
            {
                if (!m_bdRomMetaDataMsg.empty())
                {
                    // we got slice. fill previous metadata SEI message
                    auto pts90k = m_curDts / INT_FREQ_TO_TS_FREQ + m_startPts;
                    uint8_t *srcData = m_bdRomMetaDataMsg.data();
                    SEIUnit::updateMetadataPts(srcData + m_bdRomMetaDataMsgPtsPos, pts90k);

                    uint8_t *prevPos = curPos;
                    curPos = writeNalPrefix(curPos);
                    curPos += NALUnit::encodeNAL(srcData, srcData + m_bdRomMetaDataMsg.size(), curPos, dstEnd - curPos);
                    if (priorityData)
                        priorityData->push_back(std::make_pair<int, int>(static_cast<int>(prevPos - dstBuffer),
                                                                         static_cast<int>(curPos - prevPos)));
                    m_bdRomMetaDataMsg.clear();
                }
            }
        }

        SEIUnit sei;
        sei.cpb_removal_delay = m_removalDelay;
        sei.dpb_output_delay =
            static_cast<int>(((m_curPts - m_curDts + m_pcrIncPerFrame * getFrameDepth()) * 2) / m_pcrIncPerFrame);
        sei.pic_struct = m_lastPictStruct;

        if (m_lastSliceIDR)
            m_removalDelay = 0;
        m_removalDelay += 2;
        curPos += writeSEIMessage(curPos, dstEnd, sei, SEI_MSG_PIC_TIMING);
    }
    m_firstFileFrame = false;
    return static_cast<int>(curPos - dstBuffer);
}

int H264StreamReader::getTSDescriptor(uint8_t *dstBuff, bool blurayMode, const bool hdmvDescriptors)
{
    SliceUnit slice;
    if (m_firstDecodeNal)
    {
        additionalStreamCheck(m_buffer, m_bufEnd);
        m_firstDecodeNal = false;
    }

    if (hdmvDescriptors)
    {
        // Blu-ray core specifications Table 9-10 - HDMV_video_registration_descriptor
        *dstBuff++ = static_cast<int>(TSDescriptorTag::REGISTRATION);  // descriptor tag
        *dstBuff++ = 8;                                                // descriptor length
        *dstBuff++ = 'H';
        *dstBuff++ = 'D';
        *dstBuff++ = 'M';
        *dstBuff++ = 'V';
        *dstBuff++ = 0xff;  // stuffing byte

        int video_format, frame_rate_index, aspect_ratio_index;
        M2TSStreamInfo::blurayStreamParams(getFPS(), getInterlaced(), getStreamWidth(), getStreamHeight(),
                                           static_cast<int>(getStreamAR()), &video_format, &frame_rate_index,
                                           &aspect_ratio_index);
        *dstBuff++ = !m_mvcSubStream ? static_cast<uint8_t>(StreamType::VIDEO_H264)
                                     : static_cast<uint8_t>(StreamType::VIDEO_MVC);  // stream_coding_type
        *dstBuff++ = static_cast<uint8_t>(video_format << 4 | frame_rate_index);     // video_format + frame_rate
        *dstBuff = static_cast<uint8_t>(aspect_ratio_index << 4 | 0xf);              // aspect ratio + stuffing_bits

        return 10;  // total descriptor length
    }

    // Not HDMV => AVC video descriptor according to H.222 Table 2-92
    // find SPS
    for (uint8_t *nal = NALUnit::findNextNAL(m_buffer, m_bufEnd); nal < m_bufEnd - 4;
         nal = NALUnit::findNextNAL(nal, m_bufEnd))
    {
        const auto nalType = static_cast<NALUnit::NALType>(*nal & 0x1f);
        if (nalType == NALUnit::NALType::nuSPS || nalType == NALUnit::NALType::nuSubSPS)
        {
            processSPS(nal);
            *dstBuff++ = static_cast<uint8_t>(TSDescriptorTag::AVC);   // descriptor tag
            *dstBuff++ = 4;                                            // descriptor length
            *dstBuff++ = nal[1];                                       // profile_idc
            *dstBuff++ = nal[2];                                       // constraint flags
            *dstBuff++ = m_forcedLevel == 0 ? nal[3] : m_forcedLevel;  // level_idc
            *dstBuff = 0xbf;  // AVC_still_present, AVC_24_hour flag, Frame_Packing_SEI_not_present_flag, reserved

            return 6;
        }
    }
    // avoid compiler warning: control reaches end of non-void function
    assert(0);  // no SPS nal: exit
    return 0;
}

void H264StreamReader::updateStreamFps(void *nalUnit, uint8_t *buff, uint8_t *nextNal, const int oldSpsLen)
{
    const auto sps = static_cast<SPSUnit *>(nalUnit);
    sps->setFps(m_fps);
    const auto tmpBuffer = new uint8_t[oldSpsLen + 16];
    const long newSpsLen = sps->serializeBuffer(tmpBuffer, tmpBuffer + oldSpsLen + 16, false);
    if (newSpsLen == -1)
        THROW(ERR_COMMON, "Not enough buffer");
    if (newSpsLen != oldSpsLen)
    {
        const int sizeDiff = newSpsLen - oldSpsLen;
        if (m_bufEnd + sizeDiff > m_tmpBuffer + TMP_BUFFER_SIZE)
            THROW(ERR_COMMON, "Not enough buffer");
        memmove(nextNal + sizeDiff, nextNal, m_bufEnd - nextNal);
        m_bufEnd += sizeDiff;
        // m_dataLen += sizeDiff;
    }
    memcpy(buff, tmpBuffer, newSpsLen);
    delete[] tmpBuffer;
}

void H264StreamReader::updateHRDParam(SPSUnit *sps) const
{
    // correct HRD parameters here (this function is called for every SPS nal unit)
    if (!m_needSeiCorrection)
        return;

    // if (!orig_hrd_parameters_present_flag && !orig_vcl_parameters_present_flag)
    //{
    //}
    if (!sps->vui_parameters_present_flag)
        sps->setFps(m_fps);
    sps->insertHrdParameters();
}

// All the remaining checks of the stream, executed at start

void H264StreamReader::checkPyramid(const int frameNum, int *fullPicOrder, const bool nextFrameFound)
{
    int depth = frameNum - *fullPicOrder;
    if (depth > 3)
        depth = 3;  // prevent large delay. It is not to be happend in normanl stream, but this protection for invalid
                    // streams.
    if (depth > m_frameDepth)
    {
        if (nextFrameFound)
        {
            m_frameDepth = depth;
            LTRACE(LT_INFO, 2,
                   "B-pyramid level " << m_frameDepth - 1 << " detected. Shift DTS to " << m_frameDepth << " frames");
        }
        else
        {
            *fullPicOrder = frameNum - m_frameDepth;  // special case for last frame
        }
    }
}

void H264StreamReader::additionalStreamCheck(uint8_t *buff, uint8_t *end)
{
    // Make sure that the stream doesn't appear to be progressive, since pic_order_cnt_lsb doesn't reach values higher
    // than 2 in these streams.
    bool SEIFound = false;
    SliceUnit slice;
    SPSUnit tmpsps;

    PPSUnit pps1;
    bool differPPS = false;

    int rez = 0;
    SEIUnit sei;
    uint8_t *nalEnd = nullptr;
    bool tmpspsfound = false;
    for (uint8_t *nal = NALUnit::findNextNAL(buff, end); nal != end; nal = NALUnit::findNextNAL(nal, end))
    {
        switch (static_cast<NALUnit::NALType>(*nal & 0x1f))
        {
        case NALUnit::NALType::nuSubSPS:
            m_mvcSubStream = true;
            [[fallthrough]];
        case NALUnit::NALType::nuSPS:
            nalEnd = NALUnit::findNALWithStartCode(nal, m_bufEnd, true);
            tmpsps.decodeBuffer(nal, nalEnd);
            tmpsps.deserialize();
            orig_hrd_parameters_present_flag = tmpsps.nalHrdParams.isPresent;
            orig_vcl_parameters_present_flag = tmpsps.vclHrdParams.isPresent;
            tmpspsfound = true;
            break;
        case NALUnit::NALType::nuPPS:
        {
            PPSUnit pps2;
            nalEnd = NALUnit::findNALWithStartCode(nal, m_bufEnd, true);
            if (pps1.m_nalBufferLen == 0)
            {
                pps1.decodeBuffer(nal, nalEnd);
                pps1.deserialize();
            }
            pps2.decodeBuffer(nal, nalEnd);
            pps2.deserialize();
            if (pps1.pic_parameter_set_id == pps2.pic_parameter_set_id)
            {
                if (pps2.m_nalBufferLen != pps1.m_nalBufferLen ||
                    memcmp(pps2.m_nalBuffer, pps1.m_nalBuffer, pps1.m_nalBufferLen))
                    differPPS = true;
            }
            // tmpppsList.push_back(new PPSUnit());
            // tmpppsList[tmpppsList.size()-1]->decodeBuffer(nal, nalEnd);
            break;
        }
        case NALUnit::NALType::nuSEI:
            if (tmpspsfound)
            {
                nalEnd = NALUnit::findNALWithStartCode(nal, end, true);
                if (nalEnd == end)
                    break;
                sei.decodeBuffer(nal, nalEnd);
                sei.deserialize(tmpsps, orig_hrd_parameters_present_flag || orig_vcl_parameters_present_flag);
                if (sei.hasProcessedMessage(SEI_MSG_BUFFERING_PERIOD) || sei.hasProcessedMessage(SEI_MSG_PIC_TIMING))
                {
                    SEIFound = true;
                }

                if (sei.number_of_offset_sequences >= 0)
                {
                    number_of_offset_sequences = sei.number_of_offset_sequences;
                }
            }
            break;
        default:
            break;
        }
    }

    if (m_removePulldown || m_insertSEIMethod == SeiMethod::SEI_InsertForce)
        m_needSeiCorrection = true;
    else if (m_insertSEIMethod == SeiMethod::SEI_InsertAuto)
    {
        m_needSeiCorrection =
            orig_hrd_parameters_present_flag == 0 || orig_vcl_parameters_present_flag == 0 || !SEIFound;
        if (m_needSeiCorrection && differPPS)
        {
            LTRACE(LT_INFO, 2, "H264 stream contains different PPS with same pps_id. SEI correction is turned off");
            m_needSeiCorrection = false;
        }
    }
    else
        m_needSeiCorrection = false;

    m_forceLsbDiv = -1;
    for (uint8_t *nal = NALUnit::findNextNAL(buff, end); nal != end; nal = NALUnit::findNextNAL(nal, end))
    {
        switch (static_cast<NALUnit::NALType>(*nal & 0x1f))
        {
        case NALUnit::NALType::nuSPS:
        case NALUnit::NALType::nuSubSPS:
            processSPS(nal);
            break;
        case NALUnit::NALType::nuPPS:
            processPPS(nal);
            break;
        case NALUnit::NALType::nuSliceIDR:
        case NALUnit::NALType::nuSliceNonIDR:
        case NALUnit::NALType::nuSliceA:
        case NALUnit::NALType::nuSliceB:
        case NALUnit::NALType::nuSliceC:
        case NALUnit::NALType::nuSliceExt:
            rez = deserializeSliceHeader(slice, nal, end);
            if (rez == 0)
            {
                // coded fields
                if (slice.getSPS()->frame_mbs_only_flag == 0 && slice.m_field_pic_flag)
                    m_forceLsbDiv = 1;
                // coded frames
                else if (slice.pic_order_cnt_lsb % 2 == 1)
                    m_forceLsbDiv = 0;
            }
            break;
        default:
            break;
        }
    }

    if (m_forceLsbDiv == -1)
        m_forceLsbDiv = 1;

    int frameNum = -1;
    for (uint8_t *nal = NALUnit::findNextNAL(buff, end); nal != end; nal = NALUnit::findNextNAL(nal, end))
    {
        switch (static_cast<NALUnit::NALType>(*nal & 0x1f))
        {
        case NALUnit::NALType::nuSliceIDR:
        case NALUnit::NALType::nuSliceNonIDR:
        case NALUnit::NALType::nuSliceA:
        case NALUnit::NALType::nuSliceB:
        case NALUnit::NALType::nuSliceC:
        case NALUnit::NALType::nuSliceExt:
            rez = deserializeSliceHeader(slice, nal, end);
            if (rez == 0 && slice.first_mb_in_slice == 0 && slice.getSPS()->pic_order_cnt_type != 2)
            {
                int picOrder = calcPicOrder(slice);
                if (slice.isIDR())
                    frameNum = -1;
                checkPyramid(++frameNum, &picOrder, true);
            }
            break;
        default:
            break;
        }
    }
    prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
}

int H264StreamReader::calcPicOrder(SliceUnit &slice)
{
    if (slice.getSPS()->pic_order_cnt_type != 0)
        THROW(ERR_H264_UNSUPPORTED_PARAMETER,
              "SPS picture order " << slice.getSPS()->pic_order_cnt_type << " not supported.");

    if (slice.isIDR())
        prevPicOrderCntMsb = prevPicOrderCntLsb = 0;

    const int PicOrderCntLsb = slice.pic_order_cnt_lsb;
    const int MaxPicOrderCntLsb = 1 << slice.getSPS()->log2_max_pic_order_cnt_lsb;
    int PicOrderCntMsb;

    if ((PicOrderCntLsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - PicOrderCntLsb) >= (MaxPicOrderCntLsb / 2)))
        PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if ((PicOrderCntLsb > prevPicOrderCntLsb) && ((PicOrderCntLsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2)))
        PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else
        PicOrderCntMsb = prevPicOrderCntMsb;

    if (slice.memory_management_control_operation == 5)
    {
        prevPicOrderCntMsb = 0;
        if (!slice.bottom_field_flag)
            prevPicOrderCntLsb = PicOrderCntMsb + PicOrderCntLsb;
        else
            prevPicOrderCntLsb = 0;
    }
    else
    {
        prevPicOrderCntMsb = PicOrderCntMsb;
        prevPicOrderCntLsb = PicOrderCntLsb;
    }
    return (PicOrderCntMsb + PicOrderCntLsb) >> m_forceLsbDiv;
}

int H264StreamReader::getIdrPrevFrames(uint8_t *buff, uint8_t *bufEnd)
{
    int prevPicCnt = 0;
    int deserializeRez;
    for (uint8_t *nal = NALUnit::findNextNAL(buff + 4, m_bufEnd); nal < m_bufEnd;
         nal = NALUnit::findNextNAL(nal, m_bufEnd))
    {
        SliceUnit slice;

        int MaxPicOrderCntLsbHalf;
        switch (static_cast<NALUnit::NALType>(*nal & 0x1f))
        {
        case NALUnit::NALType::nuSliceIDR:
        case NALUnit::NALType::nuSliceNonIDR:
        case NALUnit::NALType::nuSliceA:
        case NALUnit::NALType::nuSliceB:
        case NALUnit::NALType::nuSliceC:
        case NALUnit::NALType::nuSliceExt:
            deserializeRez = deserializeSliceHeader(slice, nal, bufEnd);
            if (deserializeRez == NOT_ENOUGH_BUFFER && !m_eof)
                return -1;
            if (deserializeRez != 0)
                return 0;
            if (slice.isIDR() && slice.first_mb_in_slice == 0)
                return 0;
            if (slice.first_mb_in_slice != 0)
                break;
            MaxPicOrderCntLsbHalf = 1 << (slice.getSPS()->log2_max_pic_order_cnt_lsb - 1);
            if (slice.pic_order_cnt_lsb > MaxPicOrderCntLsbHalf)
            {
                if (slice.bottom_field_flag == 0)
                    prevPicCnt++;
            }
            else
                return prevPicCnt;
            break;
        default:
            break;
        }
    }
    if (m_eof)
        return prevPicCnt;
    return -1;
}

int H264StreamReader::intDecodeNAL(uint8_t *buff)
{
    const auto nal_unit_type = static_cast<NALUnit::NALType>(*buff & 0x1f);
    int nalRez;
    uint8_t *nextNal;
    m_spsPpsFound = false;

    // First NAL of Access Unit
    switch (nal_unit_type)
    {
    case NALUnit::NALType::nuPPS:
        // LTRACE(LT_DEBUG, 0, "PPS");
        nalRez = processPPS(buff);
        if (nalRez != 0)
            return nalRez;
        m_spsPpsFound = true;
        goto getAU;
    case NALUnit::NALType::nuSPS:
    case NALUnit::NALType::nuSubSPS:
        // LTRACE(LT_DEBUG, 0, "SPS");
        nalRez = processSPS(buff);
        if (nalRez != 0)
            return nalRez;
        m_spsCounter++;
        goto getAU;
    case NALUnit::NALType::nuSEI:
        // LTRACE(LT_DEBUG, 0, "SEI");
        nalRez = processSEI(buff);
        if (nalRez != 0)
            return nalRez;
        goto getAU;
    case NALUnit::NALType::nuDRD:
    case NALUnit::NALType::nuDelimiter:
        // LTRACE(LT_DEBUG, 0, "NAUD type " << int32ToStr(buff[1],16));
        m_delimiterFound = true;
    // Remaining NALs of Access Unit
    getAU:
        nextNal = NALUnit::findNextNAL(buff, m_bufEnd);
        while (true)
        {
            if (nextNal == m_bufEnd)
                return NOT_ENOUGH_BUFFER;
            switch (static_cast<NALUnit::NALType>(*nextNal & 0x1f))
            {
            case NALUnit::NALType::nuSEI:
                nalRez = processSEI(nextNal);
                if (nalRez != 0)
                    return nalRez;

                break;
            case NALUnit::NALType::nuDRD:
                if (m_blurayMode)
                    m_delimiterFound = true;
                break;
            case NALUnit::NALType::nuDelimiter:
                // LTRACE(LT_DEBUG, 0, "NAUD type " << int32ToStr(nextNal[1],16));
                m_delimiterFound = true;
                break;
            case NALUnit::NALType::nuSPS:
            case NALUnit::NALType::nuSubSPS:
                // LTRACE(LT_DEBUG, 0, "SPS");
                nalRez = processSPS(nextNal);
                if (nalRez != 0)
                    return nalRez;
                m_spsCounter++;
                break;
            case NALUnit::NALType::nuPPS:
                // LTRACE(LT_DEBUG, 0, "PPS");
                nalRez = processPPS(nextNal);
                if (nalRez != 0)
                    return nalRez;
                m_spsPpsFound = true;
                break;
            case NALUnit::NALType::nuSliceIDR:
                // m_openGOP = false;
            case NALUnit::NALType::nuSliceNonIDR:
            case NALUnit::NALType::nuSliceA:
            case NALUnit::NALType::nuSliceB:
            case NALUnit::NALType::nuSliceC:
            case NALUnit::NALType::nuSliceExt:
                nalRez = processSliceNal(nextNal);
                if (nalRez == 0)
                {
                    m_lastDecodedPos = nextNal;
                }
                return nalRez;
            default:
                break;
            }
            nextNal = NALUnit::findNextNAL(nextNal, m_bufEnd);
        }
    case NALUnit::NALType::nuSliceIDR:
    case NALUnit::NALType::nuSliceNonIDR:
    case NALUnit::NALType::nuSliceA:
    case NALUnit::NALType::nuSliceB:
    case NALUnit::NALType::nuSliceC:
    case NALUnit::NALType::nuSliceExt:
        nalRez = processSliceNal(buff);
        if (nalRez != 0)
            return nalRez;
        break;
    default:
        break;
    }
    return 0;
}

bool H264StreamReader::skipNal(uint8_t *nal)
{
    const auto nalType = static_cast<NALUnit::NALType>(*nal & 0x1f);

    if (nalType == NALUnit::NALType::nuFillerData)
        return true;

    if ((nalType == NALUnit::NALType::nuEOSeq || nalType == NALUnit::NALType::nuEOStream))
    {
        if (!m_eof || m_bufEnd - nal > 4)
        {
            updatedSPSList.clear();
            return true;
        }
    }

    if (!m_needSeiCorrection)
        return false;

    if (nalType == NALUnit::NALType::nuPPS || nalType == NALUnit::NALType::nuSPS ||
        nalType == NALUnit::NALType::nuSubSPS)
    {
        return true;  // SPS/PPS will be composed from our side on writePesHeader. It is required to compose SPS/PPS
                      // before SEI
    }
    if (nalType == NALUnit::NALType::nuSEI)
    {
        SEIUnit sei;
        const uint8_t *nextNal = NALUnit::findNALWithStartCode(nal, m_bufEnd, true);
        sei.decodeBuffer(nal, nextNal);
        sei.deserialize(*(m_spsMap.begin()->second),
                        orig_hrd_parameters_present_flag || orig_vcl_parameters_present_flag);

        if (sei.hasProcessedMessage(SEI_MSG_BUFFERING_PERIOD) || sei.hasProcessedMessage(SEI_MSG_PIC_TIMING))
        {
            return true;
        }
    }
    return false;
}

int H264StreamReader::processSEI(uint8_t *buff)
{
    if (!m_spsMap.empty())
    {
        SEIUnit lastSEI;
        uint8_t *nextNal = NALUnit::findNALWithStartCode(buff, m_bufEnd, true);
        if (nextNal == m_bufEnd)
            return NOT_ENOUGH_BUFFER;
        if (nextNal + 4 > m_bufEnd)
            return NOT_ENOUGH_BUFFER;
        lastSEI.pic_struct = -1;
        lastSEI.decodeBuffer(buff, nextNal);
        lastSEI.deserialize(*(m_spsMap.begin()->second),
                            orig_hrd_parameters_present_flag || orig_vcl_parameters_present_flag);

        bool timingSEI = false;
        bool nonTimingSEI = false;
        for (auto &msg : lastSEI.m_processedMessages)
        {
            if (msg == SEI_MSG_BUFFERING_PERIOD)
            {
                timingSEI = true;
            }
            else if (msg == SEI_MSG_PIC_TIMING)
            {
                timingSEI = true;
                m_lastPictStruct = lastSEI.pic_struct;
            }
            else if (msg == SEI_MSG_MVC_SCALABLE_NESTING)
            {
                ;
            }
            else
            {
                nonTimingSEI = true;
            }
        }

        if (m_mvcSubStream)
        {
            if (lastSEI.number_of_offset_sequences >= 0)
            {
                if (m_needSeiCorrection)
                {
                    // copy to buffer, then isert sei
                    m_bdRomMetaDataMsg.resize(lastSEI.m_nalBufferLen);
                    m_bdRomMetaDataMsgPtsPos = lastSEI.metadataPtsOffset;
                    memcpy(m_bdRomMetaDataMsg.data(), lastSEI.m_nalBuffer, lastSEI.m_nalBufferLen);
                }
                else
                {
                    // update inplace
                    m_priorityNalAddr = buff;
                    m_OffsetMetadataPtsAddr = lastSEI.metadataPtsOffset + buff;
                }
            }

            if (timingSEI && lastSEI.m_mvcHeaderLen > 0)
            {
                m_lastSeiMvcHeader.resize(lastSEI.m_mvcHeaderLen);
                memcpy(m_lastSeiMvcHeader.data(), lastSEI.m_mvcHeaderStart, lastSEI.m_mvcHeaderLen);
            }
        }

        if (timingSEI && !nonTimingSEI)

            return 0;

        if (timingSEI && nonTimingSEI && m_needSeiCorrection)
        {
            // remove timing part from muxed SEI
            int oldSize = static_cast<int>(nextNal - buff);
            lastSEI.removePicTimingSEI(*(m_spsMap.begin()->second));

            uint8_t tmpBuff[1024 * 3];
            int newSize = lastSEI.serializeBuffer(tmpBuff, tmpBuff + sizeof(tmpBuff), false);
            if (newSize == -1)
                THROW(ERR_COMMON, "Not enough buffer");

            assert(newSize > 2);

            int sizeDiff = newSize - oldSize;
            if (sizeDiff != 0)
            {
                if (m_bufEnd + sizeDiff > m_tmpBuffer + TMP_BUFFER_SIZE)
                    THROW(ERR_COMMON, "Not enough buffer");
                memmove(nextNal + sizeDiff, nextNal, m_bufEnd - nextNal);
                m_bufEnd += sizeDiff;
            }
            memcpy(buff, tmpBuff, newSize);
        }
    }
    return 0;
}

int H264StreamReader::getNalHrdLen(uint8_t *nal) const
{
    if (m_bufEnd - nal >= 3)
    {
        if (nal[0] == 0 && nal[1] == 0)
        {
            if (nal[2] == 1)
                return 3;
            if (m_bufEnd - nal >= 4 && nal[3] == 1)
                return 4;
        }
    }
    return 0;
}

bool H264StreamReader::findPPSForward(uint8_t *buff)
{
    bool spsFound = false;
    bool ppsFound = false;
    for (uint8_t *nal = NALUnit::findNextNAL(buff, m_bufEnd); nal != m_bufEnd;
         nal = NALUnit::findNextNAL(nal, m_bufEnd))
        switch (static_cast<NALUnit::NALType>(*nal & 0x1f))
        {
        case NALUnit::NALType::nuSPS:
        case NALUnit::NALType::nuSubSPS:
            processSPS(nal);
            spsFound = true;
            break;
        case NALUnit::NALType::nuPPS:
            processPPS(nal);
            ppsFound = true;
            break;
        default:
            break;
        }
    return spsFound && ppsFound;
}

int H264StreamReader::deserializeSliceHeader(SliceUnit &slice, uint8_t *buff, uint8_t *sliceEnd)
{
    int nalRez, toDecode;
    const int maxHeaderSize = static_cast<int>(sliceEnd - buff);
    do
    {
        uint8_t *tmpBuffer = m_decodedSliceHeader.data();
        const int tmpBufferSize = static_cast<int>(m_decodedSliceHeader.size());
        toDecode = FFMIN(tmpBufferSize - 8, maxHeaderSize);
        const int decodedLen = SliceUnit::decodeNAL(buff, buff + toDecode, tmpBuffer, tmpBufferSize);
        nalRez = slice.deserialize(tmpBuffer, tmpBuffer + decodedLen, m_spsMap, m_ppsMap);
        if (nalRez == NOT_ENOUGH_BUFFER && toDecode < maxHeaderSize)
            m_decodedSliceHeader.resize(m_decodedSliceHeader.size() + 1);
    } while (nalRez == NOT_ENOUGH_BUFFER && toDecode < maxHeaderSize);

    return nalRez;
}

int H264StreamReader::processSliceNal(uint8_t *buff)
{
    SliceUnit slice;
    uint8_t *sliceEnd = m_bufEnd;

    int nalRez = deserializeSliceHeader(slice, buff, sliceEnd);

    if (nalRez == NALUnit::SPS_OR_PPS_NOT_READY && m_isFirstFrame)
    {
        if (findPPSForward(buff + 1))
            nalRez = deserializeSliceHeader(slice, buff, sliceEnd);
    }

    if (nalRez != 0)
    {
        return nalRez;
    }

    if (slice.first_mb_in_slice != 0)
        return 0;

    if (slice.bottom_field_flag == 1)  // insrease PTS/DTS only for whole frame
        return 0;

    if (detectPrimaryPicType(slice, buff) != 0)
        return NOT_ENOUGH_BUFFER;

    // forceLsbDiv check
    if (slice.getSPS()->frame_mbs_only_flag && m_forceLsbDiv != 0)
    {
        if (slice.pic_order_cnt_lsb % 2 == 1)
        {
            if (m_totalFrameNum < 250)
            {
                m_forceLsbDiv = 0;
                LTRACE(LT_WARN, 2, "H264 warn: Force frame_mbs_only_flag division invalid detected. Cancel this flag.");
            }
            else
            {
                LTRACE(LT_WARN, 2,
                       "H264 warn: Unexpected pic_order_cnt_lsb value "
                           << slice.pic_order_cnt_lsb << ". FrameNum: " << m_totalFrameNum
                           << " slice type: " << sliceTypeStr[slice.slice_type]);
            }
        }
    }

    ////////////////////////////

    int fullPicOrder = 0;
    if (slice.getSPS()->pic_order_cnt_type != 2)
        fullPicOrder = calcPicOrder(slice);
    else
        m_frameDepth = 0;

    if (slice.isIDR())
    {
        // LTRACE(LT_INFO, 2, "got idr slice");
        m_frameNum = 0;
        if (slice.getSPS()->pic_order_cnt_type != 2)
            m_iFramePtsOffset = getIdrPrevFrames(buff, m_bufEnd);
        else
            m_iFramePtsOffset = 0;
        if (m_iFramePtsOffset == -1)
            return NOT_ENOUGH_BUFFER;
    }
    else
    {
        m_frameNum++;
        // m_frameNum = slice.frame_num;

        if (slice.getSPS()->pic_order_cnt_type == 2)
            fullPicOrder = m_frameNum;
    }
    fullPicOrder += m_iFramePtsOffset;

    m_isFirstFrame = false;
    m_totalFrameNum++;

    if (slice.isIDR())
    {
        m_idrSliceCnt++;
        m_picOrderOffset = 0;
    }
    if (m_idrSliceCnt == 0)
    {
        if (!m_bSliceFound && slice.slice_type == SliceUnit::B_TYPE)
        {
            m_bSliceFound = true;
            m_picOrderOffset = m_frameNum - fullPicOrder - 1;
        }
        if (m_bSliceFound)
        {
            fullPicOrder += m_picOrderOffset;
            if (fullPicOrder < 0)
                fullPicOrder = 0;
        }
        else
            fullPicOrder = m_frameNum;
    }

    // check b-pyramid
    checkPyramid(m_frameNum, &fullPicOrder, m_nextFrameFound);

    // increase PTS/DTS
    m_curDts += m_lastDtsInc;
    m_curPts = m_curDts + (fullPicOrder - m_frameNum) * m_pcrIncPerFrame;

    if (m_curPts > m_curDts && (!m_nextFrameFound || m_nextFrameIdr))
        m_curPts = m_curDts;

    if (m_OffsetMetadataPtsAddr)
    {
        const auto pts90k = m_curDts / INT_FREQ_TO_TS_FREQ + m_startPts;
        SEIUnit::updateMetadataPts(m_OffsetMetadataPtsAddr, pts90k);
        m_OffsetMetadataPtsAddr = nullptr;
    }
    // LTRACE(LT_INFO, 2, "delta=" << fullPicOrder - m_frameNum << " m_lastDtsInc=" << m_lastDtsInc);

    if (m_lastPicStruct == 5 || m_lastPicStruct == 6)  // 3 fields per frame. Used in pulldown
        m_lastDtsInc = m_pcrIncPerFrame + m_pcrIncPerField;
    else if (m_lastPicStruct == 7)  // frame doubling. Used in pulldown
        m_lastDtsInc = m_pcrIncPerFrame * 2;
    else if (m_lastPicStruct == 8)  // frame tripling. Used in pulldown
        m_lastDtsInc = m_pcrIncPerFrame * 3;
    else
        m_lastDtsInc = m_pcrIncPerFrame;

    if (m_removePulldown)
    {
        checkPulldownSync();
        m_testPulldownDts += m_lastDtsInc;
        m_lastDtsInc = m_pcrIncPerFrame;
    }

    m_lastSlicePPS = slice.pic_parameter_set_id;
    if (slice.getPPS() != nullptr)
        m_lastSliceSPS = slice.getPPS()->seq_parameter_set_id;
    else
        m_lastSliceSPS = -1;
    m_lastSliceIDR = slice.isIDR();
    m_lastIFrame = slice.isIFrame();

    return 0;
}

int H264StreamReader::detectPrimaryPicType(SliceUnit &firstSlice, uint8_t *buff)
{
    SliceUnit slice;
    m_nextFrameFound = false;
    m_nextFrameIdr = false;
    m_pict_type = -1;
    m_pict_type = (std::max)(m_pict_type, sliceTypeToPictType(firstSlice.slice_type));

    // if (firstSlice.orig_slice_type >= 5) // all other slice at this picture must be same type
    //	return 0; // OK

    uint8_t *nextSlice = NALUnit::findNextNAL(buff, m_bufEnd);
    if (nextSlice == m_bufEnd)
    {
        return m_eof ? 0 : NOT_ENOUGH_BUFFER;
    }

    while (true)
    {
        const int nalUnitType = *nextSlice;
        switch (static_cast<NALUnit::NALType>(nalUnitType & 0x1f))
        {
        case NALUnit::NALType::nuSliceIDR:
        case NALUnit::NALType::nuSliceNonIDR:
        case NALUnit::NALType::nuSliceA:
        case NALUnit::NALType::nuSliceB:
        case NALUnit::NALType::nuSliceC:
        case NALUnit::NALType::nuSliceExt:
            if (slice.deserializeSliceType(nextSlice, m_bufEnd) != 0)
            {
                return m_eof ? 0 : NOT_ENOUGH_BUFFER;
            }

            if (slice.first_mb_in_slice == 0)
            {
                m_nextFrameFound = true;
                m_nextFrameIdr = slice.isIDR();
                return 0;  // next frame found
            }
            m_pict_type = (std::max)(m_pict_type, sliceTypeToPictType(slice.slice_type));
            break;
        default:
            break;
        }
        nextSlice = NALUnit::findNextNAL(nextSlice, m_bufEnd);
        if (nextSlice == m_bufEnd)
        {
            return m_eof ? 0 : NOT_ENOUGH_BUFFER;
        }
    }
}

int H264StreamReader::sliceTypeToPictType(const int slice_type) const
{
    switch (slice_type)
    {
    case SliceUnit::P_TYPE:
    {
        if (m_pict_type < 3)
            return 1;
        return 6;
    }
    case SliceUnit::B_TYPE:
    {
        if (m_pict_type < 3)
            return 2;
        return 7;
    }
    case SliceUnit::I_TYPE:
    {
        if (m_pict_type < 3)
            return 0;
        return 5;
    }
    case SliceUnit::SP_TYPE:
    {
        if (m_pict_type == -1 || m_pict_type == 3)
            return 4;
        if (m_pict_type == 2)
            return 7;
        return 6;
    }
    case SliceUnit::SI_TYPE:
    {
        if (m_pict_type == -1)
            return 3;
        if (m_pict_type == 0)
            return 5;
        if (m_pict_type == 2)
            return 7;
        return 6;
    }
    default:;
    }
    return 0;
}

bool H264StreamReader::replaceToOwnSPS() const { return m_needSeiCorrection; }

int H264StreamReader::processSPS(uint8_t *buff)
{
    if (replaceToOwnSPS())
    {
        // do not updat SPS in stream because of we are going to insert own sps
        if (m_bufEnd - buff < 8)
            return NOT_ENOUGH_BUFFER;
        const unsigned seq_parameter_set_id = NALUnit::extractUEGolombCode(buff + 4, m_bufEnd);
        if (updatedSPSList.find(seq_parameter_set_id) != updatedSPSList.end())
            return 0;  // already processed
    }

    auto *sps = new SPSUnit();
    uint8_t *nextNal = NALUnit::findNALWithStartCode(buff, m_bufEnd, true);
    const int oldSpsLen = static_cast<int>(nextNal - buff);
    sps->decodeBuffer(buff, nextNal);
    const int nalRez = sps->deserialize();

    if (orig_hrd_parameters_present_flag != sps->nalHrdParams.isPresent ||
        orig_vcl_parameters_present_flag != sps->vclHrdParams.isPresent)
    {
        orig_hrd_parameters_present_flag = sps->nalHrdParams.isPresent;
        orig_vcl_parameters_present_flag = sps->vclHrdParams.isPresent;
        if (!m_spsChangeWarned)
        {
            LTRACE(LT_WARN, 2, "H264 warn: SPS parameters is not consistent throughout a stream");
            m_spsChangeWarned = true;
        }
    }

    // long sps fields can cause 00 00 0x code and it is required decode slice header
    if (nalRez != 0)
    {
        delete sps;
        return nalRez;  // not enough buffer
    }

    if (m_spsMap.find(sps->seq_parameter_set_id) == m_spsMap.end())
    {
        LTRACE(LT_INFO, 2, "Decoding H264 stream (track " << m_streamIndex << "): " << sps->getStreamDescr());
        if (m_forcedLevel != 0)
            LTRACE(LT_INFO, 2, "Change H264 level from " << sps->level_idc / 10.0 << " to " << m_forcedLevel / 10.0);
    }
    // update profile if needed
    if (m_forcedLevel != 0 && m_bufEnd - buff >= 4)
    {
        buff[3] = m_forcedLevel;
        sps->m_nalBuffer[3] = m_forcedLevel;
        sps->level_idc = m_forcedLevel;
    }

    updateFPS(sps, buff, nextNal, oldSpsLen);
    updateHRDParam(sps);
    if (sps->nalHrdParams.isPresent)
        updatedSPSList.insert(sps->seq_parameter_set_id);

    delete m_spsMap[sps->seq_parameter_set_id];
    m_spsMap[sps->seq_parameter_set_id] = sps;

    double sarAR = 1.0;
    if (sps->aspect_ratio_info_present_flag)
    {
        if (sps->aspect_ratio_idc >= 1 && sps->aspect_ratio_idc <= 16)
            sarAR = h264_ar_coeff[sps->aspect_ratio_idc];
        else if (sps->aspect_ratio_idc == Extended_SAR)
            sarAR = sps->sar_width / static_cast<double>(sps->sar_height);
    }
    fillAspectBySAR(sarAR);

    return 0;
}

int H264StreamReader::processPPS(uint8_t *buff)
{
    auto *pps = new PPSUnit();
    const uint8_t *nextNal = NALUnit::findNALWithStartCode(buff, m_bufEnd, true);

    pps->decodeBuffer(buff, nextNal);
    const int nalRez = pps->deserialize();
    if (nalRez != 0)
    {
        delete pps;
        return nalRez;  // not enough buffer
    }
    delete m_ppsMap[pps->pic_parameter_set_id];
    m_ppsMap[pps->pic_parameter_set_id] = pps;
    return 0;
}

int H264StreamReader::getStreamWidth() const
{
    if (m_spsMap.empty())
        return 0;
    return m_spsMap.begin()->second->getWidth();
}

int H264StreamReader::getStreamHeight() const
{
    if (m_spsMap.empty())
        return 0;
    return m_spsMap.begin()->second->getHeight();
}

bool H264StreamReader::getInterlaced()
{
    if (m_spsMap.empty())
        return true;
    return !m_spsMap.begin()->second->frame_mbs_only_flag;
}

void H264StreamReader::onShiftBuffer(const int offset)
{
    MPEGStreamReader::onShiftBuffer(offset);
    if (m_lastDecodedPos && m_priorityNalAddr >= m_lastDecodedPos)
        m_priorityNalAddr -= offset;
    else
        m_priorityNalAddr = nullptr;
    if (m_OffsetMetadataPtsAddr > m_lastDecodedPos)
        m_OffsetMetadataPtsAddr -= offset;
    else
        m_OffsetMetadataPtsAddr = nullptr;
}

bool H264StreamReader::isPriorityData(AVPacket *packet)
{
    if (packet->data + 3 == m_priorityNalAddr || packet->data + 4 == m_priorityNalAddr)
    {
        m_priorityNalAddr = nullptr;
        m_OffsetMetadataPtsAddr = nullptr;
        return true;
    }
    return false;
}
