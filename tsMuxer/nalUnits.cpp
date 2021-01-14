
#include "nalUnits.h"
//#include <math.h>
#include <assert.h>
#include <fs/systemlog.h>

#include <sstream>

#include "bitStream.h"
#include "vod_common.h"

static const double FRAME_RATE_EPS = 3e-5;

uint8_t BDROM_METADATA_GUID[] = "\x17\xee\x8c\x60\xf8\x4d\x11\xd9\x8c\xd6\x08\x00\x20\x0c\x9a\x66";

void NALUnit::write_rbsp_trailing_bits(BitStreamWriter& writer)
{
    writer.putBit(1);
    int rest = 8 - (writer.getBitsCount() & 7);
    if (rest == 8)
        return;
    writer.putBits(rest, 0);
}

int NALUnit::calcNalLenInBits(const uint8_t* nalBuffer, const uint8_t* end)
{
    if (end > nalBuffer)
    {
        int trailing = 1;
        uint8_t data = end[-1] | 0x80;
        while ((data & 1) == 0)
        {
            data >>= 1;
            trailing++;
        }
        return (end - nalBuffer) * 8 - trailing;
    }
    else
    {
        return 0;
    }
}

void NALUnit::write_byte_align_bits(BitStreamWriter& writer)
{
    int rest = 8 - (writer.getBitsCount() & 7);
    if (rest == 8)
        return;
    writer.putBit(1);
    if (rest > 1)
        writer.putBits(rest - 1, 0);
}

uint8_t* NALUnit::addStartCode(uint8_t* buffer, uint8_t* boundStart)
{
    uint8_t* rez = buffer;
    if (rez - 3 >= boundStart && rez[-1] == 1 && rez[-2] == 0 && rez[-3] == 0)
    {
        rez -= 3;
        if (rez > boundStart && rez[-1] == 0)
            rez--;
    }
    return rez;
}

uint8_t* NALUnit::findNextNAL(uint8_t* buffer, uint8_t* end)
{
    for (buffer += 2; buffer < end;)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer++;
        else if (buffer[-2] == 0 && buffer[-1] == 0)
            return buffer + 1;
        else
            buffer += 3;
    }
    return end;
}

uint8_t* NALUnit::findNALWithStartCode(const uint8_t* buffer, const uint8_t* end, bool longCodesAllowed)
{
    const uint8_t* bufStart = buffer;
    for (buffer += 2; buffer < end;)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer++;
        else if (buffer[-2] == 0 && buffer[-1] == 0)
        {
            if (longCodesAllowed && buffer - 3 >= bufStart && buffer[-3] == 0)
                return (uint8_t*)buffer - 3;
            else
                return (uint8_t*)buffer - 2;
        }
        else
            buffer += 3;
    }
    return (uint8_t*)end;
}

int NALUnit::encodeNAL(uint8_t* srcBuffer, uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize)
{
    uint8_t* srcStart = srcBuffer;
    uint8_t* initDstBuffer = dstBuffer;
    for (srcBuffer += 2; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 3;
        else if (srcBuffer[-2] == 0 && srcBuffer[-1] == 0)
        {
            if (dstBufferSize < (size_t)(srcBuffer - srcStart + 2))
                return -1;
            memcpy(dstBuffer, srcStart, srcBuffer - srcStart);
            dstBuffer += srcBuffer - srcStart;
            dstBufferSize -= srcBuffer - srcStart + 2;
            *dstBuffer++ = 3;
            *dstBuffer++ = *srcBuffer++;

            if (srcBuffer < srcEnd)
            {
                if (dstBufferSize < 1)
                    return -1;
                *dstBuffer++ = *srcBuffer++;
                dstBufferSize--;
            }
            srcStart = srcBuffer;
        }
        else
            srcBuffer++;
    }
    if (dstBufferSize < (size_t)(srcEnd - srcStart))
        return -1;
    memcpy(dstBuffer, srcStart, srcEnd - srcStart);
    dstBuffer += srcEnd - srcStart;
    return dstBuffer - initDstBuffer;
}

int NALUnit::decodeNAL(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize)
{
    uint8_t* initDstBuffer = dstBuffer;
    const uint8_t* srcStart = srcBuffer;
    for (srcBuffer += 3; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 4;
        else if (srcBuffer[-3] == 0 && srcBuffer[-2] == 0 && srcBuffer[-1] == 3)
        {
            if (dstBufferSize < (size_t)(srcBuffer - srcStart))
                return -1;
            memcpy(dstBuffer, srcStart, srcBuffer - srcStart - 1);
            dstBuffer += srcBuffer - srcStart - 1;
            dstBufferSize -= srcBuffer - srcStart;
            *dstBuffer++ = *srcBuffer++;
            srcStart = srcBuffer;
        }
        else
            srcBuffer++;
    }
    memcpy(dstBuffer, srcStart, srcEnd - srcStart);
    dstBuffer += srcEnd - srcStart;
    return dstBuffer - initDstBuffer;
}

int NALUnit::decodeNAL2(uint8_t* srcBuffer, uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize,
                        bool* keepSrcBuffer)
{
    uint8_t* initDstBuffer = dstBuffer;
    uint8_t* srcStart = srcBuffer;
    *keepSrcBuffer = true;
    for (srcBuffer += 3; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 4;
        else if (srcBuffer[-3] == 0 && srcBuffer[-2] == 0 && srcBuffer[-1] == 3)
        {
            if (dstBufferSize < (size_t)(srcBuffer - srcStart))
                return -1;
            memcpy(dstBuffer, srcStart, srcBuffer - srcStart - 1);
            dstBuffer += srcBuffer - srcStart - 1;
            dstBufferSize -= srcBuffer - srcStart;
            *dstBuffer++ = *srcBuffer++;
            srcStart = srcBuffer;
            *keepSrcBuffer = false;
        }
        else
            srcBuffer++;
    }
    if (!*keepSrcBuffer)
        memcpy(dstBuffer, srcStart, srcEnd - srcStart);
    dstBuffer += srcEnd - srcStart;
    return dstBuffer - initDstBuffer;
}

unsigned NALUnit::extractUEGolombCode(uint8_t* buffer, uint8_t* bufEnd)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, bufEnd);
    return extractUEGolombCode(reader);
}

unsigned NALUnit::extractUEGolombCode()
{
    int cnt = 0;
    for (; bitReader.getBits(1) == 0; cnt++)
        ;
    if (cnt > INT_BIT)
        THROW_BITSTREAM_ERR;
    return (1 << cnt) - 1 + bitReader.getBits(cnt);
}

void NALUnit::writeSEGolombCode(BitStreamWriter& bitWriter, int32_t value)
{
    if (value <= 0)
        writeUEGolombCode(bitWriter, -value * 2);
    else
        writeUEGolombCode(bitWriter, value * 2 - 1);
}

void NALUnit::writeUEGolombCode(BitStreamWriter& bitWriter, uint32_t value)
{
    /*
    int maxVal = 0;
    int x = 2;
    int nBit = 0;
    for (; maxVal < value; maxVal += x ) {
            x <<= 1;
            nBit++;
    }
    */
    uint32_t maxVal = 0;
    int x = 1;
    int nBit = 0;
    for (; maxVal < value; maxVal += x)
    {
        x <<= 1;
        nBit++;
    }

    bitWriter.putBits(nBit + 1, 1);
    bitWriter.putBits(nBit, value - (x - 1));
}

unsigned NALUnit::extractUEGolombCode(BitStreamReader& bitReader)
{
    int cnt = 0;
    for (; bitReader.getBits(1) == 0; cnt++)
        ;
    return (1 << cnt) - 1 + bitReader.getBits(cnt);
}

int NALUnit::extractSEGolombCode()
{
    unsigned rez = extractUEGolombCode();
    if (rez % 2 == 0)
        return -(int)(rez / 2);
    else
        return (int)((rez + 1) / 2);
}

int NALUnit::deserialize(uint8_t* buffer, uint8_t* end)
{
    if (end == buffer)
        return NOT_ENOUGH_BUFFER;

    // assert((*buffer & 0x80) == 0);
    if ((*buffer & 0x80) != 0)
    {
        LTRACE(LT_WARN, 0, "Invalid forbidden_zero_bit for nal unit " << (*buffer & 0x1f));
    }

    nal_ref_idc = (*buffer >> 5) & 0x3;
    nal_unit_type = *buffer & 0x1f;
    return 0;
}

void NALUnit::decodeBuffer(const uint8_t* buffer, const uint8_t* end)
{
    delete[] m_nalBuffer;
    m_nalBuffer = new uint8_t[end - buffer];
    m_nalBufferLen = decodeNAL(buffer, end, m_nalBuffer, end - buffer);
}

int NALUnit::serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd, bool writeStartCode) const
{
    if (m_nalBufferLen == 0)
        return 0;
    if (writeStartCode)
    {
        if (dstEnd - dstBuffer < 4)
            return -1;
        *dstBuffer++ = 0;
        *dstBuffer++ = 0;
        *dstBuffer++ = 0;
        *dstBuffer++ = 1;
    }
    int encodeRez = NALUnit::encodeNAL(m_nalBuffer, m_nalBuffer + m_nalBufferLen, dstBuffer, dstEnd - dstBuffer);
    if (encodeRez == -1)
        return -1;
    else
        return encodeRez + (writeStartCode ? 4 : 0);
}

int NALUnit::serialize(uint8_t* dstBuffer)
{
    *dstBuffer++ = 0;
    *dstBuffer++ = 0;
    *dstBuffer++ = 1;
    *dstBuffer = ((nal_ref_idc & 3) << 5) + (nal_unit_type & 0x1f);
    return 4;
}

int ceil_log2(double val)
{
    int iVal = (int)val;
    double frac = val - iVal;
    int bits = 0;
    for (; iVal > 0; iVal >>= 1)
    {
        bits++;
    }
    int mask = 1 << (bits - 1);
    iVal = (int)val;
    if (iVal - mask == 0 && frac == 0)
        return bits - 1;  // For example: cail(log2(8.0)) = 3, but for 8.2 or 9.0 it's 4
    else
        return bits;
}

// -------------------- NALDelimiter ------------------
int NALDelimiter::deserialize(uint8_t* buffer, uint8_t* end)
{
    int rez = NALUnit::deserialize(buffer, end);
    if (rez != 0)
        return rez;
    if (end - buffer < 2)
        return NOT_ENOUGH_BUFFER;
    primary_pic_type = buffer[1] >> 5;
    return 0;
}

int NALDelimiter::serialize(uint8_t* buffer)
{
    uint8_t* curBuf = buffer;
    curBuf += NALUnit::serialize(curBuf);
    *curBuf++ = (primary_pic_type << 5) + 0x10;
    return (int)(curBuf - buffer);
}

// -------------------- PPSUnit --------------------------
int PPSUnit::deserialize()
{
    uint8_t* nalEnd = m_nalBuffer + m_nalBufferLen;
    int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
    if (rez != 0)
        return rez;
    if (nalEnd - m_nalBuffer < 2)
        return NOT_ENOUGH_BUFFER;
    try
    {
        bitReader.setBuffer(m_nalBuffer + 1, nalEnd);
        pic_parameter_set_id = extractUEGolombCode();
        if (pic_parameter_set_id >= 256)
            return 1;
        seq_parameter_set_id = extractUEGolombCode();
        if (seq_parameter_set_id >= 32)
            return 1;
        // entropy_coding_mode_BitPos = bitReader.getBitsCount();
        entropy_coding_mode_flag = bitReader.getBit();
        pic_order_present_flag = bitReader.getBit();

        /*
                num_slice_groups_minus1 = extractUEGolombCode();
                slice_group_map_type = 0;
                if( num_slice_groups_minus1 > 0 ) {
                        slice_group_map_type = extractUEGolombCode();
                        if( slice_group_map_type  ==  0 )
                        {
                                if (num_slice_groups_minus1 >= 256)
                                        THROW_BITSTREAM_ERR;
                                for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )
                                        run_length_minus1[iGroup] = extractUEGolombCode();
                        }
                        else if( slice_group_map_type  ==  2)
                        {
                                if (num_slice_groups_minus1 >= 256)
                                        THROW_BITSTREAM_ERR;
                                for( int iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ ) {
                                        top_left[ iGroup ] = extractUEGolombCode();
                                        bottom_right[ iGroup ] = extractUEGolombCode();
                                }
                        }
                        else if(  slice_group_map_type  ==  3  ||
                                                slice_group_map_type  ==  4  ||
                                                slice_group_map_type  ==  5 )
                        {
                                slice_group_change_direction_flag = bitReader.getBits(1);
                                slice_group_change_rate = extractUEGolombCode() + 1;
                        } else if( slice_group_map_type  ==  6 )
                        {
                                int pic_size_in_map_units_minus1 = extractUEGolombCode();
                                if (pic_size_in_map_units_minus1 >= 256)
                                        THROW_BITSTREAM_ERR;
                                for( int i = 0; i <= pic_size_in_map_units_minus1; i++ ) {
                                        int bits = ceil_log2( num_slice_groups_minus1 + 1 );
                                        slice_group_id[i] = bitReader.getBits(1);
                                }
                        }
                }
                num_ref_idx_l0_active_minus1 = extractUEGolombCode();
                num_ref_idx_l1_active_minus1 = extractUEGolombCode();
                weighted_pred_flag = bitReader.getBit();
                weighted_bipred_idc = bitReader.getBits(2);
                pic_init_qp_minus26 = extractSEGolombCode();  // relative to 26
                pic_init_qs_minus26 = extractSEGolombCode();  // relative to 26
                chroma_qp_index_offset =  extractSEGolombCode();
                deblocking_filter_control_present_flag = bitReader.getBit();
                constrained_intra_pred_flag = bitReader.getBit();
                redundant_pic_cnt_present_flag = bitReader.getBit();
                m_ppsLenInMbit = bitReader.getBitsCount() + 8;
        */
        m_ready = true;
        return 0;
    }
    catch (BitStreamException)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

/*
void PPSUnit::duplicatePPS(PPSUnit& oldPPS, int ppsID, bool cabac)
{
        delete m_nalBuffer;
        memcpy(this, &oldPPS, sizeof(PPSUnit));
        m_nalBuffer = new uint8_t[oldPPS.m_nalBufferLen + 400]; // 4 bytes reserved for new ppsID and cabac values
        m_nalBuffer[0] = oldPPS.m_nalBuffer[0];

        pic_parameter_set_id = ppsID;
        entropy_coding_mode_flag = cabac;

        BitStreamWriter bitWriter;
        bitWriter.setBuffer(m_nalBuffer + 1, m_nalBuffer + m_nalBufferLen + 4);
        writeUEGolombCode(bitWriter, ppsID);
        writeUEGolombCode(bitWriter, seq_parameter_set_id);
        bitWriter.putBit(cabac);
        bitReader.setBuffer(oldPPS.m_nalBuffer+1, oldPPS.m_nalBuffer + oldPPS.m_nalBufferLen);
        extractUEGolombCode();
        extractUEGolombCode();
        bitReader.skipBit(); // skip cabac field
        int bitsToCopy = oldPPS.m_ppsLenInMbit - bitReader.getBitsCount();
        for (; bitsToCopy >=32; bitsToCopy -=32) {
                uint32_t value = bitReader.getBits(32);
                bitWriter.putBits(32, value);
        }
        if (bitsToCopy > 0) {
                uint32_t value = bitReader.getBits(bitsToCopy);
                bitWriter.putBits(bitsToCopy, value);
        }
        if (bitWriter.getBitsCount() % 8 != 0)
                bitWriter.putBits(8 - bitWriter.getBitsCount() % 8, 0);
        m_nalBufferLen = bitWriter.getBitsCount() / 8 + 1;
        assert(m_nalBufferLen <= oldPPS.m_nalBufferLen + 4);
}
*/

// -------------------- HRDParams -------------------------
HRDParams::HRDParams()
{
    resetDefault(false);
    isPresent = false;
}

HRDParams::~HRDParams() {}

void HRDParams::resetDefault(bool mvc)
{
    isPresent = false;
    bitLen = 0;

    initial_cpb_removal_delay_length_minus1 = 23;
    cpb_removal_delay_length_minus1 = 23;
    dpb_output_delay_length_minus1 = 23;
    time_offset_length = 0;

    cpb_cnt_minus1 = 0;
    cbr_flag.resize(1);
    cbr_flag[0] = 0;

    bit_rate_value_minus1.resize(1);
    cpb_size_value_minus1.resize(1);

    // todo: implement different parameters for MVC and AVC here
    if (!mvc)
    {
        bit_rate_scale = 3;
        cpb_size_scale = 3;
        bit_rate_value_minus1[0] = 78124;  // 78124*512 = 40 mbit. It is max allowed value for Blu-ray disks
        cpb_size_value_minus1[0] =
            234374;  // 234374*128 = 30 mbit per frame max. It is max allowed value for Blu-ray disks
    }
    else
    {
        bit_rate_scale = 2;
        cpb_size_scale = 3;
        bit_rate_value_minus1[0] = 234374;  // 60 mbit for MVC+AVC It is max allowed value for Blu-ray disks
        cpb_size_value_minus1[0] = 468749;
    }
}

// -------------------- SPSUnit --------------------------

SPSUnit::SPSUnit()
    : NALUnit(),
      m_ready(false),
      sar_width(0),
      sar_height(0),
      num_units_in_tick(0),
      time_scale(0),
      fixed_frame_rate_flag(0),
      num_units_in_tick_bit_pos(0),
      pic_struct_present_flag(0),
      mb_adaptive_frame_field_flag(0),
      frame_crop_left_offset(0),
      frame_crop_right_offset(0),
      frame_crop_top_offset(0),
      frame_crop_bottom_offset(0),
      full_sps_bit_len(0),
      low_delay_hrd_flag(0),
      separate_colour_plane_flag(0),
      profile_idc(0),
      num_views(0),
      vui_parameters_bit_pos(-1),
      hrdParamsBitPos(-1)
// m_pulldown = false;
{
}

void SPSUnit::scaling_list(int* scalingList, int sizeOfScalingList, bool& useDefaultScalingMatrixFlag)
{
    int lastScale = 8;
    int nextScale = 8;
    for (int j = 0; j < sizeOfScalingList; j++)
    {
        if (nextScale != 0)
        {
            int delta_scale = extractSEGolombCode();
            nextScale = (lastScale + delta_scale + 256) % 256;
            useDefaultScalingMatrixFlag = (j == 0 && nextScale == 0);
        }
        scalingList[j] = (nextScale == 0) ? lastScale : nextScale;
        lastScale = scalingList[j];
    }
}

int SPSUnit::deserialize()
{
    /*
    uint8_t* nextNal = findNALWithStartCode(buffer, end);
    long bufSize = nextNal - buffer;
    uint8_t* tmpBuff = new uint8_t[bufSize];
    uint8_t* tmpBufferEnd = tmpBuff + bufSize;
    int m_decodedBuffSize = decodeNAL(buffer, nextNal, tmpBuff, bufSize);
    */
    if (m_nalBufferLen < 4)
        return NOT_ENOUGH_BUFFER;
    int rez = NALUnit::deserialize(m_nalBuffer, m_nalBuffer + m_nalBufferLen);
    if (rez != 0)
        return rez;
    profile_idc = m_nalBuffer[1];
    constraint_set0_flag0 = m_nalBuffer[2] >> 7;
    constraint_set0_flag1 = m_nalBuffer[2] >> 6 & 1;
    constraint_set0_flag2 = m_nalBuffer[2] >> 5 & 1;
    constraint_set0_flag3 = m_nalBuffer[2] >> 4 & 1;
    level_idc = m_nalBuffer[3];
    chroma_format_idc = 1;  // by default format is 4:2:0
    try
    {
        bitReader.setBuffer(m_nalBuffer + 4, m_nalBuffer + m_nalBufferLen);
        seq_parameter_set_id = extractUEGolombCode();
        if (seq_parameter_set_id >= 32)
            return 1;
        pic_order_cnt_type = 0;
        if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244 || profile_idc == 44 ||
            profile_idc == 83 || profile_idc == 86 || profile_idc == 118 || profile_idc == 128)
        {
            chroma_format_idc = extractUEGolombCode();
            if (chroma_format_idc >= 4)
                return 1;
            if (chroma_format_idc == 3)
                separate_colour_plane_flag = bitReader.getBits(1);
            unsigned bit_depth_luma = extractUEGolombCode() + 8;
            if (bit_depth_luma > 14)
                return 1;
            unsigned bit_depth_chroma = extractUEGolombCode() + 8;
            if (bit_depth_chroma > 14)
                return 1;
            int qpprime_y_zero_transform_bypass_flag = bitReader.getBits(1);
            int seq_scaling_matrix_present_flag = bitReader.getBits(1);
            if (seq_scaling_matrix_present_flag != 0)
            {
                int ScalingList4x4[6][16];
                int ScalingList8x8[2][64];
                bool UseDefaultScalingMatrix4x4Flag[6];
                bool UseDefaultScalingMatrix8x8Flag[2];

                for (int i = 0; i < 8; i++)
                {
                    // seq_scaling_list_present_flag[i] = bitReader.getBits(1);
                    // if( seq_scaling_list_present_flag[i])
                    if (bitReader.getBits(1))
                    {
                        if (i < 6)
                            scaling_list(ScalingList4x4[i], 16, UseDefaultScalingMatrix4x4Flag[i]);
                        else
                            scaling_list(ScalingList8x8[i - 6], 64, UseDefaultScalingMatrix8x8Flag[i - 6]);
                    }
                }
            }
        }
        log2_max_frame_num = extractUEGolombCode() + 4;
        if (log2_max_frame_num > 16)
            return 1;

        // next parameters not used  now.

        pic_order_cnt_type = extractUEGolombCode();
        if (pic_order_cnt_type > 2)
            return 1;
        log2_max_pic_order_cnt_lsb = 0;
        delta_pic_order_always_zero_flag = 0;
        if (pic_order_cnt_type == 0)
        {
            log2_max_pic_order_cnt_lsb = extractUEGolombCode() + 4;
            if (log2_max_pic_order_cnt_lsb > 16)
                return 1;
        }
        else if (pic_order_cnt_type == 1)
        {
            delta_pic_order_always_zero_flag = bitReader.getBits(1);
            offset_for_non_ref_pic = extractSEGolombCode();
            extractSEGolombCode();  // offset_for_top_to_bottom_field
            num_ref_frames_in_pic_order_cnt_cycle = extractUEGolombCode();
            if (num_ref_frames_in_pic_order_cnt_cycle >= 256)
                return 1;

            for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
                extractSEGolombCode();  // offset_for_ref_frame[i]
        }

        num_ref_frames = extractUEGolombCode();
        int gaps_in_frame_num_value_allowed_flag = bitReader.getBits(1);
        pic_width_in_mbs = extractUEGolombCode() + 1;
        pic_height_in_map_units = extractUEGolombCode() + 1;
        frame_mbs_only_flag = bitReader.getBits(1);
        if (!frame_mbs_only_flag)
            mb_adaptive_frame_field_flag = bitReader.getBits(1);
        int direct_8x8_inference_flag = bitReader.getBits(1);
        frame_cropping_flag = bitReader.getBits(1);
        if (frame_cropping_flag)
        {
            frame_crop_left_offset = extractUEGolombCode();
            frame_crop_right_offset = extractUEGolombCode();
            frame_crop_top_offset = extractUEGolombCode();
            frame_crop_bottom_offset = extractUEGolombCode();
        }
        pic_size_in_map_units = pic_width_in_mbs * pic_height_in_map_units;  // * (2 - frame_mbs_only_flag);
        vui_parameters_bit_pos = bitReader.getBitsCount() + 32;
        vui_parameters_present_flag = bitReader.getBits(1);
        if (vui_parameters_present_flag)
            if (deserializeVuiParameters() != 0)
                return 1;

        if (nal_unit_type == nuSubSPS)
        {
            int rez = deserializeSubSPS();
            if (rez != 0)
                return rez;
        }

        m_ready = true;
        // full_sps_bit_len = bitReader.getBitsCount() + 32;
        full_sps_bit_len = calcNalLenInBits(m_nalBuffer, m_nalBuffer + m_nalBufferLen);

        // m_pulldown = ((abs(getFPS() - 23.97) < FRAME_RATE_EPS) || (abs(getFPS() - 59.94) < FRAME_RATE_EPS)) &&
        // pic_struct_present_flag;

        return 0;
    }
    catch (BitStreamException)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

int SPSUnit::deserializeSubSPS()
{
    if (profile_idc == 83 || profile_idc == 86)
    {
        /*
        seq_parameter_set_svc_extension();  // specified in Annex G
        int svc_vui_parameters_present_flag = bitReader.getBit();
        if( svc_vui_parameters_present_flag)
            svc_vui_parameters_extension();  // specified in Annex G
        */
    }
    else if (profile_idc == 118 || profile_idc == 128)
    {
        if (bitReader.getBit() != 1)
            return INVALID_BITSTREAM_SYNTAX;
        if (seq_parameter_set_mvc_extension() != 0)  // specified in Annex H
            return 1;
        int mvc_vui_parameters_present_flag = bitReader.getBit();
        if (mvc_vui_parameters_present_flag)
            if (mvc_vui_parameters_extension() != 0)  // specified in Annex H
                return 1;
    }

    // int additional_extension2_flag = bitReader.getBit();
    return 0;
}

int SPSUnit::deserializeVuiParameters()
{
    aspect_ratio_info_present_flag = bitReader.getBit();
    if (aspect_ratio_info_present_flag)
    {
        aspect_ratio_idc = bitReader.getBits(8);
        if (aspect_ratio_idc == Extended_SAR)
        {
            sar_width = bitReader.getBits(16);
            sar_height = bitReader.getBits(16);
        }
    }
    int overscan_info_present_flag = bitReader.getBit();
    if (overscan_info_present_flag)
        int overscan_appropriate_flag = bitReader.getBit();
    int video_signal_type_present_flag = bitReader.getBit();
    if (video_signal_type_present_flag)
    {
        int video_format = bitReader.getBits(3);
        int video_full_range_flag = bitReader.getBit();
        int colour_description_present_flag = bitReader.getBit();
        if (colour_description_present_flag)
        {
            int colour_primaries = bitReader.getBits(8);
            int transfer_characteristics = bitReader.getBits(8);
            int matrix_coefficients = bitReader.getBits(8);
        }
    }
    int chroma_loc_info_present_flag = bitReader.getBit();
    if (chroma_loc_info_present_flag)
    {
        unsigned chroma_sample_loc_type_top_field = extractUEGolombCode();
        if (chroma_sample_loc_type_top_field > 5)
            return 1;
        unsigned chroma_sample_loc_type_bottom_field = extractUEGolombCode();
        if (chroma_sample_loc_type_bottom_field > 5)
            return 1;
    }
    timing_info_present_flag = bitReader.getBit();
    if (timing_info_present_flag)
    {
        num_units_in_tick_bit_pos = bitReader.getBitsCount();
        num_units_in_tick = bitReader.getBits(32);
        time_scale = bitReader.getBits(32);
        fixed_frame_rate_flag = bitReader.getBit();
    }
    hrdParamsBitPos = bitReader.getBitsCount() + 32;

    // orig_hrd_parameters_present_flag =
    nalHrdParams.isPresent = bitReader.getBit();
    if (nalHrdParams.isPresent)
    {
        int beforeCount = bitReader.getBitsCount();
        if (hrd_parameters(nalHrdParams) != 0)
            return 1;
        nalHrdParams.bitLen = bitReader.getBitsCount() - beforeCount;
    }

    vclHrdParams.isPresent = bitReader.getBit();
    if (vclHrdParams.isPresent)
    {
        int beforeCount = bitReader.getBitsCount();
        if (hrd_parameters(vclHrdParams) != 0)
            return 1;
        vclHrdParams.bitLen = bitReader.getBitsCount() - beforeCount;
    }
    if (nalHrdParams.isPresent || vclHrdParams.isPresent)
        low_delay_hrd_flag = bitReader.getBit();
    pic_struct_present_flag = bitReader.getBit();
    int bitstream_restriction_flag = bitReader.getBit();
    if (bitstream_restriction_flag)
    {
        int motion_vectors_over_pic_boundaries_flag = bitReader.getBit();
        unsigned max_bytes_per_pic_denom = extractUEGolombCode();
        if (max_bytes_per_pic_denom > 16)
            return 1;
        unsigned max_bits_per_mb_denom = extractUEGolombCode();
        if (max_bits_per_mb_denom > 16)
            return 1;
        unsigned log2_max_mv_length_horizontal = extractUEGolombCode();
        if (log2_max_mv_length_horizontal > 16)
            return 1;
        unsigned log2_max_mv_length_vertical = extractUEGolombCode();
        if (log2_max_mv_length_vertical > 16)
            return 1;
        unsigned num_reorder_frames = extractUEGolombCode();
        unsigned max_dec_frame_buffering = extractUEGolombCode();
        if (num_reorder_frames > max_dec_frame_buffering)
            return 1;
    }
    return 0;
}

void SPSUnit::serializeHRDParameters(BitStreamWriter& writer, const HRDParams& params)
{
    writeUEGolombCode(writer, params.cpb_cnt_minus1);
    writer.putBits(4, params.bit_rate_scale);
    writer.putBits(4, params.cpb_size_scale);
    for (size_t SchedSelIdx = 0; SchedSelIdx <= params.cpb_cnt_minus1; SchedSelIdx++)
    {
        writeUEGolombCode(writer, params.bit_rate_value_minus1[SchedSelIdx]);
        writeUEGolombCode(writer, params.cpb_size_value_minus1[SchedSelIdx]);
        writer.putBit(params.cbr_flag[SchedSelIdx]);
    }
    writer.putBits(5, params.initial_cpb_removal_delay_length_minus1);
    writer.putBits(5, params.cpb_removal_delay_length_minus1);
    writer.putBits(5, params.dpb_output_delay_length_minus1);
    writer.putBits(5, params.time_offset_length);
    // writer.flushBits();
}

// insert HRD parameters to bitstream.
void SPSUnit::insertHrdParameters()
{
    for (int i = mvcNalHrdParams.size() - 1; i >= 0; --i)
    {
        if (!mvcNalHrdParams[i].isPresent || !mvcVclHrdParams[i].isPresent)
        {
            int nalBitLen = mvcNalHrdParams[i].bitLen;
            int vclBitLen = mvcVclHrdParams[i].bitLen;

            if (mvcNalHrdParams[i].isPresent || !mvcVclHrdParams[i].isPresent)
                mvcVclHrdParams[i] = mvcNalHrdParams[i];
            else if (!mvcNalHrdParams[i].isPresent || mvcVclHrdParams[i].isPresent)
                mvcNalHrdParams[i] = mvcVclHrdParams[i];
            else
            {
                mvcNalHrdParams[i].resetDefault(true);
                mvcVclHrdParams[i].resetDefault(true);
            }

            insertHrdData(mvcHrdParamsBitPos[i], nalBitLen, vclBitLen, false, mvcNalHrdParams[i]);
            mvcNalHrdParams[i].isPresent = true;
            mvcVclHrdParams[i].isPresent = true;
        }
    }

    if (!nalHrdParams.isPresent || !vclHrdParams.isPresent)
    {
        int nalBitLen = nalHrdParams.bitLen;
        int vclBitLen = vclHrdParams.bitLen;

        if (nalHrdParams.isPresent || !vclHrdParams.isPresent)
            vclHrdParams = nalHrdParams;
        else if (!nalHrdParams.isPresent || vclHrdParams.isPresent)
            nalHrdParams = vclHrdParams;
        else
        {
            nalHrdParams.resetDefault(false);
            vclHrdParams.resetDefault(false);
        }

        if (hrdParamsBitPos != -1)
            insertHrdData(hrdParamsBitPos, nalBitLen, vclBitLen, false, nalHrdParams);
        else
            insertHrdData(vui_parameters_bit_pos, 0, 0, true, nalHrdParams);
        nalHrdParams.isPresent = true;
        vclHrdParams.isPresent = true;
    }
    nalHrdParams.isPresent = 1;
    vclHrdParams.isPresent = 1;

    if (!fixed_frame_rate_flag)
        updateTimingInfo();
}

void SPSUnit::updateTimingInfo()
{
    // replace hrd parameters not implemented. only insert
    int bitPos = hrdParamsBitPos - 1;

    const int EXTRA_SPACE = 64;

    uint8_t* newNalBuffer = new uint8_t[m_nalBufferLen + EXTRA_SPACE];

    int beforeBytes = bitPos >> 3;
    memcpy(newNalBuffer, m_nalBuffer, m_nalBufferLen);

    BitStreamReader reader;
    reader.setBuffer(m_nalBuffer + beforeBytes, m_nalBuffer + m_nalBufferLen);
    BitStreamWriter writer;
    writer.setBuffer(newNalBuffer + beforeBytes, newNalBuffer + m_nalBufferLen + EXTRA_SPACE);
    int tmpVal = reader.getBits(bitPos & 7);
    writer.putBits(bitPos & 7, tmpVal);

    if (!timing_info_present_flag)
    {
        reader.skipBit();  // timing_info_preset_flag
        writer.putBit(1);  // timing_info_preset_flag

        writer.putBits(32, num_units_in_tick);
        writer.putBits(32, time_scale);
        writer.putBit(1);  // fixed framerate flag
    }
    else
    {
        reader.skipBit();  // fixed framerate flag
        writer.putBit(1);  // fixed framerate flag
    }

    timing_info_present_flag = 1;
    fixed_frame_rate_flag = 1;

    // copy end of SPS
    int bitRest = full_sps_bit_len - reader.getBitsCount() - beforeBytes * 8;
    for (; bitRest >= 8; bitRest -= 8)
    {
        uint8_t tmpVal = reader.getBits(8);
        writer.putBits(8, tmpVal);
    }
    if (bitRest > 0)
    {
        uint8_t tmpVal = reader.getBits(bitRest);
        writer.putBits(bitRest, tmpVal);
    }

    full_sps_bit_len = writer.getBitsCount() + beforeBytes * 8;

    write_rbsp_trailing_bits(writer);
    writer.flushBits();
    m_nalBufferLen = writer.getBitsCount() / 8 + beforeBytes;

    delete[] m_nalBuffer;
    m_nalBuffer = newNalBuffer;
}

void SPSUnit::insertHrdData(int bitPos, int nal_hrd_len, int vcl_hrd_len, bool addVuiHeader, const HRDParams& params)
{
    // replace hrd parameters not implemented. only insert
    const int EXTRA_SPACE = 64;

    uint8_t* newNalBuffer = new uint8_t[m_nalBufferLen + EXTRA_SPACE];

    int beforeBytes = bitPos >> 3;
    memcpy(newNalBuffer, m_nalBuffer, m_nalBufferLen);

    BitStreamReader reader;
    reader.setBuffer(m_nalBuffer + beforeBytes, m_nalBuffer + m_nalBufferLen);
    BitStreamWriter writer;
    writer.setBuffer(newNalBuffer + beforeBytes, newNalBuffer + m_nalBufferLen + EXTRA_SPACE);
    int tmpVal = reader.getBits(bitPos & 7);
    writer.putBits(bitPos & 7, tmpVal);

    if (addVuiHeader)
    {
        reader.skipBit();  // old vui present header
        writer.putBit(1);  // enable vui parameters
        writer.putBit(0);  // no aspect ratio info
        writer.putBit(0);  // overscan_info_present_flag
        writer.putBit(0);  // video_signal_type_present_flag
        writer.putBit(0);  // chroma_loc_info_present_flag

        timing_info_present_flag = 1;
        fixed_frame_rate_flag = 1;
        writer.putBit(1);  // timing info present flag
        writer.putBits(32, num_units_in_tick);
        writer.putBits(32, time_scale);
        writer.putBit(1);  // fixed framerate flag

        writer.putBit(1);  // enable nal_hrd_params flags here
        serializeHRDParameters(writer, nalHrdParams);

        writer.putBit(1);  // enable vcl_hrd_params flags here
        serializeHRDParameters(writer, nalHrdParams);

        writer.putBit(low_delay_hrd_flag);
        writer.putBit(0);  // pic_struct_present_flag
        writer.putBit(0);  // bitstream_restriction_flag
    }
    else
    {
        writer.putBit(1);  // enable nal_hrd_params flags here
        if (reader.getBit())
        {  // source nal_hrd_parameters_present_flag
            // nal hrd already exists, copy from a source stream
            for (int i = 0; i < nal_hrd_len / 32; ++i) writer.putBits(32, reader.getBits(32));
            writer.putBits(nal_hrd_len % 32, reader.getBits(nal_hrd_len % 32));
        }
        else
        {
            serializeHRDParameters(writer, params);
        }

        writer.putBit(1);  // enable nal_vcl_params flags here
        if (reader.getBit())
        {  // source vcl_hrd_parameters_present_flag
            // vcl hrd already exists, copy from a source stream
            for (int i = 0; i < vcl_hrd_len / 32; ++i) writer.putBits(32, reader.getBits(32));
            writer.putBits(vcl_hrd_len % 32, reader.getBits(vcl_hrd_len % 32));
        }
        else
        {
            serializeHRDParameters(writer, params);
        }

        if (nal_hrd_len == 0 && vcl_hrd_len == 0)
            writer.putBit(low_delay_hrd_flag);  // absent in the source stream, add
    }

    // copy end of SPS
    int bitRest = full_sps_bit_len - reader.getBitsCount() - beforeBytes * 8;
    for (; bitRest >= 8; bitRest -= 8)
    {
        uint8_t tmpVal = reader.getBits(8);
        writer.putBits(8, tmpVal);
    }
    if (bitRest > 0)
    {
        uint8_t tmpVal = reader.getBits(bitRest);
        writer.putBits(bitRest, tmpVal);
    }

    full_sps_bit_len = writer.getBitsCount() + beforeBytes * 8;

    write_rbsp_trailing_bits(writer);
    writer.flushBits();
    m_nalBufferLen = writer.getBitsCount() / 8 + beforeBytes;

    delete[] m_nalBuffer;
    m_nalBuffer = newNalBuffer;
}

int SPSUnit::getMaxBitrate()
{
    if (nalHrdParams.bit_rate_value_minus1.empty() == 0)
        return 0;
    else
        return (nalHrdParams.bit_rate_value_minus1[0] + 1) << (6 + nalHrdParams.bit_rate_scale);
}

int SPSUnit::hrd_parameters(HRDParams& params)
{
    params.cpb_cnt_minus1 = extractUEGolombCode();
    if (params.cpb_cnt_minus1 >= 32)
        return 1;
    params.bit_rate_scale = bitReader.getBits(4);
    params.cpb_size_scale = bitReader.getBits(4);

    params.bit_rate_value_minus1.resize(params.cpb_cnt_minus1 + 1);
    params.cpb_size_value_minus1.resize(params.cpb_cnt_minus1 + 1);
    params.cbr_flag.resize(params.cpb_cnt_minus1 + 1);

    for (size_t SchedSelIdx = 0; SchedSelIdx <= params.cpb_cnt_minus1; SchedSelIdx++)
    {
        params.bit_rate_value_minus1[SchedSelIdx] = extractUEGolombCode();
        if (params.bit_rate_value_minus1[SchedSelIdx] == 0xffffffff)
            return 1;
        params.cpb_size_value_minus1[SchedSelIdx] = extractUEGolombCode();
        if (params.cpb_size_value_minus1[SchedSelIdx] == 0xffffffff)
            return 1;
        params.cbr_flag[SchedSelIdx] = bitReader.getBit();
    }
    params.initial_cpb_removal_delay_length_minus1 = bitReader.getBits(5);
    params.cpb_removal_delay_length_minus1 = bitReader.getBits(5);
    params.dpb_output_delay_length_minus1 = bitReader.getBits(5);
    params.time_offset_length = bitReader.getBits(5);

    return 0;
}

int SPSUnit::getCropY()
{
    if (chroma_format_idc == 0)
        return (2 - frame_mbs_only_flag) * (frame_crop_top_offset + frame_crop_bottom_offset);
    else
    {
        int SubHeightC = 1;
        if (chroma_format_idc == 1)
            SubHeightC = 2;
        return SubHeightC * (2 - frame_mbs_only_flag) * (frame_crop_top_offset + frame_crop_bottom_offset);
    }
}

int SPSUnit::getCropX()
{
    if (chroma_format_idc == 0)
        return frame_crop_left_offset + frame_crop_right_offset;
    else
    {
        int SubWidthC = 1;
        if (chroma_format_idc == 1 || chroma_format_idc == 2)
            SubWidthC = 2;
        return SubWidthC * (frame_crop_left_offset + frame_crop_right_offset);
    }
}

double SPSUnit::getFPS() const
{
    if (num_units_in_tick != 0)
    {
        double tmp = time_scale / (float)num_units_in_tick / 2;  //(float)(frame_mbs_only_flag+1);
        // if (abs(tmp - (double) 23.9760239760) < 3e-3)
        //	return 23.9760239760;
        return tmp;
    }
    else
        return 0;
}

void SPSUnit::setFps(double fps)
{
    time_scale = (uint32_t)(fps + 0.5) * 1000000;
    // time_scale = (uint32_t)(fps+0.5) * 1000;
    num_units_in_tick = time_scale / fps + 0.5;
    time_scale *= 2;

    if (num_units_in_tick_bit_pos > 0)
    {
        updateBits(num_units_in_tick_bit_pos, 32, num_units_in_tick);
        updateBits(num_units_in_tick_bit_pos + 32, 32, time_scale);
    }
}

std::string SPSUnit::getStreamDescr()
{
    std::ostringstream rez;

    if (nal_unit_type == nuSubSPS)
    {
        if (profile_idc == 83 || profile_idc == 86)
            rez << "SVC part ";
        else if (profile_idc == 118 || profile_idc == 128)
            rez << "H.264/MVC Views: " << int32ToStr(num_views) << " ";
    }

    rez << "Profile: ";
    if (profile_idc <= 66)
        rez << "Baseline@";
    else if (profile_idc <= 77)
        rez << "Main@";
    else
        rez << "High@";
    rez << level_idc / 10;
    rez << ".";
    rez << level_idc % 10 << "  ";
    rez << "Resolution: " << getWidth() << ':' << getHeight();
    rez << (frame_mbs_only_flag ? 'p' : 'i') << "  ";
    rez << "Frame rate: ";
    double fps = getFPS();
    if (fps != 0)
    {
        rez << fps;
    }
    else
        rez << "not found";

    return rez.str();
}

int SPSUnit::seq_parameter_set_mvc_extension()
{
    unsigned num_views_minus1 = extractUEGolombCode();
    if (num_views_minus1 >= 1 << 10)
        return 1;
    num_views = num_views_minus1 + 1;

    view_id.resize(num_views);
    for (int i = 0; i < num_views; i++)
    {
        view_id[i] = extractUEGolombCode();
        if (view_id[i] >= 1 << 10)
            return 1;
    }

    std::vector<unsigned> num_anchor_refs_l0;
    std::vector<unsigned> num_anchor_refs_l1;
    std::vector<unsigned> num_non_anchor_refs_l0;
    std::vector<unsigned> num_applicable_ops_minus1;
    std::vector<unsigned> num_non_anchor_refs_l1;

    num_anchor_refs_l0.resize(num_views);
    num_anchor_refs_l1.resize(num_views);
    num_non_anchor_refs_l0.resize(num_views);
    num_non_anchor_refs_l1.resize(num_views);

    for (int i = 1; i < num_views; i++)
    {
        num_anchor_refs_l0[i] = extractUEGolombCode();
        if (num_anchor_refs_l0[i] >= 16)
            return 1;
        for (size_t j = 0; j < num_anchor_refs_l0[i]; j++)
            if (extractUEGolombCode() >= 1 << 10)  // anchor_ref_l0[ i ][ j ]
                return 1;
        num_anchor_refs_l1[i] = extractUEGolombCode();
        if (num_anchor_refs_l1[i] >= 16)
            return 1;
        for (size_t j = 0; j < num_anchor_refs_l1[i]; j++)
            if (extractUEGolombCode() >= 1 << 10)  // anchor_ref_l1[ i ][ j ]
                return 1;
    }

    for (int i = 1; i < num_views; i++)
    {
        num_non_anchor_refs_l0[i] = extractUEGolombCode();
        if (num_non_anchor_refs_l0[i] >= 16)
            return 1;
        for (size_t j = 0; j < num_non_anchor_refs_l0[i]; j++)
            if (extractUEGolombCode() >= 1 << 10)  // non_anchor_ref_l0[ i ][ j ]
                return 1;
        num_non_anchor_refs_l1[i] = extractUEGolombCode();
        if (num_non_anchor_refs_l1[i] >= 16)
            return 1;
        for (size_t j = 0; j < num_non_anchor_refs_l1[i]; j++)
            if (extractUEGolombCode() >= 1 << 10)  // non_anchor_ref_l1[ i ][ j ]
                return 1;
    }

    int num_level_values_signalled_minus1 = extractUEGolombCode();
    if (num_level_values_signalled_minus1 >= 64)
        return 1;
    num_applicable_ops_minus1.resize(num_level_values_signalled_minus1 + 1);
    level_idc_ext.resize(num_level_values_signalled_minus1 + 1);
    for (int i = 0; i <= num_level_values_signalled_minus1; i++)
    {
        level_idc_ext[i] = bitReader.getBits(8);
        num_applicable_ops_minus1[i] = extractUEGolombCode();
        if (num_applicable_ops_minus1[i] >= 1 << 10)
            return 1;
        for (size_t j = 0; j <= num_applicable_ops_minus1[i]; j++)
        {
            bitReader.getBits(3);                    // applicable_op_temporal_id[ i ][ j ]
            unsigned dummy = extractUEGolombCode();  // applicable_op_num_target_views_minus1[ i ][ j ]
            if (dummy >= 1 << 10)
                return 1;
            for (size_t k = 0; k <= dummy; k++)
                if (extractUEGolombCode() >= 1 << 10)  // applicable_op_target_view_id[ i ][ j ][ k ]
                    return 1;
            if (extractUEGolombCode() >= 1 << 10)  // applicable_op_num_views_minus1[ i ][ j ]
                return 1;
        }
    }
    return 0;
}

void SPSUnit::seq_parameter_set_svc_extension() {}

void SPSUnit::svc_vui_parameters_extension() {}

int SPSUnit::mvc_vui_parameters_extension()
{
    unsigned vui_mvc_num_ops = extractUEGolombCode() + 1;
    if (vui_mvc_num_ops > 1 << 10)
        return 1;
    std::vector<int> vui_mvc_temporal_id;
    vui_mvc_temporal_id.resize(vui_mvc_num_ops);
    mvcHrdParamsBitPos.resize(vui_mvc_num_ops);
    mvcNalHrdParams.resize(vui_mvc_num_ops);
    mvcVclHrdParams.resize(vui_mvc_num_ops);

    // vui_mvc_pic_struct_present_flag.resize(vui_mvc_num_ops);

    for (size_t i = 0; i < vui_mvc_num_ops; i++)
    {
        vui_mvc_temporal_id[i] = bitReader.getBits(3);
        unsigned vui_mvc_num_target_output_views = extractUEGolombCode() + 1;
        if (vui_mvc_num_target_output_views > 1 << 10)
            return 1;
        for (size_t j = 0; j < vui_mvc_num_target_output_views; j++)
            if (extractUEGolombCode() >= 1 << 10)  // vui_mvc_view_id[ i ][ j ]
                return 1;

        int vui_mvc_timing_info_present_flag = bitReader.getBit();  // vui_mvc_timing_info_present_flag[ i ]
        if (vui_mvc_timing_info_present_flag)
        {
            int vui_mvc_num_units_in_tick = bitReader.getBits(32);  // vui_mvc_num_units_in_tick[i]
            bitReader.getBits(32);                                  // vui_mvc_time_scale[ i ]
            bitReader.getBit();                                     // vui_mvc_fixed_frame_rate_flag[ i ]
        }

        mvcHrdParamsBitPos[i] = bitReader.getBitsCount() + 32;
        mvcNalHrdParams[i].isPresent = bitReader.getBit();
        if (mvcNalHrdParams[i].isPresent)
        {
            int beforeCount = bitReader.getBitsCount();
            if (hrd_parameters(mvcNalHrdParams[i]) != 0)
                return 1;
            mvcNalHrdParams[i].bitLen = bitReader.getBitsCount() - beforeCount;
        }
        mvcVclHrdParams[i].isPresent = bitReader.getBit();
        if (mvcVclHrdParams[i].isPresent)
        {
            int beforeCount = bitReader.getBitsCount();
            if (hrd_parameters(mvcVclHrdParams[i]) != 0)
                return 1;
            mvcVclHrdParams[i].bitLen = bitReader.getBitsCount() - beforeCount;
        }
        if (mvcNalHrdParams[i].isPresent || mvcVclHrdParams[i].isPresent)
            bitReader.getBit();                     // vui_mvc_low_delay_hrd_flag[i]
        int picStructpresent = bitReader.getBit();  // vui_mvc_pic_struct_present_flag[i]
        picStructpresent = picStructpresent;
    }
    return 0;
}

// --------------------- SliceUnit -----------------------

SliceUnit::SliceUnit()
    : NALUnit(),
      m_frameNumBitPos(0),
      m_frameNumBits(0),
      m_field_pic_flag(0),
      svc_extension_flag(0),
      non_idr_flag(0),
      anchor_pic_flag(0),
      memory_management_control_operation(0)
{
}

void SliceUnit::nal_unit_header_svc_extension()
{
    non_idr_flag = !bitReader.getBit();  // idr_flag here, use same variable
    int priority_id = bitReader.getBits(6);
    int no_inter_layer_pred_flag = bitReader.getBit();
    int dependency_id = bitReader.getBits(3);
    int quality_id = bitReader.getBits(4);
    int temporal_id = bitReader.getBits(3);
    int use_ref_base_pic_flag = bitReader.getBit();
    int discardable_flag = bitReader.getBit();
    int output_flag = bitReader.getBit();
    int reserved_three_2bits = bitReader.getBits(2);
}

void SliceUnit::nal_unit_header_mvc_extension()
{
    non_idr_flag = bitReader.getBit();
    int priority_id = bitReader.getBits(6);
    int view_id = bitReader.getBits(10);
    int temporal_id = bitReader.getBits(3);
    anchor_pic_flag = bitReader.getBit();
    int inter_view_flag = bitReader.getBit();
    int reserved_one_bit = bitReader.getBit();
}

bool SliceUnit::isIDR() const { return nal_unit_type == nuSliceIDR || nal_unit_type == nuSliceExt && !non_idr_flag; }

bool SliceUnit::isIFrame() const
{
    return nal_unit_type == nuSliceExt ? anchor_pic_flag : slice_type == SliceUnit::I_TYPE;
}

int SliceUnit::deserializeSliceType(uint8_t* buffer, uint8_t* end)
{
    if (end - buffer < 2)
        return NOT_ENOUGH_BUFFER;

    int rez = NALUnit::deserialize(buffer, end);
    if (rez != 0)
        return rez;

    try
    {
        int offset = 1;
        if (nal_unit_type == nuSliceExt)
        {
            if (end - buffer < 5)
                return NOT_ENOUGH_BUFFER;
            offset += 3;
            if (buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 03)
                offset++;  // inplace decode header
            non_idr_flag = buffer[1] & 0x40;
            anchor_pic_flag = buffer[offset - 1] & 0x04;
        }

        bitReader.setBuffer(buffer + offset, end);
        first_mb_in_slice = extractUEGolombCode();
        orig_slice_type = slice_type = extractUEGolombCode();
        if (slice_type > 9)
            return 1;
        if (slice_type > 4)
            slice_type -= 5;  // +5 flag is: all other slice at this picture must be same type

        return 0;
    }
    catch (BitStreamException)
    {
        return NOT_ENOUGH_BUFFER;
    }
};

int SliceUnit::deserialize(uint8_t* buffer, uint8_t* end, const std::map<uint32_t, SPSUnit*>& spsMap,
                           const std::map<uint32_t, PPSUnit*>& ppsMap)
{
    if (end - buffer < 2)
        return NOT_ENOUGH_BUFFER;

    int rez = NALUnit::deserialize(buffer, end);
    if (rez != 0)
        return rez;
    // init_bitReader.getBits( buffer+1, (end - buffer) * 8);

    try
    {
        bitReader.setBuffer(buffer + 1, end);
        // m_getbitContextBuffer = buffer+1;

        if (nal_unit_type == nuSliceExt)
        {
            svc_extension_flag = bitReader.getBit();
            if (svc_extension_flag)
                nal_unit_header_svc_extension();  // specified in Annex G
            else
                nal_unit_header_mvc_extension();
        }

        rez = deserializeSliceHeader(spsMap, ppsMap);
        // if (rez != 0 || m_shortDeserializeMode)
        return rez;
        // return deserializeSliceData();
    }
    catch (BitStreamException)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

/*
int SliceUnit::extractCABAC()
{
}
*/

void NALUnit::updateBits(int bitOffset, int bitLen, int value)
{
    // uint8_t* ptr = m_getbitContextBuffer + (bitOffset/8);
    uint8_t* ptr = (uint8_t*)bitReader.getBuffer() + bitOffset / 8;
    BitStreamWriter bitWriter;
    int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    uint8_t* ptr_end = (uint8_t*)bitReader.getBuffer() + (bitOffset + bitLen) / 8;
    int endBitsPostfix = 8 - ((bitOffset + bitLen) % 8);

    if (byteOffset > 0)
    {
        int prefix = *ptr >> (8 - byteOffset);
        bitWriter.putBits(byteOffset, prefix);
    }
    bitWriter.putBits(bitLen, value);

    if (endBitsPostfix < 8)
    {
        int postfix = *ptr_end & (1 << endBitsPostfix) - 1;
        bitWriter.putBits(endBitsPostfix, postfix);
    }
    bitWriter.flushBits();
}

int SliceUnit::deserializeSliceHeader(const std::map<uint32_t, SPSUnit*>& spsMap,
                                      const std::map<uint32_t, PPSUnit*>& ppsMap)
{
    first_mb_in_slice = extractUEGolombCode();
    orig_slice_type = slice_type = extractUEGolombCode();
    if (slice_type > 9)
        return 1;
    if (slice_type > 4)
        slice_type -= 5;  // +5 flag is: all other slice at this picture must be same type
    pic_parameter_set_id = extractUEGolombCode();
    if (pic_parameter_set_id >= 256)
        return 1;
    std::map<uint32_t, PPSUnit*>::const_iterator itr = ppsMap.find(pic_parameter_set_id);
    if (itr == ppsMap.end())
        return SPS_OR_PPS_NOT_READY;
    pps = itr->second;

    std::map<uint32_t, SPSUnit*>::const_iterator itr2 = spsMap.find(pps->seq_parameter_set_id);
    if (itr2 == spsMap.end())
        return SPS_OR_PPS_NOT_READY;
    sps = itr2->second;

    if (sps->separate_colour_plane_flag)
        bitReader.getBits(2);  // colour_plane_id

    m_frameNumBitPos = bitReader.getBitsCount();  // getBitContext.buffer
    m_frameNumBits = sps->log2_max_frame_num;
    frame_num = bitReader.getBits(m_frameNumBits);
    bottom_field_flag = 0;
    m_field_pic_flag = 0;
    if (sps->frame_mbs_only_flag == 0)
    {
        m_field_pic_flag = bitReader.getBits(1);
        if (m_field_pic_flag)
            bottom_field_flag = bitReader.getBits(1);
    }
    if (isIDR())
    {
        idr_pic_id = extractUEGolombCode();
        if (idr_pic_id >= 1 << 16)
            return 1;
    }
    m_picOrderBitPos = -1;
    if (sps->pic_order_cnt_type == 0)
    {
        m_picOrderBitPos = bitReader.getBitsCount();  // getBitContext.buffer
        m_picOrderNumBits = sps->log2_max_pic_order_cnt_lsb;
        pic_order_cnt_lsb = bitReader.getBits(sps->log2_max_pic_order_cnt_lsb);
        if (pps->pic_order_present_flag && !m_field_pic_flag)
            delta_pic_order_cnt_bottom = extractSEGolombCode();
    }
#if 0
		if (m_shortDeserializeMode)
			return 0;

		if(sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) 
		{
			delta_pic_order_cnt[0] = extractSEGolombCode();
			if( pps->pic_order_present_flag && !m_field_pic_flag)
				delta_pic_order_cnt[1] = extractSEGolombCode();
		}
		if(pps->redundant_pic_cnt_present_flag)
			redundant_pic_cnt = extractUEGolombCode();


		if( slice_type  ==  B_TYPE )
			direct_spatial_mv_pred_flag = bitReader.getBit();
		num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_active_minus1;
		num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_active_minus1;
		if( slice_type == P_TYPE || slice_type == SP_TYPE || slice_type == B_TYPE ) 
		{
			num_ref_idx_active_override_flag = bitReader.getBit();
			if( num_ref_idx_active_override_flag ) {
				num_ref_idx_l0_active_minus1 = extractUEGolombCode();
				if( slice_type == B_TYPE)
					num_ref_idx_l1_active_minus1 = extractUEGolombCode();
			}
		}
		ref_pic_list_reordering();
		if( ( pps->weighted_pred_flag  &&  ( slice_type == P_TYPE  ||  slice_type == SP_TYPE ) )  ||
			( pps->weighted_bipred_idc  ==  1  &&  slice_type  ==  B_TYPE ) )
			pred_weight_table();
		if( nal_ref_idc != 0 )
			dec_ref_pic_marking();
		if( pps->entropy_coding_mode_flag  &&  slice_type  !=  I_TYPE  &&  slice_type  !=  SI_TYPE ) {
			cabac_init_idc = extractUEGolombCode();
			//assert(cabac_init_idc >=0 &&  cabac_init_idc <= 2);
		}
		slice_qp_delta = extractSEGolombCode();
		if( slice_type  ==  SP_TYPE  ||  slice_type  ==  SI_TYPE ) {
			if( slice_type  ==  SP_TYPE )
				sp_for_switch_flag = bitReader.getBits(1);
			slice_qs_delta = extractSEGolombCode();
		}
		if( pps->deblocking_filter_control_present_flag ) {
			disable_deblocking_filter_idc = extractUEGolombCode();
			if( disable_deblocking_filter_idc != 1 ) {
				slice_alpha_c0_offset_div2 = extractSEGolombCode();
				slice_beta_offset_div2 = extractSEGolombCode();
			}
		}
		if( pps->num_slice_groups_minus1 > 0  &&
			pps->slice_group_map_type >= 3  &&  pps->slice_group_map_type <= 5) {
			int bits = ceil_log2( sps->pic_size_in_map_units / (double)pps->slice_group_change_rate + 1 );
			slice_group_change_cycle = bitReader.getBits(bits);
		}

#endif
    return 0;
}

#if 0

void SliceUnit::pred_weight_table()
{
	luma_log2_weight_denom = extractUEGolombCode();
	if( sps->chroma_format_idc  !=  0 )
		chroma_log2_weight_denom = extractUEGolombCode();
	for(int i = 0; i <= num_ref_idx_l0_active_minus1; i++ ) {
		int luma_weight_l0_flag = bitReader.getBit();
		if( luma_weight_l0_flag ) {
			luma_weight_l0.push_back(extractSEGolombCode());
			luma_offset_l0.push_back(extractSEGolombCode());
		}
		else {
			luma_weight_l0.push_back(INT_MAX);
			luma_offset_l0.push_back(INT_MAX);
		}
		if ( sps->chroma_format_idc  !=  0 ) {
			int chroma_weight_l0_flag = bitReader.getBit();
			if( chroma_weight_l0_flag ) {
				for(int j =0; j < 2; j++ ) {
					chroma_weight_l0.push_back(extractSEGolombCode());
					chroma_offset_l0.push_back(extractSEGolombCode());
				}
			}
			else {
				for(int j =0; j < 2; j++ ) {
					chroma_weight_l0.push_back(INT_MAX);
					chroma_offset_l0.push_back(INT_MAX);
				}
			}
		}
	}
	if( slice_type  ==  B_TYPE )
		for(int i = 0; i <= num_ref_idx_l1_active_minus1; i++ ) {
			int luma_weight_l1_flag = bitReader.getBit();
			if( luma_weight_l1_flag ) {
				luma_weight_l1.push_back(extractSEGolombCode());
				luma_offset_l1.push_back(extractSEGolombCode());
			}
			else {
				luma_weight_l1.push_back(INT_MAX);
				luma_offset_l1.push_back(INT_MAX);
			}
			if( sps->chroma_format_idc  !=  0 ) {
				int chroma_weight_l1_flag = bitReader.getBit();
				if( chroma_weight_l1_flag ) {
					for(int j = 0; j < 2; j++ ) {
						chroma_weight_l1.push_back(extractSEGolombCode());
						chroma_offset_l1.push_back(extractSEGolombCode());
					}
				}
				else {
					for(int j = 0; j < 2; j++ ) {
						chroma_weight_l1.push_back(INT_MAX);
						chroma_offset_l1.push_back(INT_MAX);
					}
				}
			}
		}
}

void SliceUnit::dec_ref_pic_marking()
{
	if( nal_unit_type  ==  nuSliceIDR ) {
		no_output_of_prior_pics_flag = bitReader.getBits(1);
		long_term_reference_flag = bitReader.getBits(1);
	} else 
	{
		adaptive_ref_pic_marking_mode_flag =  bitReader.getBits(1);
		if( adaptive_ref_pic_marking_mode_flag )
			do {
				memory_management_control_operation = extractUEGolombCode();
				dec_ref_pic_vector.push_back(memory_management_control_operation);
				if (memory_management_control_operation != 0) {
					uint32_t tmp = extractUEGolombCode();
					if( memory_management_control_operation  ==  1  ||
						memory_management_control_operation  ==  3 )
						int difference_of_pic_nums_minus1 = tmp;
					if(memory_management_control_operation  ==  2  )
						int long_term_pic_num = tmp;
			 		if( memory_management_control_operation  ==  3  ||
						memory_management_control_operation  ==  6 )
						int long_term_frame_idx = tmp;
					if( memory_management_control_operation  ==  4 )
						int max_long_term_frame_idx_plus1 = tmp;
					dec_ref_pic_vector.push_back(tmp);
				}
			} while( memory_management_control_operation  !=  0 );
	}
}

void SliceUnit::ref_pic_list_reordering()
{
	int reordering_of_pic_nums_idc;
	if( slice_type != I_TYPE && slice_type !=  SI_TYPE ) {
		ref_pic_list_reordering_flag_l0 = bitReader.getBit();
		if( ref_pic_list_reordering_flag_l0 )
			do {
				reordering_of_pic_nums_idc = extractUEGolombCode();
				if( reordering_of_pic_nums_idc  ==  0  || reordering_of_pic_nums_idc  ==  1 )
				{
					int tmp = extractUEGolombCode();
					abs_diff_pic_num_minus1 = tmp;
					m_ref_pic_vect.push_back(reordering_of_pic_nums_idc);
					m_ref_pic_vect.push_back(tmp);
				}
				else if( reordering_of_pic_nums_idc  ==  2 ) {
					int tmp = extractUEGolombCode();
					long_term_pic_num = tmp;
					m_ref_pic_vect.push_back(reordering_of_pic_nums_idc);
					m_ref_pic_vect.push_back(tmp);
				}
			} while(reordering_of_pic_nums_idc  !=  3);
	}
	if( slice_type  ==  B_TYPE ) {
		ref_pic_list_reordering_flag_l1 = bitReader.getBit();
		if( ref_pic_list_reordering_flag_l1 )
			do {
				int reordering_of_pic_nums_idc2 = extractUEGolombCode();
				uint32_t tmp = extractUEGolombCode();
				if( reordering_of_pic_nums_idc  ==  0  ||
					reordering_of_pic_nums_idc  ==  1 )
					abs_diff_pic_num_minus1 = tmp;
				else if( reordering_of_pic_nums_idc  ==  2 )
					long_term_pic_num = tmp;
				m_ref_pic_vect2.push_back(reordering_of_pic_nums_idc2);
				m_ref_pic_vect2.push_back(tmp);
			} while(reordering_of_pic_nums_idc  !=  3);
	}
}

int SliceUnit::NextMbAddress(int n)
{
	int FrameHeightInMbs = ( 2 - sps->frame_mbs_only_flag ) * sps->pic_height_in_map_units;
	int FrameWidthInMbs = ( 2 - sps->frame_mbs_only_flag ) * sps->pic_width_in_mbs;
	int PicHeightInMbs = FrameHeightInMbs / ( 1 + m_field_pic_flag );
	int PicWidthInMbs = FrameWidthInMbs / ( 1 + m_field_pic_flag );
	int PicSizeInMbs = PicWidthInMbs * PicHeightInMbs;
	int i = n + 1;
	//while( i < PicSizeInMbs  &&  MbToSliceGroupMap[ i ]  !=  MbToSliceGroupMap[ n ] )
	//	i++;
	return i;
}

void SliceUnit::setFrameNum(int frameNum)
{
	assert(m_frameNumBitPos != 0);
	updateBits(m_frameNumBitPos, m_frameNumBits, frameNum);
	if (m_picOrderBitPos > 0)
		updateBits(m_picOrderBitPos, m_picOrderNumBits, frameNum*2 + bottom_field_flag);
}

int SliceUnit::deserializeSliceData()
{
	//if (nal_unit_type != P_TYPE && nal_unit_type != I_TYPE && nal_unit_type != B_TYPE)
	if (nal_unit_type != nuSliceIDR && nal_unit_type != nuSliceNonIDR)
		return 0; // deserialize other nal types are not supported now
	if( pps->entropy_coding_mode_flag) {
		int bitOffs = bitReader.getBitsCount() % 8;
		if (bitOffs > 0)
			bitReader.skipBits(8 - bitOffs);
	}
	int MbaffFrameFlag = ( sps->mb_adaptive_frame_field_flag!=0  &&  m_field_pic_flag==0);
	int CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag );
	int moreDataFlag = 1;
	int mb_skip_run = 0;
	int mb_skip_flag = 0;
	int prevMbSkipped = 0;
	int mb_field_decoding_flag = 0;
	do {
		if( slice_type  !=  I_TYPE  &&  slice_type  !=  SI_TYPE )
			if( !pps->entropy_coding_mode_flag ) {
				mb_skip_run = extractUEGolombCode(); // !!!!!!!!!!!!!
				int prevMbSkipped = ( mb_skip_run > 0 );
				for(int i = 0; i<mb_skip_run; i++ )
					CurrMbAddr = NextMbAddress( CurrMbAddr );
				moreDataFlag = bitReader.getBitsLeft() >= 8;
			} else {
				/*
				mb_skip_flag = extractCABAC();
				moreDataFlag = mb_skip_flag == 0;
				*/
			}
		if( moreDataFlag ) {
			if( MbaffFrameFlag && ( CurrMbAddr % 2  ==  0  ||  
				( CurrMbAddr % 2  ==  1  &&  prevMbSkipped ) ) ) {
					mb_field_decoding_flag = bitReader.getBit(); // || ae(v) for CABAC
			}
			macroblock_layer();
		}
		if( !pps->entropy_coding_mode_flag )
			moreDataFlag = bitReader.getBitsLeft() >= 8; //more_rbsp_data();
		else {
			/*
			if( slice_type  !=  I  &&  slice_type  !=  SI )
				prevMbSkipped = mb_skip_flag
			if( MbaffFrameFlag  &&  CurrMbAddr % 2  ==  0 )
				moreDataFlag = 1
			else {
				end_of_slice_flag
				moreDataFlag = !end_of_slice_flag
			}
			*/
		}
		CurrMbAddr = NextMbAddress( CurrMbAddr );
	} while( moreDataFlag);
	return 0;
}

void SliceUnit::macroblock_layer()
{
}

int SliceUnit::serializeSliceHeader(BitStreamWriter& bitWriter, const std::map<uint32_t, SPSUnit*>& spsMap,
                                    const std::map<uint32_t, PPSUnit*>& ppsMap, uint8_t* dstBuffer, int dstBufferLen)
{
	try 
	{
		dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = 0;
		dstBuffer[3] = 1;
		dstBuffer[4] = (nal_ref_idc << 5) + nal_unit_type;
		bitWriter.setBuffer(dstBuffer + 5, dstBuffer + dstBufferLen);
		bitReader.setBuffer(dstBuffer + 5, dstBuffer + dstBufferLen);
		writeUEGolombCode(bitWriter, first_mb_in_slice);
		writeUEGolombCode(bitWriter, orig_slice_type);
		writeUEGolombCode(bitWriter, pic_parameter_set_id);
		std::map<uint32_t, PPSUnit*>::const_iterator itr = ppsMap.find(pic_parameter_set_id);
		if (itr == ppsMap.end())
			return SPS_OR_PPS_NOT_READY;
		pps = itr->second;

		std::map<uint32_t, SPSUnit*>::const_iterator itr2 = spsMap.find(pps->seq_parameter_set_id);
		if (itr2 == spsMap.end())
			return SPS_OR_PPS_NOT_READY;
		sps = itr2->second;
		m_frameNumBitPos = bitWriter.getBitsCount(); //getBitContext.buffer
		m_frameNumBits = sps->log2_max_frame_num;
		bitWriter.putBits(m_frameNumBits, frame_num);
		if( sps->frame_mbs_only_flag == 0) {
			bitWriter.putBit(m_field_pic_flag);
			if( m_field_pic_flag )
				bitWriter.putBit(bottom_field_flag);
		}
		if( isIDR())
			writeUEGolombCode(bitWriter, idr_pic_id);
		if( sps->pic_order_cnt_type ==  0) 
		{
			m_picOrderBitPos = bitWriter.getBitsCount(); //getBitContext.buffer
			m_picOrderNumBits = sps->log2_max_pic_order_cnt_lsb;
			bitWriter.putBits( sps->log2_max_pic_order_cnt_lsb, pic_order_cnt_lsb);
			if( pps->pic_order_present_flag &&  !m_field_pic_flag)
				writeUEGolombCode(bitWriter, delta_pic_order_cnt_bottom);
		}
		assert (m_shortDeserializeMode == false);


		if(sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) 
		{
			writeSEGolombCode(bitWriter, delta_pic_order_cnt[0]);
			if( pps->pic_order_present_flag && !m_field_pic_flag)
				writeSEGolombCode(bitWriter, delta_pic_order_cnt[1]);
		}
		if(pps->redundant_pic_cnt_present_flag)
			writeSEGolombCode(bitWriter, redundant_pic_cnt);


		if( slice_type  ==  B_TYPE ) {
			bitWriter.putBit(direct_spatial_mv_pred_flag);
		}
		if( slice_type == P_TYPE || slice_type == SP_TYPE || slice_type == B_TYPE ) 
		{
			bitWriter.putBit(num_ref_idx_active_override_flag);
			if( num_ref_idx_active_override_flag ) {
				writeUEGolombCode(bitWriter, num_ref_idx_l0_active_minus1);
				if( slice_type == B_TYPE)
					writeUEGolombCode(bitWriter, num_ref_idx_l1_active_minus1);
			}
		}
		write_ref_pic_list_reordering(bitWriter);
		if( ( pps->weighted_pred_flag  &&  ( slice_type == P_TYPE  ||  slice_type == SP_TYPE ) )  ||
			( pps->weighted_bipred_idc  ==  1  &&  slice_type  ==  B_TYPE ) )
			write_pred_weight_table(bitWriter);
		if( nal_ref_idc != 0 )
			write_dec_ref_pic_marking(bitWriter);
		// ------------------------

		if( pps->entropy_coding_mode_flag  &&  slice_type  !=  I_TYPE  &&  slice_type  !=  SI_TYPE ) {
			writeUEGolombCode(bitWriter, cabac_init_idc);
		}
		writeSEGolombCode(bitWriter, slice_qp_delta);
		if( slice_type  ==  SP_TYPE  ||  slice_type  ==  SI_TYPE ) {
			if( slice_type  ==  SP_TYPE )
				bitWriter.putBit(sp_for_switch_flag);
			writeSEGolombCode(bitWriter, slice_qs_delta);
		}
		if( pps->deblocking_filter_control_present_flag ) {
			writeUEGolombCode(bitWriter, disable_deblocking_filter_idc); 
			if( disable_deblocking_filter_idc != 1 ) {
				writeSEGolombCode(bitWriter, slice_alpha_c0_offset_div2);
				writeSEGolombCode(bitWriter, slice_beta_offset_div2);
			}
		}
		if( pps->num_slice_groups_minus1 > 0  &&
			pps->slice_group_map_type >= 3  &&  pps->slice_group_map_type <= 5) {
			int bits = ceil_log2( sps->pic_size_in_map_units / (double)pps->slice_group_change_rate + 1 );
			bitWriter.putBits(bits, slice_group_change_cycle);
		}

		return 0;
	} catch(BitStreamException& e) {
		return NOT_ENOUGH_BUFFER;
	}
}


void SliceUnit::write_dec_ref_pic_marking(BitStreamWriter& bitWriter)
{
	if( nal_unit_type  ==  5 ) {
		bitWriter.putBit(no_output_of_prior_pics_flag);
		bitWriter.putBit(long_term_reference_flag);
	} else 
	{
		bitWriter.putBit(adaptive_ref_pic_marking_mode_flag);
		if( adaptive_ref_pic_marking_mode_flag )
			for (int i = 0; i < dec_ref_pic_vector.size(); i++)
				writeUEGolombCode(bitWriter, dec_ref_pic_vector[i]);
	}
}


void SliceUnit::write_pred_weight_table(BitStreamWriter& bitWriter)
{
	writeUEGolombCode(bitWriter, luma_log2_weight_denom);
	if( sps->chroma_format_idc  !=  0 )
		writeUEGolombCode(bitWriter, chroma_log2_weight_denom);
	for(int i = 0; i <= num_ref_idx_l0_active_minus1; i++ ) 
	{
		if (luma_weight_l0[i] != INT_MAX) {
			bitWriter.putBit(1);
			writeSEGolombCode(bitWriter, luma_weight_l0[i]);
			writeSEGolombCode(bitWriter, luma_offset_l0[i]);
		}
		else
			bitWriter.putBit(0);
		if ( sps->chroma_format_idc  !=  0 ) {
			if (chroma_weight_l0[i*2] != INT_MAX) {
				bitWriter.putBit(1);
				for(int j =0; j < 2; j++ ) {
					writeSEGolombCode(bitWriter, chroma_weight_l0[i*2+j]);
					writeSEGolombCode(bitWriter, chroma_offset_l0[i*2+j]);
				}
			}
			else {
				bitWriter.putBit(0);
			}
		}
	}

	if( slice_type  ==  B_TYPE )
		for(int i = 0; i <= num_ref_idx_l1_active_minus1; i++ ) {
			if (luma_weight_l1[i] != INT_MAX) {
				bitWriter.putBit(1);
				writeSEGolombCode(bitWriter, luma_weight_l1[i]);
				writeSEGolombCode(bitWriter, luma_offset_l1[i]);
			}
			else
				bitWriter.putBit(0);

			if( sps->chroma_format_idc  !=  0 ) {
				if (chroma_weight_l1[i*2] != INT_MAX) {
					bitWriter.putBit(1);
					for(int j = 0; j < 2; j++ ) {
						writeSEGolombCode(bitWriter, chroma_weight_l1[i*2+j]);
						writeSEGolombCode(bitWriter, chroma_offset_l1[i*2+j]);
					}
				}
				else
					bitWriter.putBit(0);
				// -------------
			}
		}
}

void SliceUnit::write_ref_pic_list_reordering(BitStreamWriter& bitWriter)
{

	if( slice_type != I_TYPE && slice_type !=  SI_TYPE ) {
		bitWriter.putBit(ref_pic_list_reordering_flag_l0);
		for (int i = 0; i < m_ref_pic_vect.size(); i++)
			writeUEGolombCode(bitWriter, m_ref_pic_vect[i]);
	}

	if( slice_type  ==  B_TYPE ) {
		bitWriter.putBit(ref_pic_list_reordering_flag_l1);
		if( ref_pic_list_reordering_flag_l1 )
			for (int i = 0; i < m_ref_pic_vect2.size(); i++)
				writeUEGolombCode(bitWriter, m_ref_pic_vect2[i]);
	}
}

#endif

// --------------- SEI UNIT ------------------------
void SEIUnit::deserialize(SPSUnit& sps, int orig_hrd_parameters_present_flag)
{
    pic_struct = -1;

    uint8_t* nalEnd = m_nalBuffer + m_nalBufferLen;
    try
    {
        int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
        if (rez != 0)
            return;
        uint8_t* curBuff = m_nalBuffer + 1;
        while (curBuff < nalEnd - 1)
        {
            int payloadType = 0;
            for (; *curBuff == 0xFF && curBuff < nalEnd; curBuff++) payloadType += 0xFF;
            if (curBuff >= nalEnd)
                return;
            payloadType += *curBuff++;
            if (curBuff >= nalEnd)
                return;

            int payloadSize = 0;
            for (; *curBuff == 0xFF && curBuff < nalEnd; curBuff++) payloadSize += 0xFF;
            if (curBuff >= nalEnd)
                return;
            payloadSize += *curBuff++;
            if (curBuff >= nalEnd)
                return;
            sei_payload(sps, payloadType, curBuff, payloadSize, orig_hrd_parameters_present_flag);
            m_processedMessages.insert(payloadType);
            curBuff += payloadSize;
        }
    }
    catch (BitStreamException)
    {
        LTRACE(LT_WARN, 2, "Bad SEI detected. SEI too short");
    }
    return;
}

int SEIUnit::isMVCSEI()
{
    pic_struct = -1;

    uint8_t* nalEnd = m_nalBuffer + m_nalBufferLen;
    try
    {
        int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
        if (rez != 0)
            return NOT_ENOUGH_BUFFER;
        uint8_t* curBuff = m_nalBuffer + 1;
        while (curBuff < nalEnd - 1)
        {
            int payloadType = 0;
            for (; *curBuff == 0xFF && curBuff < nalEnd; curBuff++) payloadType += 0xFF;
            if (curBuff >= nalEnd)
                return NOT_ENOUGH_BUFFER;
            payloadType += *curBuff++;
            if (curBuff >= nalEnd)
                return NOT_ENOUGH_BUFFER;

            int payloadSize = 0;
            for (; *curBuff == 0xFF && curBuff < nalEnd; curBuff++) payloadSize += 0xFF;
            if (curBuff >= nalEnd)
                return NOT_ENOUGH_BUFFER;
            payloadSize += *curBuff++;
            if (curBuff >= nalEnd)
                return NOT_ENOUGH_BUFFER;
            if (payloadType == 37)
                return 1;  // mvc scalable nesting message
            curBuff += payloadSize;
        }
    }
    catch (BitStreamException)
    {
        ;
    }
    return 0;
}

int SEIUnit::removePicTimingSEI(SPSUnit& sps)
{
    uint8_t* nalEnd = m_nalBuffer + m_nalBufferLen;
    uint8_t* curBuff = m_nalBuffer + 1;
    uint8_t tmpBuffer[1024 * 4];
    tmpBuffer[0] = m_nalBuffer[0];
    int tmpBufferLen = 1;

    while (curBuff < nalEnd)
    {
        int payloadType = 0;
        for (; *curBuff == 0xFF && curBuff < nalEnd; curBuff++)
        {
            payloadType += 0xFF;
            tmpBuffer[tmpBufferLen++] = 0xff;
        }
        if (curBuff >= nalEnd)
            break;
        payloadType += *curBuff++;
        tmpBuffer[tmpBufferLen++] = payloadType;
        if (curBuff >= nalEnd)
            break;

        int payloadSize = 0;
        for (; *curBuff == 0xFF && curBuff < nalEnd; curBuff++)
        {
            payloadSize += 0xFF;
            tmpBuffer[tmpBufferLen++] = 0xff;
        }
        if (curBuff >= nalEnd)
            break;

        payloadSize += *curBuff++;
        tmpBuffer[tmpBufferLen++] = payloadSize;
        if (curBuff >= nalEnd)
            break;
        if (payloadType == SEI_MSG_PIC_TIMING)
        {
            tmpBufferLen -= 2;  // skip this sei message
        }
        else if (payloadType == SEI_MSG_BUFFERING_PERIOD)
        {
            tmpBufferLen -= 2;  // skip this sei message
        }
        else
        {
            memcpy(tmpBuffer + tmpBufferLen, curBuff, payloadSize);
            tmpBufferLen += payloadSize;
        }
        curBuff += payloadSize;
    }
    if (m_nalBufferLen < tmpBufferLen)
    {
        // assert(m_nalBufferLen < tmpBufferLen);
        delete[] m_nalBuffer;
        m_nalBuffer = new uint8_t[tmpBufferLen];
    }
    memcpy(m_nalBuffer, tmpBuffer, tmpBufferLen);
    m_nalBufferLen = tmpBufferLen;
    return tmpBufferLen;
}

void SEIUnit::sei_payload(SPSUnit& sps, int payloadType, uint8_t* curBuff, int payloadSize,
                          int orig_hrd_parameters_present_flag)
{
    if (payloadType == 0)
        buffering_period(payloadSize);
    else if (payloadType == 1)
        pic_timing(sps, curBuff, payloadSize, orig_hrd_parameters_present_flag);
    else if (payloadType == 2)
        pan_scan_rect(payloadSize);
    else if (payloadType == 3)
        filler_payload(payloadSize);
    else if (payloadType == 4)
        user_data_registered_itu_t_t35(payloadSize);
    else if (payloadType == 5)
        user_data_unregistered(payloadSize);
    else if (payloadType == 6)
        recovery_point(payloadSize);
    else if (payloadType == 7)
        dec_ref_pic_marking_repetition(payloadSize);
    else if (payloadType == 8)
        spare_pic(payloadSize);
    else if (payloadType == 9)
        scene_info(payloadSize);
    else if (payloadType == 10)
        sub_seq_info(payloadSize);
    else if (payloadType == 11)
        sub_seq_layer_characteristics(payloadSize);
    else if (payloadType == 12)
        sub_seq_characteristics(payloadSize);
    else if (payloadType == 13)
        full_frame_freeze(payloadSize);
    else if (payloadType == 14)
        full_frame_freeze_release(payloadSize);
    else if (payloadType == 15)
        full_frame_snapshot(payloadSize);
    else if (payloadType == 16)
        progressive_refinement_segment_start(payloadSize);
    else if (payloadType == 17)
        progressive_refinement_segment_end(payloadSize);
    else if (payloadType == 18)
        motion_constrained_slice_group_set(payloadSize);
    else if (payloadType == 19)
        film_grain_characteristics(payloadSize);
    else if (payloadType == 20)
        deblocking_filter_display_preference(payloadSize);
    else if (payloadType == 21)
        stereo_video_info(payloadSize);
    else if (payloadType == 37)
        mvc_scalable_nesting(sps, curBuff, payloadSize, orig_hrd_parameters_present_flag);
    else
        reserved_sei_message(payloadSize);
    /*
    if( !byte_aligned( ) ) {
    bit_equal_to_one  // equal to 1
    while( !byte_aligned( ) )
    bit_equal_to_zero  // equal to 0
    }
    */
}

void SEIUnit::buffering_period(int payloadSize) {}

int SEIUnit::getNumClockTS(int pic_struct) const
{
    int NumClockTS = 0;
    switch (pic_struct)
    {
    case 0:
    case 1:
    case 2:
        NumClockTS = 1;
        break;
    case 3:
    case 4:
    case 7:
        NumClockTS = 2;
        break;
    case 5:
    case 6:
    case 8:
        NumClockTS = 3;
        break;
    }
    return NumClockTS;
}

void SEIUnit::serialize_pic_timing_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader)
{
    if (seiHeader)
    {
        writer.putBits(8, nuSEI);
        writer.putBits(8, SEI_MSG_PIC_TIMING);
    }
    uint8_t* size = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);
    int beforeMessageLen = writer.getBitsCount();
    // pic timing
    if (sps.nalHrdParams.isPresent || sps.vclHrdParams.isPresent)
    {
        m_cpb_removal_delay_baseaddr = writer.getBuffer();
        m_cpb_removal_delay_bitpos = writer.getBitsCount();
        writer.putBits(sps.nalHrdParams.cpb_removal_delay_length_minus1 + 1, cpb_removal_delay);
        writer.putBits(sps.nalHrdParams.dpb_output_delay_length_minus1 + 1, dpb_output_delay);
    }

    // if (sps.isPictStructExist())
    if (sps.pic_struct_present_flag)
    {
        writer.putBits(4, pic_struct);
        int NumClockTS = getNumClockTS(pic_struct);
        for (int i = 0; i < NumClockTS; i++) writer.putBit(0);  // clock_timestamp_flag
        if (sps.nalHrdParams.time_offset_length > 0)
            writer.putBits(sps.nalHrdParams.time_offset_length, 0);
    }
    write_byte_align_bits(writer);
    // ---------
    int msgLen = writer.getBitsCount() - beforeMessageLen;
    *size = msgLen / 8;
    if (seiHeader)
    {
        write_rbsp_trailing_bits(writer);
    }
}

void SEIUnit::serialize_buffering_period_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader)
{
    if (seiHeader)
    {
        writer.putBits(8, nuSEI);
        writer.putBits(8, SEI_MSG_BUFFERING_PERIOD);
    }
    uint8_t* size = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);
    int beforeMessageLen = writer.getBitsCount();
    // buffering period
    writeUEGolombCode(writer, sps.seq_parameter_set_id);
    if (sps.nalHrdParams.isPresent)
    {  // NalHrdBpPresentFlag
        for (size_t SchedSelIdx = 0; SchedSelIdx <= sps.nalHrdParams.cpb_cnt_minus1; SchedSelIdx++)
        {
            writer.putBits(sps.nalHrdParams.initial_cpb_removal_delay_length_minus1 + 1,
                           initial_cpb_removal_delay[SchedSelIdx]);
            writer.putBits(sps.nalHrdParams.initial_cpb_removal_delay_length_minus1 + 1,
                           initial_cpb_removal_delay_offset[SchedSelIdx]);
        }
    }
    if (sps.vclHrdParams.isPresent)
    {  // NalHrdBpPresentFlag
        for (size_t SchedSelIdx = 0; SchedSelIdx <= sps.vclHrdParams.cpb_cnt_minus1; SchedSelIdx++)
        {
            writer.putBits(sps.vclHrdParams.initial_cpb_removal_delay_length_minus1 + 1,
                           initial_cpb_removal_delay[SchedSelIdx]);
            writer.putBits(sps.vclHrdParams.initial_cpb_removal_delay_length_minus1 + 1,
                           initial_cpb_removal_delay_offset[SchedSelIdx]);
        }
    }
    write_byte_align_bits(writer);
    // ---------
    int msgLen = writer.getBitsCount() - beforeMessageLen;
    *size = msgLen / 8;
    if (seiHeader)
    {
        write_rbsp_trailing_bits(writer);
    }
}

void SEIUnit::pic_timing(SPSUnit& sps, uint8_t* curBuff, int payloadSize, bool orig_hrd_parameters_present_flag)
{
    bitReader.setBuffer(curBuff, curBuff + payloadSize);
    pic_timing(sps, orig_hrd_parameters_present_flag);
}

void SEIUnit::pic_timing(SPSUnit& sps, bool orig_hrd_parameters_present_flag)
{
    bool CpbDpbDelaysPresentFlag = orig_hrd_parameters_present_flag == 1;
    cpb_removal_delay = dpb_output_delay = 0;
    if (CpbDpbDelaysPresentFlag)
    {
        cpb_removal_delay = bitReader.getBits(sps.nalHrdParams.cpb_removal_delay_length_minus1 + 1);
        dpb_output_delay = bitReader.getBits(sps.nalHrdParams.dpb_output_delay_length_minus1 + 1);

        // LTRACE(LT_INFO, 2, "got PicTimings " << cpb_removal_delay << "x" << dpb_output_delay);
    }
    if (sps.pic_struct_present_flag)
    {
        pic_struct = bitReader.getBits(4);
        int numClockTS = getNumClockTS(pic_struct);

        for (int i = 0; i < numClockTS; i++)
        {
            int Clock_timestamp_flag = bitReader.getBit();
        }

        /*

        for( int i = 0; i < numClockTS; i++ ) {
                Clock_timestamp_flag[i] = bitReader.getBit();
                if( clock_timestamp_flag[i] ) {
                        Ct_type	= bitReader.getBits(2);
                        nuit_field_based_flag = bitReader.getBit();
                        counting_type bitReader.getBits(5);
                        full_timestamp_flag	= bitReader.getBit();
                        discontinuity_flag	= bitReader.getBit();
                        cnt_dropped_flag = bitReader.getBit();
                        n_frames = bitReader.getBits(8);
                        if( full_timestamp_flag ) {
                                seconds_value = bitReader.getBits(6);
                                minutes_value = bitReader.getBits(6);
                                hours_value = bitReader.getBits(5);
                        } else {
                                seconds_flag = bitReader.getBit();
                                if( seconds_flag ) {
                                        seconds_value = bitReader.getBits(6);
                                        minutes_flag = bitReader.getBit();
                                        if( minutes_flag ) {
                                                minutes_value = bitReader.getBits(6);
                                                hours_flag = bitReader.getBit();
                                                if( hours_flag )
                                                        hours_value = bitReader.getBits(5);
                                        }
                                }
                        }
                        if( time_offset_length > 0 ) {
                                //time_offset	= bitReader.getBits(time_offset_length); // signed bits!
                        }
                }
        }
        */
    }
}

/*
int getNumClockTS()
{
        if (field_pic_flag == 0)
                return 0;
        if (field_pic_flag == 0 && bottom_field_flag == 0)
                return 1;
}
*/

void SEIUnit::pan_scan_rect(int payloadSize) {}
void SEIUnit::filler_payload(int payloadSize) {}
void SEIUnit::user_data_registered_itu_t_t35(int payloadSize) {}
void SEIUnit::user_data_unregistered(int payloadSize) {}
void SEIUnit::recovery_point(int payloadSize) {}
void SEIUnit::dec_ref_pic_marking_repetition(int payloadSize) {}
void SEIUnit::spare_pic(int payloadSize) {}
void SEIUnit::scene_info(int payloadSize) {}
void SEIUnit::sub_seq_info(int payloadSize) {}
void SEIUnit::sub_seq_layer_characteristics(int payloadSize) {}
void SEIUnit::sub_seq_characteristics(int payloadSize) {}
void SEIUnit::full_frame_freeze(int payloadSize) {}
void SEIUnit::full_frame_freeze_release(int payloadSize) {}
void SEIUnit::full_frame_snapshot(int payloadSize) {}
void SEIUnit::progressive_refinement_segment_start(int payloadSize) {}
void SEIUnit::progressive_refinement_segment_end(int payloadSize) {}
void SEIUnit::motion_constrained_slice_group_set(int payloadSize) {}
void SEIUnit::film_grain_characteristics(int payloadSize) {}
void SEIUnit::deblocking_filter_display_preference(int payloadSize) {}
void SEIUnit::stereo_video_info(int payloadSize) {}
void SEIUnit::reserved_sei_message(int payloadSize) {}

int SEIUnit::mvc_scalable_nesting(SPSUnit& sps, uint8_t* curBuf, int size, int orig_hrd_parameters_present_flag)
{
    try
    {
        bitReader.setBuffer(curBuf, curBuf + size);
        int operation_point_flag = bitReader.getBit();
        if (!operation_point_flag)
        {
            int all_view_components_in_au_flag = bitReader.getBit();
            if (!all_view_components_in_au_flag)
            {
                unsigned num_view_components_minus1 = extractUEGolombCode();
                if (num_view_components_minus1 >= 1 << 10)
                    return 1;
                for (size_t i = 0; i <= num_view_components_minus1; i++) bitReader.getBits(10);  // sei_view_id[ i ]
            }
        }
        else
        {
            unsigned num_view_components_op_minus1 = extractUEGolombCode();
            if (num_view_components_op_minus1 >= 1 << 10)
                return 1;
            for (size_t i = 0; i <= num_view_components_op_minus1; i++)
            {
                int sei_op_view_id = bitReader.getBits(10);     // sei_op_view_id[ i ]
                int sei_op_temporal_id = bitReader.getBits(3);  // sei_op_temporal_id
                sei_op_temporal_id = sei_op_temporal_id;
            }
        }
        int byteBits = bitReader.getBitsCount() % 8;
        if (byteBits)
            bitReader.skipBits(8 - byteBits);  // byte align

        m_mvcHeaderStart = bitReader.getBuffer();
        m_mvcHeaderLen = bitReader.getBitsCount() / 8;

        int payloadType = bitReader.getBits(8);
        int payloadSize = 0;
        uint8_t sizePart = 0;
        do
        {
            sizePart = bitReader.getBits(8);
            payloadSize += sizePart;
        } while (sizePart == 255);

        if (payloadType == 5)  // user unregister SEI payload data
        {
            if (bitReader.getBitsLeft() >= 128)
            {
                uint8_t* bdData = curBuf + bitReader.getBitsCount() / 8;
                if (memcmp(bdData, BDROM_METADATA_GUID, 128 / 8) == 0)
                {
                    // process bd rom meta data
                    for (int i = 0; i < 4; ++i) bitReader.skipBits(32);
                    int type_indicator = bitReader.getBits(32);
                    switch (type_indicator)
                    {
                    case 0x4F464D44:
                        processBlurayOffsetMetadata();
                        break;
                    case 0x47534D50:
                        processBlurayGopStructure();
                        break;
                    }
                }
            }
        }
        if (payloadType == 0)
        {
            /*
        int seq_parameter_set_id = extractUEGolombCode();
        int initialDelay = bitReader.getBits(24);
        int initialDelayOffset = bitReader.getBits(24);

        LTRACE(LT_INFO, 2, "got BufferingPeriod " << initialDelay << "x" << initialDelayOffset);
            */
            buffering_period(payloadSize);
            m_processedMessages.insert(payloadType);
        }
        else if (payloadType == 1)
        {
            pic_timing(sps, orig_hrd_parameters_present_flag);
            m_processedMessages.insert(payloadType);
        }
    }
    catch (...)
    {
    }
    return 0;
}

void SEIUnit::processBlurayGopStructure() {}

void SEIUnit::processBlurayOffsetMetadata()
{
    bitReader.skipBits(8);
    uint8_t* ptr = bitReader.getBuffer() + bitReader.getBitsCount() / 8;
    metadataPtsOffset = ptr - m_nalBuffer;
    bitReader.skipBits(24);  // PTS[32..30], marker_bit, PTS[29..15]
    bitReader.skipBits(18);  // skip: marker_bit, PTS[14..0], marker_bit, reserved_for_future_use bit
    number_of_offset_sequences = bitReader.getBits(6);
}

void SEIUnit::updateMetadataPts(uint8_t* metadataPtsPtr, int64_t pts)
{
    metadataPtsPtr[0] = (pts >> 30) & 0x07;

    uint16_t val = (uint16_t)((pts >> 15) & 0x7fff);
    metadataPtsPtr[1] = 0x80 + (val >> 8);
    metadataPtsPtr[2] = val & 0xff;

    val = (uint16_t)(pts & 0x7fff);
    metadataPtsPtr[3] = 0x80 + (val >> 8);
    metadataPtsPtr[4] = val & 0xff;
}
