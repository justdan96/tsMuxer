#include "vvcStreamReader.h"

#include <fs/systemlog.h>

#include "nalUnits.h"
#include "tsMuxer.h"
#include "tsPacket.h"
#include "vodCoreException.h"
#include "vvc.h"

using namespace std;

static const int MAX_SLICE_HEADER = 64;
static const int VVC_DESCRIPTOR_TAG = 0x39;

VVCStreamReader::VVCStreamReader()
    : MPEGStreamReader(),
      m_vps(0),
      m_sps(0),
      m_pps(0),
      m_firstFrame(true),
      m_frameNum(0),
      m_fullPicOrder(0),
      m_frameDepth(1),

      m_picOrderMsb(0),
      m_prevPicOrder(0),
      m_picOrderBase(0),
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
}

CheckStreamRez VVCStreamReader::checkStream(uint8_t* buffer, int len)
{
    CheckStreamRez rez;
    VvcSliceHeader slice;

    uint8_t* end = buffer + len;
    for (uint8_t* nal = NALUnit::findNextNAL(buffer, end); nal < end - 4; nal = NALUnit::findNextNAL(nal, end))
    {
        if (*nal & 0x80)
            return rez;  // invalid nal
        int nalType = (nal[1] >> 3);
        uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, end, true);

        switch (nalType)
        {
        case V_VPS:
            if (!m_vps)
                m_vps = new VvcVpsUnit();
            m_vps->decodeBuffer(nal, nextNal);
            if (m_vps->deserialize())
                return rez;
            m_spsPpsFound = true;
            if (m_vps->num_units_in_tick)
                updateFPS(m_vps, nal, nextNal, 0);
            break;
        case V_SPS:
            if (!m_sps)
                m_sps = new VvcSpsUnit();
            m_sps->decodeBuffer(nal, nextNal);
            if (m_sps->deserialize() != 0)
                return rez;
            m_spsPpsFound = true;
            updateFPS(m_sps, nal, nextNal, 0);
            break;
        case V_PPS:
            if (!m_pps)
                m_pps = new VvcPpsUnit();
            m_pps->decodeBuffer(nal, nextNal);
            if (m_pps->deserialize() != 0)
                return rez;
        }

        // check Frame Depth on first slices
        if (isSlice(nalType) && (nal[2] & 0x80))
        {
            slice.decodeBuffer(nal, FFMIN(nal + MAX_SLICE_HEADER, nextNal));
            if (slice.deserialize(m_sps, m_pps))
                return rez;  // not enough buffer or error
            m_fullPicOrder = toFullPicOrder(&slice, m_sps->log2_max_pic_order_cnt_lsb);
            incTimings();
        }
    }
    m_totalFrameNum = m_frameNum = m_fullPicOrder = 0;
    m_curDts = m_curPts = 0;

    if (m_sps && m_pps && m_pps->sps_id == m_sps->sps_id)
    {
        rez.codecInfo = vvcCodecInfo;
        rez.streamDescr = m_sps->getDescription();
        size_t frSpsPos = rez.streamDescr.find("Frame rate: not found");
        if (frSpsPos != string::npos)
            rez.streamDescr = rez.streamDescr.substr(0, frSpsPos);
        if (m_vps)
            rez.streamDescr += string(" ") + m_vps->getDescription();
    }

    return rez;
}

int VVCStreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    if (m_firstFrame)
        CheckStreamRez rez = checkStream(m_buffer, m_bufEnd - m_buffer);

    if (hdmvDescriptors)
    {
        // 'HDMV' registration descriptor
        *dstBuff++ = 0x05;
        *dstBuff++ = 8;
        memcpy(dstBuff, "HDMV\xff\x24", 6);
        dstBuff += 6;

        int video_format, frame_rate_index, aspect_ratio_index;
        M2TSStreamInfo::blurayStreamParams(getFPS(), getInterlaced(), getStreamWidth(), getStreamHeight(),
                                           getStreamAR(), &video_format, &frame_rate_index, &aspect_ratio_index);

        *dstBuff++ = (video_format << 4) + frame_rate_index;
        *dstBuff++ = (aspect_ratio_index << 4) + 0xf;
    }
    else
    {
        uint8_t tmpBuffer[512];

        for (uint8_t* nal = NALUnit::findNextNAL(m_buffer, m_bufEnd); nal < m_bufEnd - 4;
             nal = NALUnit::findNextNAL(nal, m_bufEnd))
        {
            uint8_t nalType = (*nal >> 1) & 0x3f;
            uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, m_bufEnd, true);

            if (nalType == V_SPS)
            {
                int toDecode = FFMIN(sizeof(tmpBuffer) - 8, nextNal - nal);
                int decodedLen = NALUnit::decodeNAL(nal, nal + toDecode, tmpBuffer, sizeof(tmpBuffer));
                break;
            }
        }

        *dstBuff++ = VVC_DESCRIPTOR_TAG;
        *dstBuff++ = 13;  // descriptor length
        memcpy(dstBuff, tmpBuffer + 3, 12);
        dstBuff += 12;
        // flags temporal_layer_subset, VVC_still_present,
        // VVC_24hr_picture_present, HDR_WCG unspecified
        *dstBuff = 0x0f;
        dstBuff++;

        /* VVC_timing_and_HRD_descriptor
        // mandatory for interlaced video only
        memcpy(dstBuff, "\x3f\x0f\x03\x7f\x7f", 5);
        dstBuff += 5;

        uint32_t N = 1001 * getFPS();
        uint32_t K = 27000000;
        uint32_t num_units_in_tick = 1001;
        if (N % 1000)
        {
            N = 1000 * getFPS();
            num_units_in_tick = 1000;
        }
        N = my_htonl(N);
        K = my_htonl(K);
        num_units_in_tick = my_htonl(num_units_in_tick);
        memcpy(dstBuff, &N, 4);
        dstBuff += 4;
        memcpy(dstBuff, &K, 4);
        dstBuff += 4;
        memcpy(dstBuff, &num_units_in_tick, 4);
        dstBuff += 4;
        */
    }

    return (hdmvDescriptors ? 10 : 15);
}

void VVCStreamReader::updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int)
{
    int oldNalSize = nextNal - buff;
    m_vpsSizeDiff = 0;
    VvcVpsUnit* vps = (VvcVpsUnit*)nalUnit;
    vps->setFPS(m_fps);
    uint8_t* tmpBuffer = new uint8_t[vps->nalBufferLen() + 16];
    long newSpsLen = vps->serializeBuffer(tmpBuffer, tmpBuffer + vps->nalBufferLen() + 16);
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
    if (fps == 0 && m_sps)
        fps = m_sps->getFPS();
    return fps;
}

bool VVCStreamReader::isSlice(int nalType) const
{
    if (!m_sps || !m_pps)
        return false;
    return (nalType >= V_TRAIL && nalType <= V_RASL) || (nalType >= V_IDR_W_RADL && nalType <= V_GDR);
}

bool VVCStreamReader::isSuffix(int nalType) const
{
    if (!m_sps || !m_vps || !m_pps)
        return false;
    return (nalType == V_FD || nalType == V_SUFFIX_APS);
}

void VVCStreamReader::incTimings()
{
    if (m_totalFrameNum++ > 0)
        m_curDts += m_pcrIncPerFrame;
    int delta = m_frameNum - m_fullPicOrder;
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
        int range = 1 << pic_bits;

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
        dst.resize(dataEnd - data);
        memcpy(dst.data(), data, dataEnd - data);
    }
}

int VVCStreamReader::intDecodeNAL(uint8_t* buff)
{
    int rez = 0;
    bool sliceFound = false;
    m_spsPpsFound = false;
    m_lastIFrame = false;

    uint8_t* prevPos = 0;
    uint8_t* curPos = buff;
    uint8_t* nextNal = NALUnit::findNextNAL(curPos, m_bufEnd);
    uint8_t* nextNalWithStartCode;
    long oldSpsLen = 0;

    if (!m_eof && nextNal == m_bufEnd)
        return NOT_ENOUGH_BUFFER;

    while (curPos < m_bufEnd)
    {
        int nalType = (*curPos >> 1) & 0x3f;
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
                    VvcSliceHeader slice;
                    slice.decodeBuffer(curPos, FFMIN(curPos + MAX_SLICE_HEADER, nextNal));
                    rez = slice.deserialize(m_sps, m_pps);
                    if (rez)
                        return rez;  // not enough buffer or error
                    // if (slice.slice_type == VVC_IFRAME_SLICE)
                    if (nalType >= NAL_BLA_W_LP)
                        m_lastIFrame = true;
                    m_fullPicOrder = toFullPicOrder(&slice, m_sps->log2_max_pic_order_cnt_lsb);
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

            nextNalWithStartCode = nextNal[-4] == 0 ? nextNal - 4 : nextNal - 3;

            switch (nalType)
            {
            case NAL_VPS:
                if (!m_vps)
                    m_vps = new VvcVpsUnit();
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
            case NAL_SPS:
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
            case NAL_PPS:
                if (!m_pps)
                    m_pps = new VvcPpsUnit();
                m_pps->decodeBuffer(curPos, nextNalWithStartCode);
                rez = m_pps->deserialize();
                if (rez)
                    return rez;
                m_spsPpsFound = true;
                storeBuffer(m_ppsBuffer, curPos, nextNalWithStartCode);
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
    int bytesLeft = dstEnd - dstBuffer;
    int requiredBytes = srcData.size() + 3 + (m_shortStartCodes ? 0 : 1);
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
        int offset = avPacket.data[2] == 1 ? 3 : 4;
        uint8_t nalType = (avPacket.data[offset] >> 1) & 0x3f;
        if (nalType == NAL_AUD)
        {
            // place delimiter at first place
            memcpy(curPos, avPacket.data, avPacket.size);
            curPos += avPacket.size;
            avPacket.size = 0;
            avPacket.data = 0;
        }
    }

    bool needInsSpsPps = m_firstFileFrame && !(avPacket.flags & AVPacket::IS_SPS_PPS_IN_GOP);
    if (needInsSpsPps)
    {
        avPacket.flags |= AVPacket::IS_SPS_PPS_IN_GOP;

        curPos = writeBuffer(m_vpsBuffer, curPos, dstEnd);
        curPos = writeBuffer(m_spsBuffer, curPos, dstEnd);
        curPos = writeBuffer(m_ppsBuffer, curPos, dstEnd);
    }

    m_firstFileFrame = false;
    return curPos - dstBuffer;
}
