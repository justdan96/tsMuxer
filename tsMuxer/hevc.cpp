#include "hevc.h"

#include <fs/systemlog.h>

#include <algorithm>

#include "tsMuxer.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;
static constexpr int EXTENDED_SAR = 255;

int ceilDiv(const int a, const int b) { return (a / b) + ((a % b) ? 1 : 0); }

// ------------------------- HevcUnit -------------------

unsigned HevcUnit::extractUEGolombCode()
{
    int cnt = 0;
    for (; m_reader.getBit() == 0; cnt++)
        ;
    if (cnt > INT_BIT)
        THROW_BITSTREAM_ERR;
    return (1 << cnt) - 1 + m_reader.getBits(cnt);
}

int HevcUnit::extractSEGolombCode()
{
    const unsigned rez = extractUEGolombCode();
    if (rez % 2 == 0)
        return -static_cast<int>(rez / 2);
    else
        return static_cast<int>((rez + 1) / 2);
}

void HevcUnit::decodeBuffer(const uint8_t* buffer, const uint8_t* end)
{
    delete[] m_nalBuffer;
    m_nalBuffer = new uint8_t[end - buffer];
    m_nalBufferLen = NALUnit::decodeNAL(buffer, end, m_nalBuffer, end - buffer);
}

int HevcUnit::deserialize()
{
    m_reader.setBuffer(m_nalBuffer, m_nalBuffer + m_nalBufferLen);
    try
    {
        m_reader.skipBit();
        nal_unit_type = static_cast<NalType>(m_reader.getBits(6));
        nuh_layer_id = m_reader.getBits(6);
        nuh_temporal_id_plus1 = m_reader.getBits(3);
        if (nuh_temporal_id_plus1 == 0 ||
            (nuh_temporal_id_plus1 != 1 &&
             (nal_unit_type == HevcUnit::NalType::VPS || nal_unit_type == HevcUnit::NalType::SPS ||
              nal_unit_type == HevcUnit::NalType::EOS || nal_unit_type == HevcUnit::NalType::EOB)))
            return 1;
        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

void HevcUnit::updateBits(const int bitOffset, const int bitLen, const int value) const
{
    uint8_t* ptr = m_reader.getBuffer() + bitOffset / 8;
    BitStreamWriter bitWriter{};
    const int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    const uint8_t* ptr_end = m_reader.getBuffer() + (bitOffset + bitLen) / 8;
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

int HevcUnit::serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd) const
{
    if (m_nalBufferLen == 0)
        return 0;
    const int encodeRez = NALUnit::encodeNAL(m_nalBuffer, m_nalBuffer + m_nalBufferLen, dstBuffer, dstEnd - dstBuffer);
    if (encodeRez == -1)
        return -1;
    else
        return encodeRez;
}

// ------------------------- HevcUnitWithProfile  -------------------

HevcUnitWithProfile::HevcUnitWithProfile() : profile_idc(0), level_idc(0), interlaced_source_flag(0) {}

int HevcUnitWithProfile::profile_tier_level(const int subLayers)
{
    try
    {
        bool sub_layer_profile_present_flag[7]{false};
        bool sub_layer_level_present_flag[7]{false};

        m_reader.skipBits(3);  // profile_space, tier_flag
        profile_idc = m_reader.getBits(5);
        m_reader.skipBits(32);  // general_profile_compatibility_flag
        m_reader.skipBit();     // progressive_source_flag
        interlaced_source_flag = m_reader.getBit();
        m_reader.skipBits(32);  // unused flags
        m_reader.skipBits(14);  // unused flags
        level_idc = m_reader.getBits(8);

        for (int i = 0; i < subLayers - 1; i++)
        {
            sub_layer_profile_present_flag[i] = m_reader.getBit();
            sub_layer_level_present_flag[i] = m_reader.getBit();
        }
        if (subLayers > 1)
        {
            for (int i = subLayers - 1; i < 8; i++) m_reader.skipBits(2);  // reserved_zero_2bits
        }

        for (int i = 0; i < subLayers - 1; i++)
        {
            if (sub_layer_profile_present_flag[i])
            {
                m_reader.skipBits(32);  // unused flags
                m_reader.skipBits(32);  // unused flags
                m_reader.skipBits(24);  // unused flags
            }
            if (sub_layer_level_present_flag[i])
                m_reader.skipBits(8);  // sub_layer_level_idc[ i ]
        }
        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

std::string HevcUnitWithProfile::getProfileString() const
{
    string rez("Profile: ");
    if (profile_idc == 1)
        rez += string("Main");
    else if (profile_idc == 2)
        rez += string("Main10");
    else if (profile_idc == 3)
        rez += string("MainStillPicture");
    else if (profile_idc == 0)
        rez += string("Not defined");
    else
        rez += "Unknown";
    if (level_idc)
    {
        rez += string("@");
        rez += int32ToStr(level_idc / 30);
        rez += string(".");
        rez += int32ToStr((level_idc % 30) / 3);
    }
    return rez;
}

// ------------------------- HevcVpsUnit -------------------

HevcVpsUnit::HevcVpsUnit()
    : HevcUnitWithProfile(), vps_id(0), num_units_in_tick(0), time_scale(0), num_units_in_tick_bit_pos(-1)
{
}

int HevcVpsUnit::deserialize()
{
    const int rez = HevcUnit::deserialize();
    if (rez)
        return rez;

    try
    {
        m_reader.skipBits(12);  // vps_id, reserved, vps_max_layers
        const int vps_max_sub_layers = m_reader.getBits(3) + 1;
        if (vps_max_sub_layers > 7)
            return 1;
        m_reader.skipBits(17);  // vps_temporal_id_nesting_flag, vps_reserved_0xffff_16bits
        if (profile_tier_level(vps_max_sub_layers) != 0)
            return 1;

        const bool vps_sub_layer_ordering_info_present_flag = m_reader.getBit();
        for (int i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers - 1);
             i <= vps_max_sub_layers - 1; i++)
        {
            const unsigned vps_max_dec_pic_buffering_minus1 = extractUEGolombCode();
            if (extractUEGolombCode() > vps_max_dec_pic_buffering_minus1)  // vps_max_num_reorder_pics
                return 1;
            if (extractUEGolombCode() == 0xffffffff)  // vps_max_latency_increase_plus1
                return 1;
        }
        const int vps_max_layer_id = m_reader.getBits(6);
        const unsigned vps_num_layer_sets_minus1 = extractUEGolombCode();
        if (vps_num_layer_sets_minus1 > 1023)
            return 1;
        for (unsigned i = 1; i <= vps_num_layer_sets_minus1; i++)
        {
            for (int j = 0; j <= vps_max_layer_id; j++) m_reader.skipBit();  // layer_id_included_flag[ i ][ j ] u(1)
        }
        if (m_reader.getBit())  // vps_timing_info_present_flag
        {
            num_units_in_tick_bit_pos = m_reader.getBitsCount();
            num_units_in_tick = m_reader.get32Bits();
            time_scale = m_reader.get32Bits();
        }

        return rez;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

void HevcVpsUnit::setFPS(const double fps)
{
    time_scale = static_cast<uint32_t>(fps + 0.5) * 1000000;
    num_units_in_tick = static_cast<unsigned>(time_scale / fps + 0.5);

    assert(num_units_in_tick_bit_pos > 0);
    updateBits(num_units_in_tick_bit_pos, 32, num_units_in_tick);
    updateBits(num_units_in_tick_bit_pos + 32, 32, time_scale);
}

double HevcVpsUnit::getFPS() const
{
    return num_units_in_tick ? time_scale / static_cast<float>(num_units_in_tick) : 0;
}

string HevcVpsUnit::getDescription() const
{
    string rez("Frame rate: ");
    const double fps = getFPS();
    if (fps != 0.0)
        rez += doubleToStr(fps);
    else
        rez += string("not found");

    return rez;
}

// ------------------------- HevcSpsUnit ------------------------------

HevcSpsUnit::HevcSpsUnit()
    : HevcUnitWithProfile(),
      vps_id(0),
      max_sub_layers(0),
      sps_id(0),
      chromaFormat(0),
      separate_colour_plane_flag(false),
      pic_width_in_luma_samples(0),
      pic_height_in_luma_samples(0),
      bit_depth_luma_minus8(0),
      bit_depth_chroma_minus8(0),
      log2_max_pic_order_cnt_lsb(0),
      nal_hrd_parameters_present_flag(false),
      vcl_hrd_parameters_present_flag(false),
      sub_pic_hrd_params_present_flag(false),
      colour_primaries(2),
      transfer_characteristics(2),
      matrix_coeffs(2),
      chroma_sample_loc_type_top_field(0),
      chroma_sample_loc_type_bottom_field(0),
      num_short_term_ref_pic_sets(0),
      num_units_in_tick(0),
      time_scale(0),
      PicSizeInCtbsY_bits(0)
{
}

// returns 0 on parse success, 1 on error
int HevcSpsUnit::hrd_parameters(const bool commonInfPresentFlag, const int maxNumSubLayersMinus1)
{
    if (commonInfPresentFlag)
    {
        nal_hrd_parameters_present_flag = m_reader.getBit();
        vcl_hrd_parameters_present_flag = m_reader.getBit();
        if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
        {
            sub_pic_hrd_params_present_flag = m_reader.getBit();
            if (sub_pic_hrd_params_present_flag)
            {
                m_reader.skipBits(19);
            }
            m_reader.skipBits(8);  // bit_rate_scale, cpb_size_scale
            if (sub_pic_hrd_params_present_flag)
                m_reader.skipBits(4);  // cpb_size_du_scale u(4)
            m_reader.skipBits(15);
        }
    }

    for (int i = 0; i <= maxNumSubLayersMinus1; i++)
    {
        bool low_delay_hrd_flag = false;
        unsigned cpb_cnt_minus1 = 0;
        const bool fixed_pic_rate_within_cvs_flag = m_reader.getBit() ? true : m_reader.getBit();
        if (fixed_pic_rate_within_cvs_flag)
        {
            if (extractUEGolombCode() > 2047)  // elemental_duration_in_tc_minus1
                return 1;
        }
        else
            low_delay_hrd_flag = m_reader.getBit();
        if (!low_delay_hrd_flag)
        {
            cpb_cnt_minus1 = extractUEGolombCode();
            if (cpb_cnt_minus1 > 32)
                return 1;
        }
        if (nal_hrd_parameters_present_flag)
            if (sub_layer_hrd_parameters(cpb_cnt_minus1) != 0)
                return 1;
        if (vcl_hrd_parameters_present_flag)
            if (sub_layer_hrd_parameters(cpb_cnt_minus1) != 0)
                return 1;
    }
    return 0;
}

// returns 0 on parse success, 1 on error
int HevcSpsUnit::sub_layer_hrd_parameters(const int cpb_cnt_minus1)
{
    for (size_t i = 0; i <= cpb_cnt_minus1; i++)
    {
        if (extractUEGolombCode() == 0xffffffff)  // bit_rate_value_minus1[i]
            return 1;
        if (extractUEGolombCode() == 0xffffffff)  // cpb_size_value_minus1[i]
            return 1;
        if (sub_pic_hrd_params_present_flag)
        {
            if (extractUEGolombCode() == 0xffffffff)  // cpb_size_du_value_minus1[i]
                return 1;
            if (extractUEGolombCode() == 0xffffffff)  // bit_rate_du_value_minus1[i]
                return 1;
        }
        m_reader.skipBit();  // cbr_flag[i]
    }
    return 0;
}

int HevcSpsUnit::vui_parameters()
{
    const bool aspect_ratio_info_present_flag = m_reader.getBit();
    if (aspect_ratio_info_present_flag)
    {
        if (m_reader.getBits(8) == EXTENDED_SAR)  // aspect_ratio_idc
            m_reader.skipBits(32);                // sar_width, sar_height
    }

    if (m_reader.getBit())   // overscan_info_present_flag
        m_reader.skipBit();  // overscan_appropriate_flag u(1)
    if (m_reader.getBit())   // video_signal_type_present_flag
    {
        m_reader.skipBits(4);   // video_format, video_full_range_flag
        if (m_reader.getBit())  // colour_description_present_flag
        {
            colour_primaries = m_reader.getBits(8);
            transfer_characteristics = m_reader.getBits(8);
            matrix_coeffs = m_reader.getBits(8);
        }
    }

    if (m_reader.getBit())  // chroma_loc_info_present_flag
    {
        chroma_sample_loc_type_top_field = extractUEGolombCode();
        if (chroma_sample_loc_type_top_field > 5)
            return 1;
        chroma_sample_loc_type_bottom_field = extractUEGolombCode();
        if (chroma_sample_loc_type_bottom_field > 5)
            return 1;
    }

    m_reader.skipBits(3);  // unused flags

    if (m_reader.getBit())  // default_display_window_flag
    {
        extractUEGolombCode();  // def_disp_win_left_offset ue(v)
        extractUEGolombCode();  // def_disp_win_right_offset ue(v)
        extractUEGolombCode();  // def_disp_win_top_offset ue(v)
        extractUEGolombCode();  // def_disp_win_bottom_offset ue(v)
    }

    if (m_reader.getBit())  // vui_timing_info_present_flag
    {
        num_units_in_tick = m_reader.get32Bits();
        time_scale = m_reader.get32Bits();

        if (m_reader.getBit())  // vui_poc_proportional_to_timing_flag
        {
            if (extractUEGolombCode() == 0xffffffff)  // vui_num_ticks_poc_diff_one_minus1
                return 1;
        }
        if (m_reader.getBit())  // vui_hrd_parameters_present_flag
        {
            if (hrd_parameters(true, max_sub_layers - 1) != 0)
                return 1;
        }
    }
    if (m_reader.getBit())  // bitstream_restriction_flag
    {
        m_reader.skipBits(3);  //  unused flags

        if (extractUEGolombCode() > 4095)  // min_spatial_segmentation_idc
            return 1;
        if (extractUEGolombCode() > 16)  // max_bytes_per_pic_denom
            return 1;
        if (extractUEGolombCode() > 16)  // max_bits_per_min_cu_denom
            return 1;
        if (extractUEGolombCode() > 15)  // log2_max_mv_length_horizontal
            return 1;
        if (extractUEGolombCode() > 15)  // log2_max_mv_length_vertical
            return 1;
    }
    return 0;
}

int HevcSpsUnit::short_term_ref_pic_set(const unsigned stRpsIdx)
{
    int numDeltaPocs = 0;

    const bool inter_ref_pic_set_prediction_flag = stRpsIdx ? m_reader.getBit() : false;

    if (inter_ref_pic_set_prediction_flag)
    {
        int refRpsIdx = stRpsIdx - 1;

        if (stRpsIdx == num_short_term_ref_pic_sets)
        {
            const unsigned delta_idx_minus1 = extractUEGolombCode();
            if (delta_idx_minus1 >= stRpsIdx)
                return 1;
            refRpsIdx -= delta_idx_minus1;
        }

        m_reader.skipBit();     // delta_rps_sign
        extractUEGolombCode();  // abs_delta_rps_minus1

        for (int j = 0; j <= num_delta_pocs[refRpsIdx]; j++)
        {
            const bool used = m_reader.getBit();                          // used_by_curr_pic_flag[j]
            const bool use_delta_flag = used ? true : m_reader.getBit();  // use_delta_flag[j]
            if (use_delta_flag)
                numDeltaPocs++;
        }
    }
    else
    {
        // numDeltaPocs = num_negative_pics + num_positive_pics
        numDeltaPocs = extractUEGolombCode() + extractUEGolombCode();
        if (numDeltaPocs > 64)
            return 1;

        for (int i = 0; i < numDeltaPocs; i++)
        {
            if (extractUEGolombCode() >= 0x8000)  // delta_poc_minus1[i]
                return 1;
            m_reader.skipBit();  // used_by_curr_pic_flag[i]
        }
    }
    num_delta_pocs[stRpsIdx] = numDeltaPocs;

    return 0;
}

int HevcSpsUnit::scaling_list_data()
{
    for (int sizeId = 0; sizeId < 4; sizeId++)
    {
        for (int matrixId = 0; matrixId < 6; matrixId += (sizeId == 3) ? 3 : 1)
        {
            if (!m_reader.getBit())
            {
                if (extractUEGolombCode() > 5)  // scaling_list_pred_matrix_id_delta
                    return 1;
            }
            else
            {
                const int coefNum = FFMIN(64, (1 << (4 + (sizeId << 1))));
                if (sizeId > 1)
                {
                    const int scaling_list_dc_coef = extractSEGolombCode() + 8;
                    if (scaling_list_dc_coef < 1 || scaling_list_dc_coef > 255)
                        return 1;
                }
                for (int i = 0; i < coefNum; i++)
                {
                    const int scaling_list_delta_coef = extractSEGolombCode();
                    if (scaling_list_delta_coef < -128 || scaling_list_delta_coef > 127)
                        return 1;
                }
            }
        }
    }
    return 0;
}

int HevcSpsUnit::deserialize()
{
    const int rez = HevcUnit::deserialize();
    if (rez)
        return rez;
    try
    {
        vps_id = m_reader.getBits(4);
        max_sub_layers = m_reader.getBits(3) + 1;
        if (max_sub_layers > 7)
            return 1;
        m_reader.skipBit();  // temporal_id_nesting_flag
        if (profile_tier_level(max_sub_layers) != 0)
            return 1;
        sps_id = extractUEGolombCode();
        if (sps_id > 15)
            return 1;
        chromaFormat = extractUEGolombCode();
        if (chromaFormat > 3)
            return 1;
        if (chromaFormat == 3)
            separate_colour_plane_flag = m_reader.getBit();
        pic_width_in_luma_samples = extractUEGolombCode();
        if (pic_width_in_luma_samples == 0)
            return 1;
        pic_height_in_luma_samples = extractUEGolombCode();
        if (pic_height_in_luma_samples == 0)
            return 1;
        if (pic_width_in_luma_samples >= 3840)
            V3_flags |= FOUR_K;

        if (m_reader.getBit())  // conformance_window_flag
        {
            extractUEGolombCode();  // conf_win_left_offset ue(v)
            extractUEGolombCode();  // conf_win_right_offset ue(v)
            extractUEGolombCode();  // conf_win_top_offset ue(v)
            extractUEGolombCode();  // conf_win_bottom_offset ue(v)
        }

        bit_depth_luma_minus8 = extractUEGolombCode();
        if (bit_depth_luma_minus8 > 8)
            return 1;
        bit_depth_chroma_minus8 = extractUEGolombCode();
        if (bit_depth_chroma_minus8 > 8)
            return 1;
        log2_max_pic_order_cnt_lsb = extractUEGolombCode() + 4;
        if (log2_max_pic_order_cnt_lsb > 16)
            return 1;
        const bool sps_sub_layer_ordering_info_present_flag = m_reader.getBit();
        for (int i = (sps_sub_layer_ordering_info_present_flag ? 0 : max_sub_layers - 1); i <= max_sub_layers - 1; i++)
        {
            const unsigned sps_max_dec_pic_buffering_minus1 = extractUEGolombCode();
            const unsigned sps_max_num_reorder_pics = extractUEGolombCode();
            if (sps_max_num_reorder_pics > sps_max_dec_pic_buffering_minus1)
                return 1;
            const unsigned sps_max_latency_increase_plus1 = extractUEGolombCode();
            if (sps_max_latency_increase_plus1 == 0xffffffff)
                return 1;
        }

        const unsigned log2_min_luma_coding_block_size_minus3 = extractUEGolombCode();
        const unsigned log2_diff_max_min_luma_coding_block_size = extractUEGolombCode();
        extractUEGolombCode();  // log2_min_luma_transform_block_size_minus2 ue(v)
        extractUEGolombCode();  // log2_diff_max_min_luma_transform_block_size ue(v)
        extractUEGolombCode();  // max_transform_hierarchy_depth_inter ue(v)
        extractUEGolombCode();  // max_transform_hierarchy_depth_intra ue(v)

        const int MinCbLog2SizeY = log2_min_luma_coding_block_size_minus3 + 3;
        const int CtbLog2SizeY = MinCbLog2SizeY + log2_diff_max_min_luma_coding_block_size;
        const int CtbSizeY = 1 << CtbLog2SizeY;
        const int PicWidthInCtbsY = ceilDiv(pic_width_in_luma_samples, CtbSizeY);
        const int PicHeightInCtbsY = ceilDiv(pic_height_in_luma_samples, CtbSizeY);
        int PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;
        PicSizeInCtbsY_bits = 0;
        int count1bits = 0;

        // Ceil( Log2( PicSizeInCtbsY ))
        while (PicSizeInCtbsY)
        {
            count1bits += PicSizeInCtbsY & 1;
            PicSizeInCtbsY_bits++;
            PicSizeInCtbsY >>= 1;
        }
        if (count1bits == 1)
            PicSizeInCtbsY_bits -= 1;  // in case PicSizeInCtbsY is power of 2

        if (m_reader.getBit())  // scaling_list_enabled_flag
        {
            if (m_reader.getBit())  // sps_scaling_list_data_present_flag
            {
                if (scaling_list_data())
                    return 1;
            }
        }

        m_reader.skipBits(2);   // amp_enabled_flag, sample_adaptive_offset_enabled_flag
        if (m_reader.getBit())  // pcm_enabled_flag
        {
            m_reader.skipBits(8);           // pcm_sample_bit_depth_luma_minus1, pcm_sample_bit_depth_chroma_minus1
            if (extractUEGolombCode() > 2)  // log2_min_pcm_luma_coding_block_size_minus3
                return 1;
            if (extractUEGolombCode() > 2)  // log2_diff_max_min_pcm_luma_coding_block_size
                return 1;
            m_reader.skipBit();  // m_rpcm_loop_filter_disabled_flag
        }
        num_short_term_ref_pic_sets = extractUEGolombCode();
        if (num_short_term_ref_pic_sets > 64)
            return 1;

        num_delta_pocs.resize(num_short_term_ref_pic_sets);

        for (unsigned i = 0; i < num_short_term_ref_pic_sets; i++)
        {
            if (short_term_ref_pic_set(i) != 0)
                return 1;
        }
        if (m_reader.getBit())  // long_term_ref_pics_present_flag
        {
            const unsigned num_long_term_ref_pics_sps = extractUEGolombCode();
            if (num_long_term_ref_pics_sps > 32)
                return 1;
            for (unsigned i = 0; i < num_long_term_ref_pics_sps; i++)
            {
                m_reader.skipBits(log2_max_pic_order_cnt_lsb + 1);  // lt_ref_pic_poc_lsb_sps[i]
            }
        }
        m_reader.skipBits(2);   // sps_temporal_mvp_enabled_flag, strong_intra_smoothing_enabled_flag
        if (m_reader.getBit())  // vui_parameters_present_flag
        {
            if (vui_parameters())
                return 1;
        }

        return 0;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

double HevcSpsUnit::getFPS() const
{
    return num_units_in_tick ? time_scale / static_cast<float>(num_units_in_tick) : 0;
}

string HevcSpsUnit::getDescription() const
{
    string result = getProfileString();
    result += string(" Resolution: ") + int32ToStr(pic_width_in_luma_samples) + string(":") +
              int32ToStr(pic_height_in_luma_samples);
    result += (interlaced_source_flag ? string("i") : string("p"));

    const double fps = getFPS();
    result += "  Frame rate: ";
    result += (fps ? doubleToStr(fps) : string("not found"));
    return result;
}

// ----------------------- HevcPpsUnit ------------------------
HevcPpsUnit::HevcPpsUnit()
    : pps_id(0),
      sps_id(0),
      dependent_slice_segments_enabled_flag(false),
      output_flag_present_flag(false),
      num_extra_slice_header_bits(0)
{
}

int HevcPpsUnit::deserialize()
{
    const int rez = HevcUnit::deserialize();
    if (rez)
        return rez;

    try
    {
        pps_id = extractUEGolombCode();
        if (pps_id > 63)
            return 1;
        sps_id = extractUEGolombCode();
        if (sps_id > 15)
            return 1;
        dependent_slice_segments_enabled_flag = m_reader.getBit();
        output_flag_present_flag = m_reader.getBit();
        num_extra_slice_header_bits = m_reader.getBits(3);

        return 0;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

// ----------------------- HevcHdrUnit ------------------------
HevcHdrUnit::HevcHdrUnit() : isHDR10(false), isHDR10plus(false), isDVRPU(false), isDVEL(false) {}

int HevcHdrUnit::deserialize()
{
    const int rez = HevcUnit::deserialize();
    if (rez)
        return rez;
    try
    {
        do
        {
            int payloadType = 0;
            unsigned payloadSize = 0;
            int nbyte = 0xFF;
            while (nbyte == 0xFF)
            {
                nbyte = m_reader.getBits(8);
                payloadType += nbyte;
            }
            nbyte = 0xFF;
            while (nbyte == 0xFF)
            {
                nbyte = m_reader.getBits(8);
                payloadSize += nbyte;
            }
            if (m_reader.getBitsLeft() < payloadSize * 8)
            {
                LTRACE(LT_WARN, 2, "Bad SEI detected. SEI too short");
                return 1;
            }
            if (payloadType == 137 && !isHDR10)  // mastering_display_colour_volume
            {
                isHDR10 = true;
                V3_flags |= HDR10;
                HDR10_metadata[0] = m_reader.getBits(32);  // display_primaries Green
                HDR10_metadata[1] = m_reader.getBits(32);  // display_primaries Red
                HDR10_metadata[2] = m_reader.getBits(32);  // display_primaries Blue
                HDR10_metadata[3] = m_reader.getBits(32);  // White Point
                HDR10_metadata[4] = ((m_reader.getBits(32) / 10000) << 16) +
                                    m_reader.getBits(32);  // max & min display_mastering_luminance
            }
            else if (payloadType == 144)  // content_light_level_info
            {
                int maxCLL = m_reader.getBits(16);
                int maxFALL = m_reader.getBits(16);
                if (maxCLL > (HDR10_metadata[5] >> 16) || maxFALL > (HDR10_metadata[5] & 0x0000ffff))
                {
                    maxCLL = (std::max)(maxCLL, HDR10_metadata[5] >> 16);
                    maxFALL = (std::max)(maxFALL, HDR10_metadata[5] & 0x0000ffff);
                    HDR10_metadata[5] = (maxCLL << 16) + maxFALL;
                }
            }
            else if (payloadType == 4 && payloadSize >= 8 && !isHDR10plus)
            {                           // HDR10Plus Metadata
                m_reader.skipBits(8);   // country_code
                m_reader.skipBits(32);  // terminal_provider
                const int application_identifier = m_reader.getBits(8);
                const int application_version = m_reader.getBits(8);
                const int num_windows = m_reader.getBits(2);
                m_reader.skipBits(6);
                if (application_identifier == 4 && application_version == 1 && num_windows == 1)
                {
                    isHDR10plus = true;
                    V3_flags |= HDR10PLUS;
                }
                payloadSize -= 8;
                for (unsigned i = 0; i < payloadSize; i++) m_reader.skipBits(8);
            }
            else
                for (unsigned i = 0; i < payloadSize; i++) m_reader.skipBits(8);
        } while (m_reader.getBitsLeft() > 16);

        return 0;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

// -----------------------  HevcSliceHeader() -------------------------------------

HevcSliceHeader::HevcSliceHeader() : HevcUnit(), first_slice(false), pps_id(-1), slice_type(-1), pic_order_cnt_lsb(0) {}

int HevcSliceHeader::deserialize(const HevcSpsUnit* sps, const HevcPpsUnit* pps)
{
    const int rez = HevcUnit::deserialize();
    if (rez)
        return rez;

    try
    {
        pic_order_cnt_lsb = 0;
        first_slice = m_reader.getBit();
        if (nal_unit_type >= NalType::BLA_W_LP && nal_unit_type <= NalType::RSV_IRAP_VCL23)
            m_reader.skipBit();  // no_output_of_prior_pics_flag u(1)
        pps_id = extractUEGolombCode();
        if (pps_id > 63)
            return 1;
        bool dependent_slice_segment_flag = false;
        if (!first_slice)
        {
            if (pps->dependent_slice_segments_enabled_flag)
                dependent_slice_segment_flag = m_reader.getBit();
            m_reader.skipBits(sps->PicSizeInCtbsY_bits);  // slice_segment_address
        }
        if (!dependent_slice_segment_flag)
        {
            for (int i = 0; i < pps->num_extra_slice_header_bits; i++)
                m_reader.skipBit();  // slice_reserved_flag[ i ] u(1)
            slice_type = extractUEGolombCode();
            if (slice_type > 2)
                return 1;
            if (pps->output_flag_present_flag)
                m_reader.skipBit();  // pic_output_flag u(1)
            if (sps->separate_colour_plane_flag == 1)
            {
                if (m_reader.getBits(2) > 2)  // colour_plane_id
                    return 1;
            }
            if (!isIDR())
            {
                pic_order_cnt_lsb = m_reader.getBits(sps->log2_max_pic_order_cnt_lsb);
            }
        }

        return 0;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

bool HevcSliceHeader::isIDR() const
{
    return nal_unit_type == NalType::IDR_W_RADL || nal_unit_type == NalType::IDR_N_LP;
}

vector<vector<uint8_t>> hevc_extract_priv_data(const uint8_t* buff, int size, int* nal_size)
{
    *nal_size = 4;

    vector<vector<uint8_t>> spsPps;
    if (size < 23)
        return spsPps;

    *nal_size = (buff[21] & 3) + 1;
    int num_arrays = buff[22];

    const uint8_t* src = buff + 23;
    const uint8_t* end = buff + size;
    for (int i = 0; i < num_arrays; ++i)
    {
        if (src + 3 > end)
            THROW(ERR_MOV_PARSE, "Invalid HEVC extra data format");
        src++;  // type
        int cnt = AV_RB16(src);
        src += 2;

        for (int j = 0; j < cnt; ++j)
        {
            if (src + 2 > end)
                THROW(ERR_MOV_PARSE, "Invalid HEVC extra data format");
            int nalSize = (src[0] << 8) + src[1];
            src += 2;
            if (src + nalSize > end)
                THROW(ERR_MOV_PARSE, "Invalid HEVC extra data format");
            if (nalSize > 0)
            {
                spsPps.push_back(vector<uint8_t>());
                for (int k = 0; k < nalSize; ++k, ++src) spsPps.rbegin()->push_back(*src);
            }
        }
    }

    return spsPps;
}
