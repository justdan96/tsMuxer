
#include <fs/systemlog.h>

#include <cmath>
#include <cstring>
#include <sstream>

#include "bitStream.h"
#include "nalUnits.h"
#include "vod_common.h"

static constexpr uint8_t BDROM_METADATA_GUID[] = "\x17\xee\x8c\x60\xf8\x4d\x11\xd9\x8c\xd6\x08\x00\x20\x0c\x9a\x66";

void NALUnit::write_rbsp_trailing_bits(BitStreamWriter& writer)
{
    writer.putBit(1);
    const unsigned rest = 8 - (writer.getBitsCount() & 7);
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
        return static_cast<int>(end - nalBuffer) * 8 - trailing;
    }
    return 0;
}

void NALUnit::write_byte_align_bits(BitStreamWriter& writer)
{
    const unsigned rest = 8 - (writer.getBitsCount() & 7);
    if (rest == 8)
        return;
    writer.putBit(1);
    if (rest > 1)
        writer.putBits(rest - 1, 0);
}

uint8_t* NALUnit::addStartCode(uint8_t* buffer, const uint8_t* boundStart)
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
        else  // *buffer == 1
        {
            if (buffer[-2] == 0 && buffer[-1] == 0)
                return buffer + 1;
            buffer += 3;
        }
    }
    return end;
}

uint8_t* NALUnit::findNALWithStartCode(uint8_t* buffer, uint8_t* end, const bool longCodesAllowed)
{
    const uint8_t* bufStart = buffer;
    for (buffer += 2; buffer < end;)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer++;
        else  // *buffer == 1
        {
            if (buffer[-2] == 0 && buffer[-1] == 0)
            {
                if (longCodesAllowed && buffer - 3 >= bufStart && buffer[-3] == 0)
                    return buffer - 3;
                return buffer - 2;
            }
            buffer += 3;
        }
    }
    return end;
}

int NALUnit::encodeNAL(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize)
{
    const uint8_t* srcStart = srcBuffer;
    const uint8_t* initDstBuffer = dstBuffer;
    for (srcBuffer += 2; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 3;
        else if (srcBuffer[-2] == 0 && srcBuffer[-1] == 0)
        {
            if (dstBufferSize < static_cast<size_t>(srcBuffer - srcStart + 2))
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
    if (dstBufferSize < static_cast<size_t>(srcEnd - srcStart))
        return -1;
    memcpy(dstBuffer, srcStart, srcEnd - srcStart);
    dstBuffer += srcEnd - srcStart;
    return static_cast<int>(dstBuffer - initDstBuffer);
}

int NALUnit::decodeNAL(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize)
{
    const uint8_t* initDstBuffer = dstBuffer;
    const uint8_t* srcStart = srcBuffer;
    for (srcBuffer += 3; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 4;
        else if (srcBuffer[-3] == 0 && srcBuffer[-2] == 0 && srcBuffer[-1] == 3)
        {
            if (dstBufferSize < static_cast<size_t>(srcBuffer - srcStart))
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
    return static_cast<int>(dstBuffer - initDstBuffer);
}

int NALUnit::decodeNAL2(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize,
                        bool* keepSrcBuffer)
{
    const uint8_t* initDstBuffer = dstBuffer;
    const uint8_t* srcStart = srcBuffer;
    *keepSrcBuffer = true;
    for (srcBuffer += 3; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 4;
        else if (srcBuffer[-3] == 0 && srcBuffer[-2] == 0 && srcBuffer[-1] == 3)
        {
            if (dstBufferSize < static_cast<size_t>(srcBuffer - srcStart))
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
    return static_cast<int>(dstBuffer - initDstBuffer);
}

unsigned NALUnit::extractUEGolombCode(uint8_t* buffer, const uint8_t* bufEnd)
{
    BitStreamReader reader{};
    reader.setBuffer(buffer, bufEnd);
    return extractUEGolombCode(reader);
}

unsigned NALUnit::extractUEGolombCode()
{
    unsigned cnt = 0;
    for (; bitReader.getBit() == 0; cnt++)
        ;
    if (cnt > INT_BIT)
        THROW_BITSTREAM_ERR;
    return (1 << cnt) - 1 + bitReader.getBits(cnt);
}

void NALUnit::writeSEGolombCode(BitStreamWriter& bitWriter, const int32_t value)
{
    if (value <= 0)
        writeUEGolombCode(bitWriter, -value * 2);
    else
        writeUEGolombCode(bitWriter, value * 2 - 1);
}

void NALUnit::writeUEGolombCode(BitStreamWriter& bitWriter, const uint32_t value)
{
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
    for (; bitReader.getBit() == 0; cnt++)
        ;
    return (1 << cnt) - 1 + bitReader.getBits(cnt);
}

int NALUnit::extractSEGolombCode()
{
    const unsigned rez = extractUEGolombCode();
    if (rez % 2 == 0)
        return -static_cast<int>(rez / 2);
    return static_cast<int>((rez + 1) / 2);
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
    nal_unit_type = static_cast<NALType>(*buffer & 0x1f);
    return 0;
}

void NALUnit::decodeBuffer(const uint8_t* buffer, const uint8_t* end)
{
    delete[] m_nalBuffer;
    m_nalBuffer = new uint8_t[end - buffer];
    m_nalBufferLen = decodeNAL(buffer, end, m_nalBuffer, end - buffer);
}

int NALUnit::serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd, const bool writeStartCode) const
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
    const int encodeRez = encodeNAL(m_nalBuffer, m_nalBuffer + m_nalBufferLen, dstBuffer, dstEnd - dstBuffer);
    if (encodeRez == -1)
        return -1;
    return encodeRez + (writeStartCode ? 4 : 0);
}

int NALUnit::serialize(uint8_t* dstBuffer)
{
    *dstBuffer++ = 0;
    *dstBuffer++ = 0;
    *dstBuffer++ = 1;
    *dstBuffer = static_cast<uint8_t>((nal_ref_idc & 3) << 5) | static_cast<uint8_t>(nal_unit_type);
    return 4;
}

// -------------------- NALDelimiter ------------------
int NALDelimiter::deserialize(uint8_t* buffer, uint8_t* end)
{
    const int rez = NALUnit::deserialize(buffer, end);
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
    *curBuf++ = static_cast<uint8_t>(primary_pic_type << 5 | 0x10);
    return static_cast<int>(curBuf - buffer);
}

// -------------------- PPSUnit --------------------------
int PPSUnit::deserialize()
{
    uint8_t* nalEnd = m_nalBuffer + m_nalBufferLen;
    const int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
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
        entropy_coding_mode_flag = bitReader.getBit();
        pic_order_present_flag = bitReader.getBit();

        m_ready = true;
        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

// -------------------- HRDParams -------------------------
HRDParams::HRDParams()
{
    resetDefault(false);
    isPresent = false;
}

void HRDParams::resetDefault(const bool mvc)
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
    : m_ready(false),
      sar_width(0),
      sar_height(0),
      num_units_in_tick(0),
      time_scale(0),
      fixed_frame_rate_flag(0),
      num_units_in_tick_bit_pos(0),
      pic_struct_present_flag(0),
      frame_crop_left_offset(0),
      frame_crop_right_offset(0),
      frame_crop_top_offset(0),
      frame_crop_bottom_offset(0),
      full_sps_bit_len(0),
      vui_parameters_bit_pos(-1),
      low_delay_hrd_flag(0),
      separate_colour_plane_flag(0),
      profile_idc(0),
      num_views(0),
      level_idc(0),
      level_idc_ext(0),
      seq_parameter_set_id(0),
      chroma_format_idc(1),
      log2_max_frame_num(0),
      pic_order_cnt_type(0),
      log2_max_pic_order_cnt_lsb(0),
      delta_pic_order_always_zero_flag(0),
      offset_for_non_ref_pic(0),
      num_ref_frames(0),
      pic_width_in_mbs(0),
      pic_height_in_map_units(0),
      frame_mbs_only_flag(0),
      vui_parameters_present_flag(0),
      timing_info_present_flag(0),
      aspect_ratio_info_present_flag(0),
      aspect_ratio_idc(0),
      hrdParamsBitPos(-1)
{
}

void SPSUnit::scaling_list(int* scalingList, const int sizeOfScalingList, bool& useDefaultScalingMatrixFlag)
{
    int lastScale = 8;
    int nextScale = 8;
    for (int j = 0; j < sizeOfScalingList; j++)
    {
        if (nextScale != 0)
        {
            const int delta_scale = extractSEGolombCode();
            nextScale = (lastScale + delta_scale + 256) % 256;
            useDefaultScalingMatrixFlag = (j == 0 && nextScale == 0);
        }
        scalingList[j] = (nextScale == 0) ? lastScale : nextScale;
        lastScale = scalingList[j];
    }
}

int SPSUnit::deserialize()
{
    if (m_nalBufferLen < 4)
        return NOT_ENOUGH_BUFFER;
    int rez = NALUnit::deserialize(m_nalBuffer, m_nalBuffer + m_nalBufferLen);
    if (rez != 0)
        return rez;
    profile_idc = m_nalBuffer[1];
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
                separate_colour_plane_flag = bitReader.getBit();
            if (extractUEGolombCode() > 6)  // bit_depth_luma - 8
                return 1;
            if (extractUEGolombCode() > 6)  // bit_depth_chroma - 8
                return 1;
            bitReader.skipBit();     // qpprime_y_zero_transform_bypass_flag
            if (bitReader.getBit())  // seq_scaling_matrix_present_flag
            {
                int ScalingList4x4[6][16]{};
                int ScalingList8x8[2][64]{};
                bool UseDefaultScalingMatrix4x4Flag[6]{};
                bool UseDefaultScalingMatrix8x8Flag[2]{};

                for (int i = 0; i < 8; i++)
                {
                    if (bitReader.getBit())  // seq_scaling_list_present_flag[i]
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
            delta_pic_order_always_zero_flag = bitReader.getBit();
            offset_for_non_ref_pic = extractSEGolombCode();
            extractSEGolombCode();  // offset_for_top_to_bottom_field
            const unsigned num_ref_frames_in_pic_order_cnt_cycle = extractUEGolombCode();
            if (num_ref_frames_in_pic_order_cnt_cycle >= 256)
                return 1;

            for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
                extractSEGolombCode();  // offset_for_ref_frame[i]
        }

        num_ref_frames = extractUEGolombCode();
        bitReader.skipBit();  // gaps_in_frame_num_value_allowed_flag
        pic_width_in_mbs = extractUEGolombCode() + 1;
        pic_height_in_map_units = extractUEGolombCode() + 1;
        frame_mbs_only_flag = bitReader.getBit();
        if (!frame_mbs_only_flag)
            bitReader.skipBit();  // mb_adaptive_frame_field_flag
        bitReader.skipBit();      // direct_8x8_inference_flag
        if (bitReader.getBit())   // frame_cropping_flag
        {
            frame_crop_left_offset = extractUEGolombCode();
            frame_crop_right_offset = extractUEGolombCode();
            frame_crop_top_offset = extractUEGolombCode();
            frame_crop_bottom_offset = extractUEGolombCode();
        }
        vui_parameters_bit_pos = bitReader.getBitsCount() + 32;
        vui_parameters_present_flag = bitReader.getBit();
        if (vui_parameters_present_flag)
            if (deserializeVuiParameters() != 0)
                return 1;

        if (nal_unit_type == NALType::nuSubSPS)
        {
            rez = deserializeSubSPS();
            if (rez != 0)
                return rez;
        }

        m_ready = true;
        full_sps_bit_len = calcNalLenInBits(m_nalBuffer, m_nalBuffer + m_nalBufferLen);

        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

int SPSUnit::deserializeSubSPS()
{
    if (profile_idc == 83 || profile_idc == 86)
    {
    }
    else if (profile_idc == 118 || profile_idc == 128)
    {
        if (bitReader.getBit() != 1)
            return INVALID_BITSTREAM_SYNTAX;
        if (seq_parameter_set_mvc_extension() != 0)  // specified in Annex H
            return 1;
        const int mvc_vui_parameters_present_flag = bitReader.getBit();
        if (mvc_vui_parameters_present_flag)
            if (mvc_vui_parameters_extension() != 0)  // specified in Annex H
                return 1;
    }

    return 0;
}

int SPSUnit::deserializeVuiParameters()
{
    aspect_ratio_info_present_flag = bitReader.getBit();
    if (aspect_ratio_info_present_flag)
    {
        aspect_ratio_idc = bitReader.getBits<uint8_t>(8);
        if (aspect_ratio_idc == Extended_SAR)
        {
            sar_width = bitReader.getBits<uint16_t>(16);
            sar_height = bitReader.getBits<uint16_t>(16);
        }
    }
    if (bitReader.getBit())   // overscan_info_present_flag
        bitReader.skipBit();  // overscan_appropriate_flag
    if (bitReader.getBit())   // video_signal_type_present_flag
    {
        bitReader.skipBits(4);       // video_format, video_full_range_flag
        if (bitReader.getBit())      // colour_description_present_flag
            bitReader.skipBits(24);  // colour_primaries, transfer_characteristics, matrix_coefficients
    }
    if (bitReader.getBit())  // chroma_loc_info_present_flag
    {
        if (extractUEGolombCode() > 5)  // chroma_sample_loc_type_top_field
            return 1;
        if (extractUEGolombCode() > 5)  // chroma_sample_loc_type_bottom_field
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
        const int beforeCount = bitReader.getBitsCount();
        if (hrd_parameters(nalHrdParams) != 0)
            return 1;
        nalHrdParams.bitLen = bitReader.getBitsCount() - beforeCount;
    }

    vclHrdParams.isPresent = bitReader.getBit();
    if (vclHrdParams.isPresent)
    {
        const int beforeCount = bitReader.getBitsCount();
        if (hrd_parameters(vclHrdParams) != 0)
            return 1;
        vclHrdParams.bitLen = bitReader.getBitsCount() - beforeCount;
    }
    if (nalHrdParams.isPresent || vclHrdParams.isPresent)
        low_delay_hrd_flag = bitReader.getBit();
    pic_struct_present_flag = bitReader.getBit();
    if (bitReader.getBit())  // bitstream_restriction_flag
    {
        bitReader.skipBit();             // motion_vectors_over_pic_boundaries_flag
        if (extractUEGolombCode() > 16)  // max_bytes_per_pic_denom
            return 1;
        if (extractUEGolombCode() > 16)  // max_bits_per_mb_denom
            return 1;
        if (extractUEGolombCode() > 16)  // log2_max_mv_length_horizontal
            return 1;
        if (extractUEGolombCode() > 16)  // log2_max_mv_length_vertical
            return 1;
        if (extractUEGolombCode() > extractUEGolombCode())  // num_reorder_frames > max_dec_frame_buffering
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
    for (size_t i = mvcNalHrdParams.size(); i-- > 0;)
    {
        if (!mvcNalHrdParams[i].isPresent || !mvcVclHrdParams[i].isPresent)
        {
            const int nalBitLen = mvcNalHrdParams[i].bitLen;
            const int vclBitLen = mvcVclHrdParams[i].bitLen;

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
        const int nalBitLen = nalHrdParams.bitLen;
        const int vclBitLen = vclHrdParams.bitLen;

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
    nalHrdParams.isPresent = true;
    vclHrdParams.isPresent = true;

    if (!fixed_frame_rate_flag)
        updateTimingInfo();
}

void SPSUnit::updateTimingInfo()
{
    // replace hrd parameters not implemented. only insert
    const int bitPos = hrdParamsBitPos - 1;

    static constexpr int EXTRA_SPACE = 64;

    const auto newNalBuffer = new uint8_t[m_nalBufferLen + EXTRA_SPACE];

    const int beforeBytes = bitPos >> 3;
    memcpy(newNalBuffer, m_nalBuffer, m_nalBufferLen);

    BitStreamReader reader{};
    reader.setBuffer(m_nalBuffer + beforeBytes, m_nalBuffer + m_nalBufferLen);
    BitStreamWriter writer{};
    writer.setBuffer(newNalBuffer + beforeBytes, newNalBuffer + m_nalBufferLen + EXTRA_SPACE);
    unsigned tmpVal = reader.getBits(bitPos & 7);
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
    unsigned bitRest = full_sps_bit_len - reader.getBitsCount() - beforeBytes * 8;
    for (; bitRest >= 8; bitRest -= 8)
    {
        tmpVal = reader.getBits(8);
        writer.putBits(8, tmpVal);
    }
    if (bitRest > 0)
    {
        tmpVal = reader.getBits(bitRest);
        writer.putBits(bitRest, tmpVal);
    }

    full_sps_bit_len = writer.getBitsCount() + beforeBytes * 8;

    write_rbsp_trailing_bits(writer);
    writer.flushBits();
    m_nalBufferLen = writer.getBitsCount() / 8 + beforeBytes;

    delete[] m_nalBuffer;
    m_nalBuffer = newNalBuffer;
}

void SPSUnit::insertHrdData(const int bitPos, const int nal_hrd_len, const int vcl_hrd_len, const bool addVuiHeader,
                            const HRDParams& params)
{
    // replace hrd parameters not implemented. only insert
    static constexpr int EXTRA_SPACE = 64;

    const auto newNalBuffer = new uint8_t[m_nalBufferLen + EXTRA_SPACE];

    const int beforeBytes = bitPos >> 3;
    memcpy(newNalBuffer, m_nalBuffer, m_nalBufferLen);

    BitStreamReader reader{};
    reader.setBuffer(m_nalBuffer + beforeBytes, m_nalBuffer + m_nalBufferLen);
    BitStreamWriter writer{};
    writer.setBuffer(newNalBuffer + beforeBytes, newNalBuffer + m_nalBufferLen + EXTRA_SPACE);
    unsigned tmpVal = reader.getBits(bitPos & 7);
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
    unsigned bitRest = full_sps_bit_len - reader.getBitsCount() - beforeBytes * 8;
    for (; bitRest >= 8; bitRest -= 8)
    {
        tmpVal = reader.getBits(8);
        writer.putBits(8, tmpVal);
    }
    if (bitRest > 0)
    {
        tmpVal = reader.getBits(bitRest);
        writer.putBits(bitRest, tmpVal);
    }

    full_sps_bit_len = writer.getBitsCount() + beforeBytes * 8;

    write_rbsp_trailing_bits(writer);
    writer.flushBits();
    m_nalBufferLen = writer.getBitsCount() / 8 + beforeBytes;

    delete[] m_nalBuffer;
    m_nalBuffer = newNalBuffer;
}

unsigned SPSUnit::getMaxBitrate() const
{
    if (nalHrdParams.bit_rate_value_minus1.empty() == 0)
        return 0;
    return (nalHrdParams.bit_rate_value_minus1[0] + 1) << (6 + nalHrdParams.bit_rate_scale);
}

int SPSUnit::hrd_parameters(HRDParams& params)
{
    params.cpb_cnt_minus1 = extractUEGolombCode();
    if (params.cpb_cnt_minus1 >= 32)
        return 1;
    params.bit_rate_scale = bitReader.getBits<uint8_t>(4);
    params.cpb_size_scale = bitReader.getBits<uint8_t>(4);

    params.bit_rate_value_minus1.resize(params.cpb_cnt_minus1 + 1);
    params.cpb_size_value_minus1.resize(params.cpb_cnt_minus1 + 1);
    params.cbr_flag.resize(params.cpb_cnt_minus1 + 1);

    for (size_t SchedSelIdx = 0; SchedSelIdx <= params.cpb_cnt_minus1; SchedSelIdx++)
    {
        params.bit_rate_value_minus1[SchedSelIdx] = extractUEGolombCode();
        if (params.bit_rate_value_minus1[SchedSelIdx] == UINT_MAX)
            return 1;
        params.cpb_size_value_minus1[SchedSelIdx] = extractUEGolombCode();
        if (params.cpb_size_value_minus1[SchedSelIdx] == UINT_MAX)
            return 1;
        params.cbr_flag[SchedSelIdx] = bitReader.getBit();
    }
    params.initial_cpb_removal_delay_length_minus1 = bitReader.getBits<uint8_t>(5);
    params.cpb_removal_delay_length_minus1 = bitReader.getBits<uint8_t>(5);
    params.dpb_output_delay_length_minus1 = bitReader.getBits<uint8_t>(5);
    params.time_offset_length = bitReader.getBits<uint8_t>(5);

    return 0;
}

unsigned SPSUnit::getCropY() const
{
    const int SubHeightC = chroma_format_idc == 1 ? 2 : 1;
    return SubHeightC * (2 - frame_mbs_only_flag) * (frame_crop_top_offset + frame_crop_bottom_offset);
}

unsigned SPSUnit::getCropX() const
{
    const int SubWidthC = chroma_format_idc == 1 || chroma_format_idc == 2 ? 2 : 1;
    return SubWidthC * (frame_crop_left_offset + frame_crop_right_offset);
}

double SPSUnit::getFPS() const
{
    return num_units_in_tick != 0 ? static_cast<double>(time_scale) / num_units_in_tick / 2 : 0;
}

void SPSUnit::setFps(const double fps)
{
    time_scale = lround(fps) * 1000000;
    num_units_in_tick = lround(time_scale / fps);
    time_scale *= 2;

    if (num_units_in_tick_bit_pos > 0)
    {
        updateBits(num_units_in_tick_bit_pos, 32, num_units_in_tick);
        updateBits(num_units_in_tick_bit_pos + 32, 32, time_scale);
    }
}

std::string SPSUnit::getStreamDescr() const
{
    std::ostringstream rez;

    if (nal_unit_type == NALType::nuSubSPS)
    {
        if (profile_idc == 83 || profile_idc == 86)
            rez << "SVC part ";
        else if (profile_idc == 118 || profile_idc == 128)
            rez << "H.264/MVC Views: " << int32uToStr(num_views) << " ";
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
    const double fps = getFPS();
    if (fps != 0.0)
    {
        rez << fps;
    }
    else
        rez << "not found";

    return rez.str();
}

int SPSUnit::seq_parameter_set_mvc_extension()
{
    const unsigned num_views_minus1 = extractUEGolombCode();
    if (num_views_minus1 >= 1 << 10)
        return 1;
    num_views = num_views_minus1 + 1;

    view_id.resize(num_views);
    for (unsigned i = 0; i < num_views; i++)
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

    for (unsigned i = 1; i < num_views; i++)
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

    for (unsigned i = 1; i < num_views; i++)
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

    const unsigned num_level_values_signalled_minus1 = extractUEGolombCode();
    if (num_level_values_signalled_minus1 >= 64)
        return 1;
    num_applicable_ops_minus1.resize(num_level_values_signalled_minus1 + 1);
    level_idc_ext.resize(num_level_values_signalled_minus1 + 1);
    for (unsigned i = 0; i <= num_level_values_signalled_minus1; i++)
    {
        level_idc_ext[i] = bitReader.getBits<uint8_t>(8);
        num_applicable_ops_minus1[i] = extractUEGolombCode();
        if (num_applicable_ops_minus1[i] >= 1 << 10)
            return 1;
        for (size_t j = 0; j <= num_applicable_ops_minus1[i]; j++)
        {
            bitReader.skipBits(3);                         // applicable_op_temporal_id[ i ][ j ]
            const unsigned dummy = extractUEGolombCode();  // applicable_op_num_target_views_minus1[ i ][ j ]
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
    const unsigned vui_mvc_num_ops = extractUEGolombCode() + 1;
    if (vui_mvc_num_ops > 1 << 10)
        return 1;
    std::vector<uint8_t> vui_mvc_temporal_id;
    vui_mvc_temporal_id.resize(vui_mvc_num_ops);
    mvcHrdParamsBitPos.resize(vui_mvc_num_ops);
    mvcNalHrdParams.resize(vui_mvc_num_ops);
    mvcVclHrdParams.resize(vui_mvc_num_ops);

    for (size_t i = 0; i < vui_mvc_num_ops; i++)
    {
        vui_mvc_temporal_id[i] = bitReader.getBits<uint8_t>(3);
        const unsigned vui_mvc_num_target_output_views = extractUEGolombCode() + 1;
        if (vui_mvc_num_target_output_views > 1 << 10)
            return 1;
        for (size_t j = 0; j < vui_mvc_num_target_output_views; j++)
            if (extractUEGolombCode() >= 1 << 10)  // vui_mvc_view_id[ i ][ j ]
                return 1;

        if (bitReader.getBit())  // vui_mvc_timing_info_present_flag[ i ]
        {
            bitReader.skipBits(32);  // vui_mvc_num_units_in_tick[i]
            bitReader.skipBits(32);  // vui_mvc_time_scale[ i ]
            bitReader.skipBit();     // vui_mvc_fixed_frame_rate_flag[ i ]
        }

        mvcHrdParamsBitPos[i] = bitReader.getBitsCount() + 32;
        mvcNalHrdParams[i].isPresent = bitReader.getBit();
        if (mvcNalHrdParams[i].isPresent)
        {
            const int beforeCount = bitReader.getBitsCount();
            if (hrd_parameters(mvcNalHrdParams[i]) != 0)
                return 1;
            mvcNalHrdParams[i].bitLen = bitReader.getBitsCount() - beforeCount;
        }
        mvcVclHrdParams[i].isPresent = bitReader.getBit();
        if (mvcVclHrdParams[i].isPresent)
        {
            const int beforeCount = bitReader.getBitsCount();
            if (hrd_parameters(mvcVclHrdParams[i]) != 0)
                return 1;
            mvcVclHrdParams[i].bitLen = bitReader.getBitsCount() - beforeCount;
        }
        if (mvcNalHrdParams[i].isPresent || mvcVclHrdParams[i].isPresent)
            bitReader.skipBit();  // vui_mvc_low_delay_hrd_flag[i]
        bitReader.skipBit();      // vui_mvc_pic_struct_present_flag[i]
    }
    return 0;
}

// --------------------- SliceUnit -----------------------

SliceUnit::SliceUnit()
    : m_field_pic_flag(0),
      non_idr_flag(0),
      memory_management_control_operation(0),
      first_mb_in_slice(0),
      slice_type(0),
      orig_slice_type(0),
      pic_parameter_set_id(0),
      frame_num(0),
      bottom_field_flag(0),
      pic_order_cnt_lsb(0),
      anchor_pic_flag(0),
      pps(),
      sps()
{
}

void SliceUnit::nal_unit_header_svc_extension()
{
    non_idr_flag = !bitReader.getBit();  // idr_flag here, use same variable
    bitReader.skipBits(22);
}

void SliceUnit::nal_unit_header_mvc_extension()
{
    non_idr_flag = bitReader.getBit();
    bitReader.skipBits(19);  // priority_id, view_id, temporal_id
    anchor_pic_flag = bitReader.getBit();
    bitReader.skipBits(2);  // inter_view_flag, reserved_one_bit
}

bool SliceUnit::isIDR() const
{
    return nal_unit_type == NALType::nuSliceIDR || (nal_unit_type == NALType::nuSliceExt && !non_idr_flag);
}

bool SliceUnit::isIFrame() const
{
    return nal_unit_type == NALType::nuSliceExt ? anchor_pic_flag : slice_type == I_TYPE;
}

int SliceUnit::deserializeSliceType(uint8_t* buffer, uint8_t* end)
{
    if (end - buffer < 2)
        return NOT_ENOUGH_BUFFER;

    const int rez = NALUnit::deserialize(buffer, end);
    if (rez != 0)
        return rez;

    try
    {
        int offset = 1;
        if (nal_unit_type == NALType::nuSliceExt)
        {
            if (end - buffer < 5)
                return NOT_ENOUGH_BUFFER;
            offset += 3;
            if (buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 3)
                offset++;  // inplace decode header
            non_idr_flag = buffer[1] & 0x40;
            anchor_pic_flag = buffer[offset - 1] & 0x04;
        }

        bitReader.setBuffer(buffer + offset, end);
        first_mb_in_slice = extractUEGolombCode();
        const unsigned sliceType = extractUEGolombCode();
        if (sliceType > 9)
            return 1;
        orig_slice_type = slice_type = sliceType;
        if (slice_type > 4)
            slice_type -= 5;  // +5 flag is: all other slice at this picture must be same type

        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
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

    try
    {
        bitReader.setBuffer(buffer + 1, end);
        // m_getbitContextBuffer = buffer+1;

        if (nal_unit_type == NALType::nuSliceExt)
        {
            if (bitReader.getBit())               // svc_extension_flag
                nal_unit_header_svc_extension();  // specified in Annex G
            else
                nal_unit_header_mvc_extension();
        }

        rez = deserializeSliceHeader(spsMap, ppsMap);
        // if (rez != 0 || m_shortDeserializeMode)
        return rez;
        // return deserializeSliceData();
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

void NALUnit::updateBits(const int bitOffset, const int bitLen, const unsigned value) const
{
    // uint8_t* ptr = m_getbitContextBuffer + (bitOffset/8);
    uint8_t* ptr = bitReader.getBuffer() + bitOffset / 8;
    BitStreamWriter bitWriter{};
    const int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    const uint8_t* ptr_end = bitReader.getBuffer() + (bitOffset + bitLen) / 8;
    const int endBitsPostfix = 8 - ((bitOffset + bitLen) % 8);

    if (byteOffset > 0)
    {
        const int prefix = *ptr >> (8 - byteOffset);
        bitWriter.putBits(byteOffset, prefix);
    }
    bitWriter.putBits(bitLen, value);

    if (endBitsPostfix < 8)
    {
        const int postfix = *ptr_end & (1 << endBitsPostfix) - 1;
        bitWriter.putBits(endBitsPostfix, postfix);
    }
    bitWriter.flushBits();
}

int SliceUnit::deserializeSliceHeader(const std::map<uint32_t, SPSUnit*>& spsMap,
                                      const std::map<uint32_t, PPSUnit*>& ppsMap)
{
    first_mb_in_slice = extractUEGolombCode();
    const unsigned sliceType = extractUEGolombCode();
    if (sliceType > 9)
        return 1;
    orig_slice_type = slice_type = static_cast<int>(sliceType);
    if (slice_type > 4)
        slice_type -= 5;  // +5 flag is: all other slice at this picture must be same type
    pic_parameter_set_id = extractUEGolombCode();
    if (pic_parameter_set_id >= 256)
        return 1;
    const auto itr = ppsMap.find(pic_parameter_set_id);
    if (itr == ppsMap.end())
        return SPS_OR_PPS_NOT_READY;
    pps = itr->second;

    const auto itr2 = spsMap.find(pps->seq_parameter_set_id);
    if (itr2 == spsMap.end())
        return SPS_OR_PPS_NOT_READY;
    sps = itr2->second;

    if (sps->separate_colour_plane_flag)
        bitReader.skipBits(2);  // colour_plane_id

    frame_num = bitReader.getBits<uint16_t>(sps->log2_max_frame_num);
    bottom_field_flag = 0;
    m_field_pic_flag = 0;
    if (sps->frame_mbs_only_flag == 0)
    {
        m_field_pic_flag = bitReader.getBit();
        if (m_field_pic_flag)
            bottom_field_flag = bitReader.getBit();
    }
    if (isIDR())
    {
        if (extractUEGolombCode() >= 1 << 16)  // idr_pic_id
            return 1;
    }
    if (sps->pic_order_cnt_type == 0)
    {
        pic_order_cnt_lsb = bitReader.getBits<uint16_t>(sps->log2_max_pic_order_cnt_lsb);
        if (pps->pic_order_present_flag && !m_field_pic_flag)
            extractSEGolombCode();  // delta_pic_order_cnt_bottom
    }
    return 0;
}

// --------------- SEI UNIT ------------------------
void SEIUnit::deserialize(const SPSUnit& sps, const int orig_hrd_parameters_present_flag)
{
    pic_struct = -1;

    uint8_t* nalEnd = m_nalBuffer + m_nalBufferLen;
    try
    {
        const int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
        if (rez != 0)
            return;
        uint8_t* curBuff = m_nalBuffer + 1;
        while (curBuff < nalEnd - 1)
        {
            int payloadType = 0;
            for (; curBuff < nalEnd && *curBuff == 0xFF; curBuff++) payloadType += 0xFF;
            if (curBuff >= nalEnd)
                return;
            payloadType += *curBuff++;
            if (curBuff >= nalEnd)
                return;

            int payloadSize = 0;
            for (; curBuff < nalEnd && *curBuff == 0xFF; curBuff++) payloadSize += 0xFF;
            if (curBuff >= nalEnd)
                return;
            payloadSize += *curBuff++;
            if (nalEnd - curBuff < payloadSize)
            {
                LTRACE(LT_WARN, 2, "Bad SEI detected. SEI too short");
                return;
            }
            sei_payload(sps, payloadType, curBuff, payloadSize, orig_hrd_parameters_present_flag);
            m_processedMessages.insert(payloadType);
            curBuff += payloadSize;
        }
    }
    catch (BitStreamException& e)
    {
        (void)e;
        LTRACE(LT_WARN, 2, "Bad SEI detected. SEI too short");
    }
}

int SEIUnit::isMVCSEI()
{
    pic_struct = -1;

    uint8_t* nalEnd = m_nalBuffer + m_nalBufferLen;
    try
    {
        const int rez = NALUnit::deserialize(m_nalBuffer, nalEnd);
        if (rez != 0)
            return NOT_ENOUGH_BUFFER;
        const uint8_t* curBuff = m_nalBuffer + 1;
        while (curBuff < nalEnd - 1)
        {
            int payloadType = 0;
            for (; curBuff < nalEnd && *curBuff == 0xFF; curBuff++) payloadType += 0xFF;
            if (curBuff >= nalEnd)
                return NOT_ENOUGH_BUFFER;
            payloadType += *curBuff++;
            if (curBuff >= nalEnd)
                return NOT_ENOUGH_BUFFER;

            int payloadSize = 0;
            for (; curBuff < nalEnd && *curBuff == 0xFF; curBuff++) payloadSize += 0xFF;
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
    catch (BitStreamException& e)
    {
        (void)e;
    }
    return 0;
}

unsigned SEIUnit::removePicTimingSEI(SPSUnit& sps)
{
    const uint8_t* nalEnd = m_nalBuffer + m_nalBufferLen;
    const uint8_t* curBuff = m_nalBuffer + 1;
    uint8_t tmpBuffer[1024 * 4]{};
    tmpBuffer[0] = m_nalBuffer[0];
    unsigned tmpBufferLen = 1;

    while (curBuff < nalEnd)
    {
        int payloadType = 0;
        for (; curBuff < nalEnd && *curBuff == 0xFF; curBuff++)
        {
            payloadType += 0xFF;
            tmpBuffer[tmpBufferLen++] = 0xff;
        }
        if (curBuff >= nalEnd)
            break;
        payloadType += *curBuff;
        tmpBuffer[tmpBufferLen++] = *curBuff++;
        if (curBuff >= nalEnd)
            break;

        int payloadSize = 0;
        for (; curBuff < nalEnd && *curBuff == 0xFF; curBuff++)
        {
            payloadSize += 0xFF;
            tmpBuffer[tmpBufferLen++] = 0xff;
        }
        if (curBuff >= nalEnd)
            break;

        payloadSize += *curBuff;
        tmpBuffer[tmpBufferLen++] = *curBuff++;
        if (curBuff >= nalEnd)
            break;
        if (payloadType == SEI_MSG_PIC_TIMING || payloadType == SEI_MSG_BUFFERING_PERIOD)
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

void SEIUnit::sei_payload(const SPSUnit& sps, const int payloadType, uint8_t* curBuff, const int payloadSize,
                          const int orig_hrd_parameters_present_flag)
{
    switch (payloadType)
    {
    case 0:
        buffering_period(payloadSize);
        break;
    case 1:
        pic_timing(sps, curBuff, payloadSize, orig_hrd_parameters_present_flag);
        break;
    case 2:
        pan_scan_rect(payloadSize);
        break;
    case 3:
        filler_payload(payloadSize);
        break;
    case 4:
        user_data_registered_itu_t_t35(payloadSize);
        break;
    case 5:
        user_data_unregistered(payloadSize);
        break;
    case 6:
        recovery_point(payloadSize);
        break;
    case 7:
        dec_ref_pic_marking_repetition(payloadSize);
        break;
    case 8:
        spare_pic(payloadSize);
        break;
    case 9:
        scene_info(payloadSize);
        break;
    case 10:
        sub_seq_info(payloadSize);
        break;
    case 11:
        sub_seq_layer_characteristics(payloadSize);
        break;
    case 12:
        sub_seq_characteristics(payloadSize);
        break;
    case 13:
        full_frame_freeze(payloadSize);
        break;
    case 14:
        // full_frame_freeze_release
        break;
    case 15:
        full_frame_snapshot(payloadSize);
        break;
    case 16:
        progressive_refinement_segment_start(payloadSize);
        break;
    case 17:
        progressive_refinement_segment_end(payloadSize);
        break;
    case 18:
        motion_constrained_slice_group_set(payloadSize);
        break;
    case 19:
        film_grain_characteristics(payloadSize);
        break;
    case 20:
        deblocking_filter_display_preference(payloadSize);
        break;
    case 21:
        stereo_video_info(payloadSize);
        break;
    case 37:
        mvc_scalable_nesting(sps, curBuff, payloadSize, orig_hrd_parameters_present_flag);
        break;
    default:
        reserved_sei_message(payloadSize);
    }
}

void SEIUnit::buffering_period(int payloadSize) {}

int SEIUnit::getNumClockTS(const int pic_struct)
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
    default:;
    }
    return NumClockTS;
}

void SEIUnit::serialize_pic_timing_message(const SPSUnit& sps, BitStreamWriter& writer, const bool seiHeader) const
{
    if (seiHeader)
    {
        writer.putBits(8, static_cast<int>(NALType::nuSEI));
        writer.putBits(8, SEI_MSG_PIC_TIMING);
    }
    uint8_t* size = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);
    const unsigned beforeMessageLen = writer.getBitsCount();

    // pic timing
    if (sps.nalHrdParams.isPresent || sps.vclHrdParams.isPresent)
    {
        writer.putBits(sps.nalHrdParams.cpb_removal_delay_length_minus1 + 1, cpb_removal_delay);
        writer.putBits(sps.nalHrdParams.dpb_output_delay_length_minus1 + 1, dpb_output_delay);
    }

    if (sps.pic_struct_present_flag)
    {
        writer.putBits(4, pic_struct);
        const int NumClockTS = getNumClockTS(pic_struct);
        for (int i = 0; i < NumClockTS; i++) writer.putBit(0);  // clock_timestamp_flag
        if (sps.nalHrdParams.time_offset_length > 0)
            writer.putBits(sps.nalHrdParams.time_offset_length, 0);
    }
    write_byte_align_bits(writer);
    // ---------
    const unsigned msgLen = writer.getBitsCount() - beforeMessageLen;
    *size = static_cast<uint8_t>(msgLen / 8);

    if (seiHeader)
        write_rbsp_trailing_bits(writer);
}

void SEIUnit::serialize_buffering_period_message(const SPSUnit& sps, BitStreamWriter& writer,
                                                 const bool seiHeader) const
{
    if (seiHeader)
    {
        writer.putBits(8, static_cast<int>(NALType::nuSEI));
        writer.putBits(8, SEI_MSG_BUFFERING_PERIOD);
    }
    uint8_t* size = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);
    const unsigned beforeMessageLen = writer.getBitsCount();

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
    {  // vclHrdBpPresentFlag
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
    const unsigned msgLen = writer.getBitsCount() - beforeMessageLen;
    *size = static_cast<uint8_t>(msgLen / 8);

    if (seiHeader)
        write_rbsp_trailing_bits(writer);
}

void SEIUnit::pic_timing(const SPSUnit& sps, uint8_t* curBuff, const int payloadSize,
                         const bool orig_hrd_parameters_present_flag)
{
    bitReader.setBuffer(curBuff, curBuff + payloadSize);
    pic_timing(sps, orig_hrd_parameters_present_flag);
}

void SEIUnit::pic_timing(const SPSUnit& sps, const bool orig_hrd_parameters_present_flag)
{
    const bool CpbDpbDelaysPresentFlag = orig_hrd_parameters_present_flag == 1;
    cpb_removal_delay = dpb_output_delay = 0;
    if (CpbDpbDelaysPresentFlag)
    {
        cpb_removal_delay = bitReader.getBits(sps.nalHrdParams.cpb_removal_delay_length_minus1 + 1);
        dpb_output_delay = bitReader.getBits(sps.nalHrdParams.dpb_output_delay_length_minus1 + 1);
    }
    if (sps.pic_struct_present_flag)
    {
        pic_struct = bitReader.getBits<int8_t>(4);
        const int numClockTS = getNumClockTS(pic_struct);

        for (int i = 0; i < numClockTS; i++)
        {
            bitReader.skipBit();  // Clock_timestamp_flag
        }
    }
}

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

int SEIUnit::mvc_scalable_nesting(const SPSUnit& sps, uint8_t* curBuf, const int size,
                                  const int orig_hrd_parameters_present_flag)
{
    try
    {
        bitReader.setBuffer(curBuf, curBuf + size);
        const int operation_point_flag = bitReader.getBit();
        if (!operation_point_flag)
        {
            if (!bitReader.getBit())  // all_view_components_in_au_flag
            {
                const unsigned num_view_components_minus1 = extractUEGolombCode();
                if (num_view_components_minus1 >= 1 << 10)
                    return 1;
                for (size_t i = 0; i <= num_view_components_minus1; i++) bitReader.skipBits(10);  // sei_view_id[ i ]
            }
        }
        else
        {
            const unsigned num_view_components_op_minus1 = extractUEGolombCode();
            if (num_view_components_op_minus1 >= 1 << 10)
                return 1;
            for (size_t i = 0; i <= num_view_components_op_minus1; i++)
            {
                bitReader.skipBits(13);  // sei_op_view_id[ i ], sei_op_temporal_id
            }
        }
        const int byteBits = bitReader.getBitsCount() % 8;
        if (byteBits)
            bitReader.skipBits(8 - byteBits);  // byte align

        m_mvcHeaderStart = bitReader.getBuffer();
        m_mvcHeaderLen = bitReader.getBitsCount() / 8;

        const auto payloadType = bitReader.getBits<uint8_t>(8);
        int payloadSize = 0;
        uint8_t sizePart;
        do
        {
            sizePart = bitReader.getBits<uint8_t>(8);
            payloadSize += sizePart;
        } while (sizePart == 0xff);

        if (payloadType == 5)  // user unregister SEI payload data
        {
            if (bitReader.getBitsLeft() >= 128)
            {
                const uint8_t* bdData = curBuf + bitReader.getBitsCount() / 8;
                if (memcmp(bdData, BDROM_METADATA_GUID, 128 / 8) == 0)
                {
                    // process bd rom meta data
                    for (int i = 0; i < 4; ++i) bitReader.skipBits(32);
                    const auto type_indicator = bitReader.getBits(32);
                    switch (type_indicator)
                    {
                    case 0x4F464D44:
                        processBlurayOffsetMetadata();
                        break;
                    case 0x47534D50:
                        processBlurayGopStructure();
                        break;
                    default:;
                    }
                }
            }
        }
        if (payloadType == 0)
        {
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
    const uint8_t* ptr = bitReader.getBuffer() + bitReader.getBitsCount() / 8;
    metadataPtsOffset = static_cast<int>(ptr - m_nalBuffer);
    bitReader.skipBits(24);  // PTS[32..30], marker_bit, PTS[29..15]
    bitReader.skipBits(18);  // marker_bit, PTS[14..0], marker_bit, reserved_for_future_use bit
    number_of_offset_sequences = bitReader.getBits<uint8_t>(6);
}

void SEIUnit::updateMetadataPts(uint8_t* metadataPtsPtr, const int64_t pts)
{
    metadataPtsPtr[0] = (pts >> 30) & 0x07;

    auto val = static_cast<uint16_t>((pts >> 15) & 0x7fff);
    metadataPtsPtr[1] = 0x80 + (val >> 8);
    metadataPtsPtr[2] = val & 0xff;

    val = static_cast<uint16_t>(pts & 0x7fff);
    metadataPtsPtr[3] = 0x80 + (val >> 8);
    metadataPtsPtr[4] = val & 0xff;
}
