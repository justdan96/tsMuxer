#include "vvcStreamReader.h"

#include <fs/systemlog.h>

#include "nalUnits.h"
#include "tsMuxer.h"
#include "tsPacket.h"
#include "vodCoreException.h"
#include "vvc.h"

using namespace std;

static constexpr int MAX_SLICE_HEADER = 64;

VVCStreamReader::VVCStreamReader()
    : MPEGStreamReader(),
      m_vps(new VvcVpsUnit()),
      m_sps(nullptr),
      m_pps(nullptr),
      m_slice(new VvcSliceHeader()),
      m_firstFrame(true),
      m_frameNum(0),
      m_fullPicOrder(0),
      m_picOrderBase(0),
      m_frameDepth(1),
      m_picOrderMsb(0),
      m_prevPicOrder(0),
      m_lastIFrame(false),
      m_firstFileFrame(false),
      m_vpsCounter(0),
      m_vpsSizeDiff(0)
{
}

VVCStreamReader::~VVCStreamReader()
{
    delete m_vps;
    delete m_sps;
    delete m_pps;
    delete m_slice;
}

CheckStreamRez VVCStreamReader::checkStream(uint8_t* buffer, int len)
{
    CheckStreamRez rez;
    uint8_t* end = buffer + len;

    for (uint8_t* nal = NALUnit::findNextNAL(buffer, end); nal < end - 4; nal = NALUnit::findNextNAL(nal, end))
    {
        if (*nal & 0x80)
            return rez;  // invalid nal
        const auto nalType = (VvcUnit::NalType)(nal[1] >> 3);
        uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, end, true);
        if (!m_eof && nextNal == end)
            break;

        switch (nalType)
        {
        case VvcUnit::NalType::VPS:
            m_vps->decodeBuffer(nal, nextNal);
            if (m_vps->deserialize())
                return rez;
            m_spsPpsFound = true;
            if (m_vps->num_units_in_tick)
                updateFPS(m_vps, nal, nextNal, 0);
            break;
        case VvcUnit::NalType::SPS:
            if (!m_sps)
                m_sps = new VvcSpsUnit();
            m_sps->decodeBuffer(nal, nextNal);
            if (m_sps->deserialize() != 0)
                return rez;
            m_spsPpsFound = true;
            updateFPS(m_sps, nal, nextNal, 0);
            break;
        case VvcUnit::NalType::PPS:
            if (!m_pps)
                m_pps = new VvcPpsUnit();
            m_pps->decodeBuffer(nal, nextNal);
            if (m_pps->deserialize() != 0)
                return rez;
            break;
        default:
            break;
        }

        // check Frame Depth on first slices
        if (isSlice(nalType) && (nal[2] & 0x80))
        {
            m_slice->decodeBuffer(nal, FFMIN(nal + MAX_SLICE_HEADER, nextNal));
            if (m_slice->deserialize(m_sps, m_pps))
                return rez;  // not enough buffer or error
            m_fullPicOrder = toFullPicOrder(m_slice, m_sps->log2_max_pic_order_cnt_lsb);
            incTimings();
        }
    }
    m_totalFrameNum = m_frameNum = m_fullPicOrder = 0;
    m_curDts = m_curPts = 0;

    if (m_sps && m_pps && m_pps->sps_id == m_sps->sps_id)
    {
        rez.codecInfo = vvcCodecInfo;
        rez.streamDescr = m_sps->getDescription();
        const size_t frSpsPos = rez.streamDescr.find("Frame rate: not found");
        if (frSpsPos != string::npos)
        {
            rez.streamDescr = rez.streamDescr.substr(0, frSpsPos);
            if (m_vps)
                rez.streamDescr += string(" ") + m_vps->getDescription();
        }
    }

    return rez;
}

int VVCStreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    if (m_firstFrame)
        CheckStreamRez rez = checkStream(m_buffer, (int)(m_bufEnd - m_buffer));

    if (hdmvDescriptors)
    {
        *dstBuff++ = (uint8_t)TSDescriptorTag::HDMV;  // descriptor tag
        *dstBuff++ = 8;                               // descriptor length
        memcpy(dstBuff, "HDMV\xff", 5);
        dstBuff += 5;

        *dstBuff++ = (int)StreamType::VIDEO_H266;  // stream_coding_type
        int video_format, frame_rate_index, aspect_ratio_index;
        M2TSStreamInfo::blurayStreamParams(getFPS(), getInterlaced(), getStreamWidth(), getStreamHeight(),
                                           (int)getStreamAR(), &video_format, &frame_rate_index, &aspect_ratio_index);

        *dstBuff++ = (video_format << 4) + frame_rate_index;
        *dstBuff++ = (aspect_ratio_index << 4) + 0xf;

        return 10;  // total descriptor length
    }

    const uint8_t* descStart = dstBuff;

    // ITU-T Rec.H.222 Table 2-133 - VVC video descriptor
    *dstBuff++ = (uint8_t)TSDescriptorTag::VVC;
    uint8_t* descLength = dstBuff++;  // descriptor length, filled at the end
    *dstBuff++ = (m_sps->profile_idc << 1) | m_sps->tier_flag;
    *dstBuff++ = m_sps->ptl_num_sub_profiles;
    auto bufPos = (uint32_t*)dstBuff;
    for (const auto i : m_sps->general_sub_profile_idc) *bufPos++ = i;
    dstBuff = (uint8_t*)bufPos;
    *dstBuff++ = (m_sps->progressive_source_flag << 7) | (m_sps->interlaced_source_flag << 6) |
                 (m_sps->non_packed_constraint_flag << 5) | (m_sps->ptl_frame_only_constraint_flag << 4);
    *dstBuff++ = m_sps->level_idc;
    *dstBuff++ = 0;
    *dstBuff++ = 0xc0;

    const uint8_t* descEnd = dstBuff;
    const auto descSize = (int)(descEnd - descStart);
    *descLength = descSize - 2;  // fill descriptor length

    return descSize;
}

void VVCStreamReader::updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int)
{
    const int oldNalSize = (int)(nextNal - buff);
    m_vpsSizeDiff = 0;
    const auto vps = (VvcVpsUnit*)nalUnit;
    vps->setFPS(m_fps);
    const auto tmpBuffer = new uint8_t[vps->nalBufferLen() + 16];
    const long newSpsLen = vps->serializeBuffer(tmpBuffer, tmpBuffer + vps->nalBufferLen() + 16);
    if (newSpsLen == -1)
        THROW(ERR_COMMON, "Not enough buffer");

    if (m_bufEnd && newSpsLen != oldNalSize)
    {
        m_vpsSizeDiff = newSpsLen - oldNalSize;
        if (m_bufEnd + m_vpsSizeDiff > m_tmpBuffer + TMP_BUFFER_SIZE)
            THROW(ERR_COMMON, "Not enough buffer");
        memmove(nextNal + m_vpsSizeDiff, nextNal, m_bufEnd - nextNal);
        m_bufEnd += m_vpsSizeDiff;
    }
    memcpy(buff, tmpBuffer, newSpsLen);

    delete[] tmpBuffer;
}

int VVCStreamReader::getStreamWidth() const { return m_sps ? m_sps->pic_width_max_in_luma_samples : 0; }

int VVCStreamReader::getStreamHeight() const { return m_sps ? m_sps->pic_height_max_in_luma_samples : 0; }

double VVCStreamReader::getStreamFPS(void* curNalUnit)
{
    double fps = 0;
    if (m_vps)
        fps = m_vps->getFPS();
    if (fps == 0.0 && m_sps)
        fps = m_sps->getFPS();
    return fps;
}

bool VVCStreamReader::skipNal(uint8_t* nal)
{
    const auto nalType = (VvcUnit::NalType)(nal[1] >> 3);

    if (nalType == VvcUnit::NalType::FD)
        return true;

    if ((nalType == VvcUnit::NalType::EOS || nalType == VvcUnit::NalType::EOB))
    {
        if (!m_eof || m_bufEnd - nal > 4)
        {
            return true;
        }
    }

    return false;
}

bool VVCStreamReader::isSlice(VvcUnit::NalType nalType) const
{
    if (!m_sps || !m_pps)
        return false;

    switch (nalType)
    {
    case VvcUnit::NalType::TRAIL:
    case VvcUnit::NalType::STSA:
    case VvcUnit::NalType::RADL:
    case VvcUnit::NalType::RASL:
    case VvcUnit::NalType::IDR_W_RADL:
    case VvcUnit::NalType::IDR_N_LP:
    case VvcUnit::NalType::CRA:
    case VvcUnit::NalType::GDR:
        return true;
    default:
        return false;
    }
}

bool VVCStreamReader::isSuffix(VvcUnit::NalType nalType) const
{
    if (!m_sps || !m_pps)
        return false;

    switch (nalType)
    {
    case VvcUnit::NalType::SUFFIX_APS:
    case VvcUnit::NalType::SUFFIX_SEI:
    case VvcUnit::NalType::EOS:
    case VvcUnit::NalType::EOB:
    case VvcUnit::NalType::FD:
    case VvcUnit::NalType::RSV_NVCL_27:
    case VvcUnit::NalType::UNSPEC_30:
    case VvcUnit::NalType::UNSPEC_31:
        return true;
    default:
        return false;
    }

    return (nalType == VvcUnit::NalType::FD || nalType == VvcUnit::NalType::SUFFIX_APS);
}

void VVCStreamReader::incTimings()
{
    if (m_totalFrameNum++ > 0)
        m_curDts += m_pcrIncPerFrame;
    const int delta = m_frameNum - m_fullPicOrder;
    m_curPts = m_curDts - delta * m_pcrIncPerFrame;
    m_frameNum++;
    m_firstFrame = false;

    if (delta > m_frameDepth)
    {
        m_frameDepth = delta;
        LTRACE(LT_INFO, 2,
               "B-pyramid level " << m_frameDepth - 1 << " detected. Shift DTS to " << m_frameDepth << " frames");
    }
}

int VVCStreamReader::toFullPicOrder(VvcSliceHeader* slice, int pic_bits)
{
    if (slice->isIDR())
    {
        m_picOrderBase = m_frameNum;
        m_picOrderMsb = 0;
        m_prevPicOrder = 0;
    }
    else
    {
        const int range = 1 << pic_bits;

        if (slice->pic_order_cnt_lsb < m_prevPicOrder && m_prevPicOrder - slice->pic_order_cnt_lsb >= range / 2)
            m_picOrderMsb += range;
        else if (slice->pic_order_cnt_lsb > m_prevPicOrder && slice->pic_order_cnt_lsb - m_prevPicOrder >= range / 2)
            m_picOrderMsb -= range;

        m_prevPicOrder = slice->pic_order_cnt_lsb;
    }

    return slice->pic_order_cnt_lsb + m_picOrderMsb + m_picOrderBase;
}

void VVCStreamReader::storeBuffer(MemoryBlock& dst, const uint8_t* data, const uint8_t* dataEnd)
{
    dataEnd--;
    while (dataEnd > data && dataEnd[-1] == 0) dataEnd--;
    if (dataEnd > data)
    {
        dst.resize((int)(dataEnd - data));
        memcpy(dst.data(), data, dataEnd - data);
    }
}

int VVCStreamReader::intDecodeNAL(uint8_t* buff)
{
    int rez = 0;
    bool sliceFound = false;
    m_spsPpsFound = false;
    m_lastIFrame = false;

    const uint8_t* prevPos = nullptr;
    uint8_t* curPos = buff;
    uint8_t* nextNal = NALUnit::findNextNAL(curPos, m_bufEnd);

    if (!m_eof && nextNal == m_bufEnd)
        return NOT_ENOUGH_BUFFER;

    while (curPos < m_bufEnd)
    {
        const auto nalType = (VvcUnit::NalType)((*curPos >> 1) & 0x3f);
        if (isSlice(nalType))
        {
            if (curPos[2] & 0x80)  // slice.first_slice
            {
                if (sliceFound)
                {  // first slice of next frame: case where there is no non-VCL NAL between the two frames
                    m_lastDecodedPos = prevPos;  // next frame started
                    incTimings();
                    return 0;
                }
                else
                {  // first slice of current frame
                    m_slice->decodeBuffer(curPos, FFMIN(curPos + MAX_SLICE_HEADER, nextNal));
                    rez = m_slice->deserialize(m_sps, m_pps);
                    if (rez)
                        return rez;  // not enough buffer or error
                    if (nalType >= VvcUnit::NalType::IDR_W_RADL)
                        m_lastIFrame = true;
                    m_fullPicOrder = toFullPicOrder(m_slice, m_sps->log2_max_pic_order_cnt_lsb);
                }
            }
            sliceFound = true;
        }
        else if (!isSuffix(nalType))
        {  // first non-VCL prefix NAL (AUD, SEI...) following current frame
            if (sliceFound)
            {
                incTimings();
                m_lastDecodedPos = prevPos;  // next frame started
                return 0;
            }

            uint8_t* nextNalWithStartCode = nextNal[-4] == 0 ? nextNal - 4 : nextNal - 3;

            switch (nalType)
            {
            case VvcUnit::NalType::VPS:
                m_vps->decodeBuffer(curPos, nextNalWithStartCode);
                rez = m_vps->deserialize();
                if (rez)
                    return rez;
                m_spsPpsFound = true;
                m_vpsCounter++;
                m_vpsSizeDiff = 0;
                if (m_vps->num_units_in_tick)
                    updateFPS(m_vps, curPos, nextNalWithStartCode, 0);
                nextNal += m_vpsSizeDiff;
                storeBuffer(m_vpsBuffer, curPos, nextNalWithStartCode);
                break;
            case VvcUnit::NalType::SPS:
                if (!m_sps)
                    m_sps = new VvcSpsUnit();
                m_sps->decodeBuffer(curPos, nextNalWithStartCode);
                rez = m_sps->deserialize();
                if (rez)
                    return rez;
                m_spsPpsFound = true;
                updateFPS(m_sps, curPos, nextNalWithStartCode, 0);
                storeBuffer(m_spsBuffer, curPos, nextNalWithStartCode);
                break;
            case VvcUnit::NalType::PPS:
                if (!m_pps)
                    m_pps = new VvcPpsUnit();
                m_pps->decodeBuffer(curPos, nextNalWithStartCode);
                rez = m_pps->deserialize();
                if (rez)
                    return rez;
                m_spsPpsFound = true;
                storeBuffer(m_ppsBuffer, curPos, nextNalWithStartCode);
                break;
            default:
                break;
            }
        }
        prevPos = curPos;
        curPos = nextNal;
        nextNal = NALUnit::findNextNAL(curPos, m_bufEnd);

        if (!m_eof && nextNal == m_bufEnd)
            return NOT_ENOUGH_BUFFER;
    }
    if (m_eof)
    {
        m_lastDecodedPos = m_bufEnd;
        return 0;
    }
    else
        return NEED_MORE_DATA;
}

uint8_t* VVCStreamReader::writeNalPrefix(uint8_t* curPos)
{
    if (!m_shortStartCodes)
        *curPos++ = 0;
    *curPos++ = 0;
    *curPos++ = 0;
    *curPos++ = 1;
    return curPos;
}

uint8_t* VVCStreamReader::writeBuffer(MemoryBlock& srcData, uint8_t* dstBuffer, uint8_t* dstEnd)
{
    if (srcData.isEmpty())
        return dstBuffer;
    const int64_t bytesLeft = dstEnd - dstBuffer;
    const int64_t requiredBytes = srcData.size() + 3 + (m_shortStartCodes ? 0 : 1);
    if (bytesLeft < requiredBytes)
        return dstBuffer;

    dstBuffer = writeNalPrefix(dstBuffer);
    memcpy(dstBuffer, srcData.data(), srcData.size());
    dstBuffer += srcData.size();
    return dstBuffer;
}

int VVCStreamReader::writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                                       PriorityDataInfo* priorityData)
{
    uint8_t* curPos = dstBuffer;

    if (avPacket.size > 4 && avPacket.size < dstEnd - dstBuffer)
    {
        const int offset = avPacket.data[2] == 1 ? 3 : 4;
        const auto nalType = (VvcUnit::NalType)((avPacket.data[offset] >> 1) & 0x3f);
        if (nalType == VvcUnit::NalType::AUD)
        {
            // place delimiter at first place
            memcpy(curPos, avPacket.data, avPacket.size);
            curPos += avPacket.size;
            avPacket.size = 0;
            avPacket.data = nullptr;
        }
    }

    const bool needInsSpsPps = m_firstFileFrame && !(avPacket.flags & AVPacket::IS_SPS_PPS_IN_GOP);
    if (needInsSpsPps)
    {
        avPacket.flags |= AVPacket::IS_SPS_PPS_IN_GOP;

        curPos = writeBuffer(m_vpsBuffer, curPos, dstEnd);
        curPos = writeBuffer(m_spsBuffer, curPos, dstEnd);
        curPos = writeBuffer(m_ppsBuffer, curPos, dstEnd);
    }

    m_firstFileFrame = false;
    return (int)(curPos - dstBuffer);
}
