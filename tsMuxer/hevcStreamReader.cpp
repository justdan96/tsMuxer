#include "hevcStreamReader.h"
#include "nalUnits.h"
#include <fs/systemlog.h>
#include "vodCoreException.h"
#include "hevc.h"

using namespace std;

static const int MAX_SLICE_HEADER = 64;
int HDR10_metadata[7] = { 1,0,0,0,0,0,0 };

HEVCStreamReader::HEVCStreamReader(): 
    MPEGStreamReader(),
    m_vps(0),
    m_sps(0),
    m_pps(0),
	m_sei(0),
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
    delete m_sei;
}


CheckStreamRez HEVCStreamReader::checkStream(uint8_t* buffer, int len)
{
    CheckStreamRez rez;

    uint8_t* end = buffer + len;
    for (uint8_t* nal = NALUnit::findNextNAL(buffer, end); nal < end - 4; nal = NALUnit::findNextNAL(nal, end))
    {
        if (*nal & 0x80)
            return rez; // invalid nal
        int nalType = (*nal >> 1) & 0x3f;
        switch (nalType)
        {
            case NAL_VPS: {
                uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, end, true);
                if (!m_vps)
                    m_vps = new HevcVpsUnit();
                m_vps->decodeBuffer(nal, nextNal);
                if (m_vps->deserialize() != 0)
                    return rez;
                break;
            }
            case NAL_SPS: {
                uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, end, true);
                if (!m_sps)
                    m_sps = new HevcSpsUnit();
                m_sps->decodeBuffer(nal, nextNal);
                if (m_sps->deserialize() != 0)
                    return rez;
                break;
            }
            case NAL_PPS: {
                uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, end, true);
                if (!m_pps)
                    m_pps = new HevcPpsUnit();
                m_pps->decodeBuffer(nal, nextNal);
                if (m_pps->deserialize() != 0)
                    return rez;
                break;
            }
            case NAL_SEI_PREFIX: {
                uint8_t* nextNal = NALUnit::findNALWithStartCode(nal, end, true);
                if (!m_sei)
                    m_sei = new HevcSeiUnit();
                m_sei->decodeBuffer(nal, nextNal);
                if (m_sei->deserialize() != 0)
                    return rez;
                break;
            }
            case NAL_DV: {
                if (!m_sei)
                    m_sei = new HevcSeiUnit();
                if (nal[1] == 1 && !m_sei->isDV) {
                    m_sei->isDV = true;
                    *HDR10_metadata |= 4; // Dolby Vision flag
                }
                break;
            }
        }
    }

    if (m_vps && m_sps && m_pps && m_sps->vps_id == m_vps->vps_id && m_pps->sps_id == m_sps->sps_id)
    {
        rez.codecInfo = hevcCodecInfo;
        rez.streamDescr = m_sps->getDescription() + string(" ") + m_vps->getDescription();
    }

    return rez;
}

int HEVCStreamReader::getTSDescriptor(uint8_t* dstBuff)
{
    if (m_firstFrame) {
        checkStream(m_buffer, m_bufEnd - m_buffer);
    }

    *dstBuff++ = 0x05; // registreation descriptor tag
    *dstBuff++ = 4;
    memcpy(dstBuff, "HEVC", 4);
    return 6;
}

void HEVCStreamReader::updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int )
{
    int oldNalSize = nextNal - buff;
    m_vpsSizeDiff = 0;
    HevcVpsUnit* vps = (HevcVpsUnit*) nalUnit;
    vps->setFPS(m_fps);
    uint8_t* tmpBuffer = new uint8_t[vps->nalBufferLen() + 16];
    long newSpsLen = vps->serializeBuffer(tmpBuffer, tmpBuffer + vps->nalBufferLen() + 16);
    if (newSpsLen == -1) 
        THROW(ERR_COMMON, "Not enough buffer");
    
    if (newSpsLen != oldNalSize) 
    {
        m_vpsSizeDiff = newSpsLen - oldNalSize;
        if (m_bufEnd + m_vpsSizeDiff > m_tmpBuffer + TMP_BUFFER_SIZE)
            THROW(ERR_COMMON, "Not enough buffer");
        memmove(nextNal + m_vpsSizeDiff, nextNal, m_bufEnd - nextNal);
        m_bufEnd += m_vpsSizeDiff;
    }
    memcpy(buff, tmpBuffer, newSpsLen);
    
    delete [] tmpBuffer;
}

int HEVCStreamReader::getStreamWidth() const  
{
    return m_sps ? m_sps->pic_width_in_luma_samples : 0;
}

int HEVCStreamReader::getStreamHeight() const  
{
    return m_sps ? m_sps->pic_height_in_luma_samples : 0;
}

int HEVCStreamReader::getStreamHDR() const
{
    return m_sei->isDV ? 4 : (m_sei->isHDR10plus ? 16 : (m_sei->isHDR10 ? 2 : 1));
}

double HEVCStreamReader::getStreamFPS(void * curNalUnit)
{
    HevcVpsUnit* vps = (HevcVpsUnit*) curNalUnit;
    return vps->getFPS();
}

bool HEVCStreamReader::isSlice(int nalType) const
{
    if (!m_sps || !m_vps || !m_pps)
        return false;
    return (nalType >= NAL_TRAIL_N && nalType <= NAL_RASL_R) || (nalType >= NAL_BLA_W_LP && nalType <= NAL_RSV_IRAP_VCL23);
}

void HEVCStreamReader::incTimings()
{
    if (m_totalFrameNum++ > 0)
        m_curDts += m_pcrIncPerFrame;
    int delta = m_frameNum - m_fullPicOrder;
    m_curPts = m_curDts - delta * m_pcrIncPerFrame;
    m_frameNum++;
    m_firstFrame = false;

    if (delta > m_frameDepth) {
        m_frameDepth = FFMIN(4, delta);
        LTRACE(LT_INFO, 2, "B-pyramid level " << m_frameDepth-1 << " detected. Shift DTS to " << m_frameDepth << " frames");
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
    else {
        int range = 1 << pic_bits;

        if (slice->pic_order_cnt_lsb < m_prevPicOrder && m_prevPicOrder - slice->pic_order_cnt_lsb >= range/2)
            m_picOrderMsb += range;
        else if (slice->pic_order_cnt_lsb > m_prevPicOrder && slice->pic_order_cnt_lsb - m_prevPicOrder  >= range/2)
            m_picOrderMsb -= range;

        m_prevPicOrder = slice->pic_order_cnt_lsb;
    }

    return slice->pic_order_cnt_lsb + m_picOrderMsb + m_picOrderBase;
}

void HEVCStreamReader::storeBuffer(MemoryBlock& dst, const uint8_t* data, const uint8_t* dataEnd)
{
    dataEnd--;
    while (dataEnd > data && dataEnd[-1] == 0)
        dataEnd--;
    if (dataEnd > data) {
        dst.resize(dataEnd - data);
        memcpy(dst.data(), data, dataEnd - data);
    }
}

int HEVCStreamReader::intDecodeNAL(uint8_t* buff)
{
    int rez = 0;
    bool sliceFound = false;
    m_spsPpsFound   = false;
    m_lastIFrame = false;

    uint8_t* prevPos = 0;
    uint8_t* curPos = buff;
    uint8_t* nextNal = NALUnit::findNextNAL(curPos, m_bufEnd);
    uint8_t* nextNalWithStartCode;
    long oldSpsLen  = 0;

    while (curPos < m_bufEnd)
    {
        int nalType = (*curPos >> 1) & 0x3f;
        if (isSlice(nalType)) 
        {
            if (curPos >= m_bufEnd-2)
                return NOT_ENOUGH_BUFFER;

            if (curPos[2] & 0x80) // slice.first_slice
            {
                if (sliceFound ) {
                    m_lastDecodedPos = prevPos; // next frame started
                    incTimings();
                    return 0;
                }
                else 
                {
                    HevcSliceHeader slice;
                    slice.decodeBuffer(curPos, FFMIN(curPos + MAX_SLICE_HEADER, nextNal));
                    rez = slice.deserialize(m_sps, m_pps);
                    if (rez)
                        return rez; // not enough buffer or error
                    //if (slice.slice_type == HEVC_IFRAME_SLICE)
                    if (nalType >= NAL_BLA_W_LP)
                        m_lastIFrame = true;
                    m_fullPicOrder = toFullPicOrder(&slice, m_sps->log2_max_pic_order_cnt_lsb);
                }
            }
            sliceFound = true;
        }
        else {
            if (sliceFound) {
                incTimings();
                m_lastDecodedPos = prevPos;  // next frame started
                return 0;
            }

            switch(nalType) 
            {
                case NAL_VPS: 
                    if (nextNal == m_bufEnd)
                        return NOT_ENOUGH_BUFFER;
                    nextNalWithStartCode = nextNal[-4] == 0 ? nextNal - 4 : nextNal - 3;

                    if (!m_vps)
                        m_vps = new HevcVpsUnit();

                    m_vps->decodeBuffer(curPos, nextNalWithStartCode);
                    rez = m_vps->deserialize();
                    if (rez)
                        return rez;
                    m_spsPpsFound = true;
                    m_vpsCounter++;
                    m_vpsSizeDiff = 0;
                    updateFPS(m_vps, curPos, nextNalWithStartCode, 0);
                    nextNal += m_vpsSizeDiff;
                    storeBuffer(m_vpsBuffer, curPos, nextNalWithStartCode);
                    break;
                case NAL_SPS:
                    if (nextNal == m_bufEnd)
                        return NOT_ENOUGH_BUFFER;
                    nextNalWithStartCode = nextNal[-4] == 0 ? nextNal - 4 : nextNal - 3;

                    if (!m_sps)
                        m_sps = new HevcSpsUnit();
                    m_sps->decodeBuffer(curPos, nextNalWithStartCode);
                    rez = m_sps->deserialize();
                    if (rez)
                        return rez;
                    m_spsPpsFound = true;
                    storeBuffer(m_spsBuffer, curPos, nextNalWithStartCode);
                    break;
                case NAL_PPS:
                    if (nextNal == m_bufEnd)
                        return NOT_ENOUGH_BUFFER;
                    nextNalWithStartCode = nextNal[-4] == 0 ? nextNal - 4 : nextNal - 3;

                    if (!m_pps)
                        m_pps = new HevcPpsUnit();
                    m_pps->decodeBuffer(curPos, nextNalWithStartCode);
                    rez = m_pps->deserialize();
                    if (rez)
                        return rez;
                    m_spsPpsFound = true;
                    storeBuffer(m_ppsBuffer, curPos, nextNalWithStartCode);
                    break;
            }
        }
        prevPos = curPos;
        curPos = nextNal;
        nextNal = NALUnit::findNextNAL(curPos, m_bufEnd);
    }
    if (m_eof) {
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
    int bytesLeft = dstEnd - dstBuffer;
    int requiredBytes = srcData.size() + 3 + (m_shortStartCodes ? 0 : 1);
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

    bool needInsSpsPps = m_firstFileFrame && !(avPacket.flags &  AVPacket::IS_SPS_PPS_IN_GOP);
	if (needInsSpsPps)
	{
        avPacket.flags |=  AVPacket::IS_SPS_PPS_IN_GOP;

        curPos = writeBuffer(m_vpsBuffer, curPos, dstEnd);
        curPos = writeBuffer(m_spsBuffer, curPos, dstEnd);
        curPos = writeBuffer(m_ppsBuffer, curPos, dstEnd);
	}

    m_firstFileFrame = false;
	return curPos - dstBuffer;
}
