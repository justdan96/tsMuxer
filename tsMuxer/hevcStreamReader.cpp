#include "hevcStreamReader.h"

#include <fs/systemlog.h>

#include "hevc.h"
#include "nalUnits.h"
#include "tsMuxer.h"
#include "tsPacket.h"
#include "vodCoreException.h"

using namespace std;

static const int MAX_SLICE_HEADER = 64;

HEVCStreamReader::HEVCStreamReader()
    : MPEGStreamReader(),
      m_vps(0),
      m_sps(0),
      m_pps(0),
      m_hdr(new HevcHdrUnit()),
      m_slice(0),
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

HEVCStreamReader::~HEVCStreamReader()
{
    delete m_vps;
    delete m_sps;
    delete m_pps;
    delete m_hdr;
    delete m_slice;
}

CheckStreamRez HEVCStreamReader::checkStream(uint8_t* buffer, int len)
{
    CheckStreamRez rez;

    uint8_t* end = buffer + len;
    for (uint8_t* nal = NALUnit::findNextNAL(buffer, end); nal < end - 4; nal = NALUnit::findNextNAL(nal, end))
    {
        if (*nal & 0x80)
            return rez;  // invalid nal
        auto nalType = (HevcUnit::NalType)((*nal >> 1) & 0x3f);
        uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, end, true);

        switch (nalType)
        {
        case HevcUnit::NalType::VPS:
            if (!m_vps)
                m_vps = new HevcVpsUnit();
            m_vps->decodeBuffer(nal, nextNal);
            if (m_vps->deserialize())
            {
                delete m_vps;
                return rez;
            }
            m_spsPpsFound = true;
            if (m_vps->num_units_in_tick)
                updateFPS(m_vps, nal, nextNal, 0);
            break;
        case HevcUnit::NalType::SPS:
            if (!m_sps)
                m_sps = new HevcSpsUnit();
            m_sps->decodeBuffer(nal, nextNal);
            if (m_sps->deserialize() != 0)
                return rez;
            m_spsPpsFound = true;
            updateFPS(m_sps, nal, nextNal, 0);
            break;
        case HevcUnit::NalType::PPS:
            if (!m_pps)
                m_pps = new HevcPpsUnit();
            m_pps->decodeBuffer(nal, nextNal);
            if (m_pps->deserialize() != 0)
                return rez;
            break;
        case HevcUnit::NalType::SEI_PREFIX:
            m_hdr->decodeBuffer(nal, nextNal);
            if (m_hdr->deserialize() != 0)
                return rez;
            break;
        case HevcUnit::NalType::DVRPU:
        case HevcUnit::NalType::DVEL:
            if (nal[1] == 1)
            {
                if (nalType == HevcUnit::NalType::DVEL)
                    m_hdr->isDVEL = true;
                else
                    m_hdr->isDVRPU = true;
                V3_flags |= DV;
            }
            break;
        default:
            break;
        }

        // check Frame Depth on first slices
        if (isSlice(nalType) && (nal[2] & 0x80))
        {
            if (!m_slice)
                m_slice = new HevcSliceHeader();
            m_slice->decodeBuffer(nal, FFMIN(nal + MAX_SLICE_HEADER, nextNal));
            if (m_slice->deserialize(m_sps, m_pps))
                return rez;  // not enough buffer or error
            m_fullPicOrder = toFullPicOrder(m_slice, m_sps->log2_max_pic_order_cnt_lsb);
            incTimings();
        }
    }
    m_totalFrameNum = m_frameNum = m_fullPicOrder = 0;
    m_curDts = m_curPts = 0;

    // Set HDR10 flag if PQ detected
    if (m_vps && m_sps && m_pps && m_sps->vps_id == m_vps->vps_id && m_pps->sps_id == m_sps->sps_id)
    {
        if (m_sps->colour_primaries == 9 && m_sps->transfer_characteristics == 16 &&
            m_sps->matrix_coeffs == 9)  // SMPTE.ST.2084 (PQ)
        {
            m_hdr->isHDR10 = true;
            V3_flags |= HDR10;
        }

        rez.codecInfo = hevcCodecInfo;
        rez.streamDescr = m_sps->getDescription();
        size_t frSpsPos = rez.streamDescr.find("Frame rate: not found");
        if (frSpsPos != string::npos)
            rez.streamDescr = rez.streamDescr.substr(0, frSpsPos) + string(" ") + m_vps->getDescription();
    }

    return rez;
}

int HEVCStreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    if (m_firstFrame)
        CheckStreamRez rez = checkStream(m_buffer, (int)(m_bufEnd - m_buffer));

    int lenDoviDesc = 0;
    if (!blurayMode && m_hdr->isDVRPU)
    {
        // 'DOVI' registration descriptor
        *dstBuff++ = (uint8_t)TSDescriptorTag::REGISTRATION;
        *dstBuff++ = 4;  // descriptor length
        *dstBuff++ = 'D';
        *dstBuff++ = 'O';
        *dstBuff++ = 'V';
        *dstBuff++ = 'I';
        lenDoviDesc += 6;
    }

    if (hdmvDescriptors)
    {
        // 'HDMV' registration descriptor
        *dstBuff++ = (uint8_t)TSDescriptorTag::HDMV;  // descriptor tag
        *dstBuff++ = 8;                               // descriptor length
        memcpy(dstBuff, "HDMV\xff", 5);               // HDMV + stuffing byte
        dstBuff += 5;

        *dstBuff++ = (uint8_t)StreamType::VIDEO_H265;  // stream_conding_type
        int video_format, frame_rate_index, aspect_ratio_index;
        M2TSStreamInfo::blurayStreamParams(getFPS(), getInterlaced(), getStreamWidth(), getStreamHeight(),
                                           (int)getStreamAR(), &video_format, &frame_rate_index, &aspect_ratio_index);

        *dstBuff++ = (video_format << 4) + frame_rate_index;
        *dstBuff++ = (aspect_ratio_index << 4) + 0xf;
    }
    else
    {
        uint8_t tmpBuffer[512];

        for (uint8_t* nal = NALUnit::findNextNAL(m_buffer, m_bufEnd); nal < m_bufEnd - 4;
             nal = NALUnit::findNextNAL(nal, m_bufEnd))
        {
            auto nalType = (HevcUnit::NalType)((*nal >> 1) & 0x3f);
            uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, m_bufEnd, true);

            if (nalType == HevcUnit::NalType::SPS)
            {
                int toDecode = FFMIN(sizeof(tmpBuffer) - 8, (unsigned)(nextNal - nal));
                int decodedLen = NALUnit::decodeNAL(nal, nal + toDecode, tmpBuffer, sizeof(tmpBuffer));
                break;
            }
        }

        *dstBuff++ = (int)TSDescriptorTag::HEVC;
        *dstBuff++ = 13;  // descriptor length
        memcpy(dstBuff, tmpBuffer + 3, 12);
        dstBuff += 12;
        // flags temporal_layer_subset, HEVC_still_present,
        // HEVC_24hr_picture_present, HDR_WCG unspecified
        *dstBuff = 0x0f;

        if (!m_sps->sub_pic_hrd_params_present_flag)
            *dstBuff |= 0x10;
        dstBuff++;

        /* HEVC_timing_and_HRD_descriptor
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

    if (!blurayMode && m_hdr->isDVRPU)
        lenDoviDesc += setDoViDescriptor(dstBuff);

    return (hdmvDescriptors ? 10 : 15) + lenDoviDesc;
}

int HEVCStreamReader::setDoViDescriptor(uint8_t* dstBuff)
{
    bool isDVBL = (V3_flags & NON_DV_TRACK) == 0;
    if (!isDVBL)
        m_hdr->isDVEL = true;

    int width = getStreamWidth();
    uint32_t pixelRate = (uint32_t)(width * getStreamHeight() * getFPS());

    if (!isDVBL && V3_flags & FOUR_K)
    {
        width *= 2;
        pixelRate *= 4;
    }

    // cf. "http://www.dolby.com/us/en/technologies/dolby-vision/dolby-vision-profiles-levels.pdf"
    // "For profiles 7, 8.1 and 8.4, VUI parameters are required, as bitstreams employing these profiles
    // have a non-SDR base layer. For other Dolby Vision profiles, VUI parameters are optional."
    int profile;
    int compatibility;

    if (m_sps->bit_depth_luma_minus8 == 2)  // 10-bit
    {
        if (m_hdr->isDVEL)
        {
            if (m_sps->transfer_characteristics == 16)  // PQ
            {
                if (m_sps->chroma_sample_loc_type_top_field == 2)  // Blu-ray
                {
                    profile = 7;
                    compatibility = 6;
                }
                else  // CTA HDR10
                {
                    profile = 6;
                    compatibility = 1;
                }
            }
            else  // unspecified, assumed DV IPT
            {
                profile = 4;
                compatibility = 2;
            }
        }
        else  // single BL layer
        {
            switch (m_sps->transfer_characteristics)
            {
            case 16:  // PQ
                profile = 8;
                compatibility = 1;
                break;
            case 14:  // HLG-DVB
            case 18:  // HLG-ARIB
                profile = 8;
                compatibility = 4;
                break;
            case 1:  // SDR
                profile = 8;
                compatibility = 2;
                break;
            default:  // unspecified, assumed DV IPT
                profile = 5;
                compatibility = 0;
            }
        }
    }
    else  // 8-bit
    {
        if (m_sps->transfer_characteristics == 1)  // SDR
        {
            profile = 2;
            compatibility = 2;
        }
        else  // unspecified, assumed DV IPT
        {
            profile = 3;
            compatibility = 0;
        }
    }

    int level = 0;
    if (width <= 1280 && pixelRate <= 22118400)
        level = 1;
    else if (width <= 1280 && pixelRate <= 27648000)
        level = 2;
    else if (width <= 1920 && pixelRate <= 49766400)
        level = 3;
    else if (width <= 2560 && pixelRate <= 62208000)
        level = 4;
    else if (width <= 3840 && pixelRate <= 124416000)
        level = 5;
    else if (width <= 3840 && pixelRate <= 199065600)
        level = 6;
    else if (width <= 3840 && pixelRate <= 248832000)
        level = 7;
    else if (width <= 3840 && pixelRate <= 398131200)
        level = 8;
    else if (width <= 3840 && pixelRate <= 497664000)
        level = 9;
    else if (width <= 3840 && pixelRate <= 995328000)
        level = 10;
    else if (width <= 7680 && pixelRate <= 995328000)
        level = 11;
    else if (width <= 7680 && pixelRate <= 1990656000)
        level = 12;
    else if (width <= 7680 && pixelRate <= 3981312000)
        level = 13;

    BitStreamWriter bitWriter{};
    bitWriter.setBuffer(dstBuff, dstBuff + 128);

    bitWriter.putBits(8, 0xb0);            // DoVi descriptor tag
    bitWriter.putBits(8, isDVBL ? 5 : 7);  // descriptor length
    bitWriter.putBits(8, 1);               // dv version major
    bitWriter.putBits(8, 0);               // dv version minor
    bitWriter.putBits(7, profile);         // dv profile
    bitWriter.putBits(6, level);           // dv level
    bitWriter.putBits(1, m_hdr->isDVRPU);  // rpu_present_flag
    bitWriter.putBits(1, m_hdr->isDVEL);   // el_present_flag
    bitWriter.putBits(1, isDVBL);          // bl_present_flag
    if (!isDVBL)
    {
        bitWriter.putBits(13, 0x1011);  // dependency_pid
        bitWriter.putBits(3, 7);        // reserved
    }
    bitWriter.putBits(4, compatibility);  // dv_bl_sigHevcUnit::NalType::compatibility_id
    bitWriter.putBits(4, 15);             // reserved

    bitWriter.flushBits();
    return isDVBL ? 7 : 9;
}

void HEVCStreamReader::updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int)
{
    int oldNalSize = (int)(nextNal - buff);
    m_vpsSizeDiff = 0;
    HevcVpsUnit* vps = (HevcVpsUnit*)nalUnit;
    vps->setFPS(m_fps);
    uint8_t* tmpBuffer = new uint8_t[vps->nalBufferLen() + 16];
    int newSpsLen = vps->serializeBuffer(tmpBuffer, tmpBuffer + vps->nalBufferLen() + 16);
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

int HEVCStreamReader::getStreamWidth() const { return m_sps ? m_sps->pic_width_in_luma_samples : 0; }

int HEVCStreamReader::getStreamHeight() const { return m_sps ? m_sps->pic_height_in_luma_samples : 0; }

int HEVCStreamReader::getStreamHDR() const
{
    return (m_hdr->isDVRPU || m_hdr->isDVEL) ? 4 : (m_hdr->isHDR10plus ? 16 : (m_hdr->isHDR10 ? 2 : 1));
}

double HEVCStreamReader::getStreamFPS(void* curNalUnit)
{
    double fps = 0;
    if (m_vps)
        fps = m_vps->getFPS();
    if (fps == 0 && m_sps)
        fps = m_sps->getFPS();
    return fps;
}

bool HEVCStreamReader::isSlice(HevcUnit::NalType nalType) const
{
    if (!m_sps || !m_vps || !m_pps)
        return false;
    return (nalType >= HevcUnit::NalType::TRAIL_N && nalType <= HevcUnit::NalType::RASL_R) ||
           (nalType >= HevcUnit::NalType::BLA_W_LP && nalType <= HevcUnit::NalType::RSV_IRAP_VCL23);
}

bool HEVCStreamReader::isSuffix(HevcUnit::NalType nalType) const
{
    if (!m_sps || !m_vps || !m_pps)
        return false;
    return (nalType == HevcUnit::NalType::FD || nalType == HevcUnit::NalType::SEI_SUFFIX ||
            nalType == HevcUnit::NalType::RSV_NVCL45 ||
            (nalType >= HevcUnit::NalType::RSV_NVCL45 && nalType <= HevcUnit::NalType::RSV_NVCL47) ||
            (nalType >= HevcUnit::NalType::UNSPEC56 && nalType <= HevcUnit::NalType::DVEL));
}

void HEVCStreamReader::incTimings()
{
    if (m_totalFrameNum++ > 0)
        m_curDts += m_pcrIncPerFrame;
    int delta = m_frameNum - m_fullPicOrder;
    m_curPts = m_curDts - delta * m_pcrIncPerFrame;
    m_frameNum++;
    m_firstFrame = false;

    if (delta > m_frameDepth)
    {
        m_frameDepth = FFMIN(4, delta);
        LTRACE(LT_INFO, 2,
               "B-pyramid level " << m_frameDepth - 1 << " detected. Shift DTS to " << m_frameDepth << " frames");
    }
}

int HEVCStreamReader::toFullPicOrder(HevcSliceHeader* slice, int pic_bits)
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

void HEVCStreamReader::storeBuffer(MemoryBlock& dst, const uint8_t* data, const uint8_t* dataEnd)
{
    dataEnd--;
    while (dataEnd > data && dataEnd[-1] == 0) dataEnd--;
    if (dataEnd > data)
    {
        dst.resize((int)(dataEnd - data));
        memcpy(dst.data(), data, dataEnd - data);
    }
}

int HEVCStreamReader::intDecodeNAL(uint8_t* buff)
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
        auto nalType = (HevcUnit::NalType)((*curPos >> 1) & 0x3f);
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
                    if (!m_slice)
                        m_slice = new HevcSliceHeader();
                    m_slice->decodeBuffer(curPos, FFMIN(curPos + MAX_SLICE_HEADER, nextNal));
                    rez = m_slice->deserialize(m_sps, m_pps);
                    if (rez)
                        return rez;  // not enough buffer or error
                    if (nalType >= HevcUnit::NalType::BLA_W_LP)
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

            nextNalWithStartCode = nextNal[-4] == 0 ? nextNal - 4 : nextNal - 3;

            switch (nalType)
            {
            case HevcUnit::NalType::VPS:
                if (!m_vps)
                    m_vps = new HevcVpsUnit();
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
            case HevcUnit::NalType::SPS:
                if (!m_sps)
                    m_sps = new HevcSpsUnit();
                m_sps->decodeBuffer(curPos, nextNalWithStartCode);
                rez = m_sps->deserialize();
                if (rez)
                    return rez;
                m_spsPpsFound = true;
                updateFPS(m_sps, curPos, nextNalWithStartCode, 0);
                storeBuffer(m_spsBuffer, curPos, nextNalWithStartCode);
                break;
            case HevcUnit::NalType::PPS:
                if (!m_pps)
                    m_pps = new HevcPpsUnit();
                m_pps->decodeBuffer(curPos, nextNalWithStartCode);
                rez = m_pps->deserialize();
                if (rez)
                    return rez;
                m_spsPpsFound = true;
                storeBuffer(m_ppsBuffer, curPos, nextNalWithStartCode);
                break;
            case HevcUnit::NalType::SEI_PREFIX:
                m_hdr->decodeBuffer(curPos, nextNal);
                if (m_hdr->deserialize() != 0)
                    return rez;
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

uint8_t* HEVCStreamReader::writeNalPrefix(uint8_t* curPos)
{
    if (!m_shortStartCodes)
        *curPos++ = 0;
    *curPos++ = 0;
    *curPos++ = 0;
    *curPos++ = 1;
    return curPos;
}

uint8_t* HEVCStreamReader::writeBuffer(MemoryBlock& srcData, uint8_t* dstBuffer, uint8_t* dstEnd)
{
    if (srcData.isEmpty())
        return dstBuffer;
    int bytesLeft = (int)(dstEnd - dstBuffer);
    int requiredBytes = (int)srcData.size() + 3 + (m_shortStartCodes ? 0 : 1);
    if (bytesLeft < requiredBytes)
        return dstBuffer;

    dstBuffer = writeNalPrefix(dstBuffer);
    memcpy(dstBuffer, srcData.data(), srcData.size());
    dstBuffer += srcData.size();
    return dstBuffer;
}

int HEVCStreamReader::writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                                        PriorityDataInfo* priorityData)
{
    uint8_t* curPos = dstBuffer;

    if (avPacket.size > 4 && avPacket.size < dstEnd - dstBuffer)
    {
        int offset = avPacket.data[2] == 1 ? 3 : 4;
        auto nalType = (HevcUnit::NalType)((avPacket.data[offset] >> 1) & 0x3f);
        if (nalType == HevcUnit::NalType::AUD)
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
    return (int)(curPos - dstBuffer);
}
