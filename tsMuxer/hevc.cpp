#include "hevc.h"

#include <fs/systemlog.h>

#include <algorithm>
#include "tsMuxer.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;
static const int EXTENDED_SAR = 255;

int ceilDiv(int a, int b) { return (a / b) + (a % b ? 1 : 0); }

// ------------------------- HevcUnit -------------------

int HevcUnit::extractUEGolombCode()
{
    int cnt = 0;
    for (; m_reader.getBits(1) == 0; cnt++)
        ;
    if (cnt > INT_BIT)
        THROW_BITSTREAM_ERR;
    return (1 << cnt) - 1 + m_reader.getBits(cnt);
}

int HevcUnit::extractSEGolombCode()
{
    int rez = extractUEGolombCode();
    if (rez % 2 == 0)
        return -rez / 2;
    else
        return (rez + 1) / 2;
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
        nal_unit_type = m_reader.getBits(6);
        nuh_layer_id = m_reader.getBits(6);
        nuh_temporal_id_plus1 = m_reader.getBits(3);
        return 0;
    }
    catch (BitStreamException& e)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

void HevcUnit::updateBits(int bitOffset, int bitLen, int value)
{
    uint8_t* ptr = (uint8_t*)m_reader.getBuffer() + bitOffset / 8;
    BitStreamWriter bitWriter;
    int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    uint8_t* ptr_end = (uint8_t*)m_reader.getBuffer() + (bitOffset + bitLen) / 8;
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

int HevcUnit::serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd) const
{
    if (m_nalBufferLen == 0)
        return 0;
    int encodeRez = NALUnit::encodeNAL(m_nalBuffer, m_nalBuffer + m_nalBufferLen, dstBuffer, dstEnd - dstBuffer);
    if (encodeRez == -1)
        return -1;
    else
        return encodeRez;
}

// ------------------------- HevcUnitWithProfile  -------------------

HevcUnitWithProfile::HevcUnitWithProfile() : profile_idc(0), level_idc(0), interlaced_source_flag(0)
{
    memset(sub_layer_profile_present_flag, 0, sizeof(sub_layer_profile_present_flag));
    memset(sub_layer_level_present_flag, 0, sizeof(sub_layer_level_present_flag));
    memset(sub_layer_profile_space, 0, sizeof(sub_layer_profile_space));
}

void HevcUnitWithProfile::profile_tier_level(int subLayers)
{
    int profile_space = m_reader.getBits(2);
    int tier_flag = m_reader.getBit();
    profile_idc = m_reader.getBits(5);
    m_reader.skipBits(32);  // general_profile_compatibility_flag
    int progressive_source_flag = m_reader.getBit();
    interlaced_source_flag = m_reader.getBit();
    int non_packed_constraint_flag = m_reader.getBit();
    int frame_only_constraint_flag = m_reader.getBit();
    m_reader.skipBits(32);  // reserved_zero_44bits
    m_reader.skipBits(12);  // reserved_zero_44bits
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
            sub_layer_profile_space[i] = m_reader.getBits(2);
            bool sub_layer_tier_flag = m_reader.getBit();
            int sub_layer_profile_idc = m_reader.getBits(5);
            for (int j = 0; j < 32; j++) m_reader.skipBit();  // sub_layer_profile_compatibility_flag[ i ][ j ] u(1)
            m_reader.skipBit();                               // sub_layer_progressive_source_flag[ i ] u(1)
            m_reader.skipBit();                               // sub_layer_interlaced_source_flag[ i ] u(1)
            m_reader.skipBit();                               // sub_layer_non_packed_constraint_flag[ i ] u(1)
            m_reader.skipBit();                               // sub_layer_frame_only_constraint_flag[ i ] u(1)
            m_reader.skipBits(32);                            // sub_layer_reserved_zero_44bits
            m_reader.skipBits(12);                            // sub_layer_reserved_zero_44bits
        }
        if (sub_layer_level_present_flag[i])
            m_reader.skipBits(8);  // sub_layer_level_idc[ i ]
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
    : HevcUnitWithProfile(),
      vps_id(0),
      vps_max_layers(0),
      vps_max_sub_layers(0),
      num_units_in_tick(0),
      time_scale(0),
      num_units_in_tick_bit_pos(-1)
{
}

int HevcVpsUnit::deserialize()
{
    int rez = HevcUnit::deserialize();
    if (rez)
        return rez;
    try
    {
        vps_id = m_reader.getBits(4);
        m_reader.skipBits(2);  // reserved
        vps_max_layers = m_reader.getBits(6) + 1;
        vps_max_sub_layers = m_reader.getBits(3) + 1;
        int vps_temporal_id_nesting_flag = m_reader.getBit();
        m_reader.skipBits(16);  // reserved
        profile_tier_level(vps_max_sub_layers);

        bool vps_sub_layer_ordering_info_present_flag = m_reader.getBit();
        for (int i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers - 1);
             i <= vps_max_sub_layers - 1; i++)
        {
            extractUEGolombCode();  // vps_max_dec_pic_buffering_minus1[ i ] ue(v)
            extractUEGolombCode();  // vps_max_num_reorder_pics[ i ] ue(v)
            extractUEGolombCode();  // vps_max_latency_increase_plus1[ i ] ue(v)
        }
        int vps_max_layer_id = m_reader.getBits(6);
        int vps_num_layer_sets_minus1 = extractUEGolombCode();
        for (int i = 1; i <= vps_num_layer_sets_minus1; i++)
        {
            for (int j = 0; j <= vps_max_layer_id; j++) m_reader.skipBit();  // layer_id_included_flag[ i ][ j ] u(1)
        }
        bool vps_timing_info_present_flag = m_reader.getBit();
        if (vps_timing_info_present_flag)
        {
            num_units_in_tick_bit_pos = m_reader.getBitsCount();
            num_units_in_tick = m_reader.getBits(32);
            time_scale = m_reader.getBits(32);
        }

        return rez;
    }
    catch (VodCoreException& e)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

void HevcVpsUnit::setFPS(double fps)
{
    time_scale = (uint32_t)(fps + 0.5) * 1000000;
    num_units_in_tick = time_scale / fps + 0.5;

    // num_units_in_tick = time_scale/2 / fps;
    assert(num_units_in_tick_bit_pos > 0);
    updateBits(num_units_in_tick_bit_pos, 32, num_units_in_tick);
    updateBits(num_units_in_tick_bit_pos + 32, 32, time_scale);
}

double HevcVpsUnit::getFPS() const { return num_units_in_tick ? time_scale / (float)num_units_in_tick : 0; }

string HevcVpsUnit::getDescription() const
{
    string rez("Frame rate: ");
    double fps = getFPS();
    if (fps != 0)
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
      num_short_term_ref_pic_sets(0),
      colour_primaries(0),
      transfer_characteristics(0),
      matrix_coeffs(0),
      chroma_sample_loc_type_top_field(0),
      chroma_sample_loc_type_bottom_field(0),
      num_units_in_tick(0),
      time_scale(0),
      PicSizeInCtbsY_bits(0)
{
}

void HevcSpsUnit::hrd_parameters(bool commonInfPresentFlag, int maxNumSubLayersMinus1)
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
                m_reader.skipBits(8);  // tick_divisor_minus2 u(8)
                m_reader.skipBits(5);  // du_cpb_removal_delay_increment_length_minus1 u(5)
                m_reader.skipBit();    // sub_pic_cpb_params_in_pic_timing_sei_flag u(1)
                m_reader.skipBits(5);  // dpb_output_delay_du_length_minus1 u(5)
            }
            m_reader.skipBits(4);  // bit_rate_scale u(4)
            m_reader.skipBits(4);  // cpb_size_scale u(4)
            if (sub_pic_hrd_params_present_flag)
                m_reader.skipBits(4);  // cpb_size_du_scale u(4)
            m_reader.skipBits(5);      // initial_cpb_removal_delay_length_minus1 u(5)
            m_reader.skipBits(5);      // au_cpb_removal_delay_length_minus1 u(5)
            m_reader.skipBits(5);      // dpb_output_delay_length_minus1 u(5)
        }
    }

    cpb_cnt_minus1.resize(maxNumSubLayersMinus1 + 1);
    for (int i = 0; i <= maxNumSubLayersMinus1; i++)
    {
        bool low_delay_hrd_flag = false;
        bool fixed_pic_rate_general_flag = m_reader.getBit();
        bool fixed_pic_rate_within_cvs_flag = true;
        if (!fixed_pic_rate_general_flag)
            fixed_pic_rate_within_cvs_flag = m_reader.getBit();
        if (fixed_pic_rate_within_cvs_flag)
            extractUEGolombCode();  // elemental_duration_in_tc_minus1[ i ] ue(v)
        else
            low_delay_hrd_flag = m_reader.getBit();
        if (!low_delay_hrd_flag)
            cpb_cnt_minus1[i] = extractUEGolombCode();
        if (nal_hrd_parameters_present_flag)
            sub_layer_hrd_parameters(i);
        if (vcl_hrd_parameters_present_flag)
            sub_layer_hrd_parameters(i);
    }
}

void HevcSpsUnit::sub_layer_hrd_parameters(int subLayerId)
{
    for (int i = 0; i <= cpb_cnt_minus1[subLayerId]; i++)
    {
        extractUEGolombCode();  // bit_rate_value_minus1[ i ] ue(v)
        extractUEGolombCode();  // cpb_size_value_minus1[ i ] ue(v)
        if (sub_pic_hrd_params_present_flag)
        {
            extractUEGolombCode();  // cpb_size_du_value_minus1[ i ] ue(v)
            extractUEGolombCode();  // bit_rate_du_value_minus1[ i ] ue(v)
        }
        m_reader.skipBit();  // cbr_flag[ i ] u(1)
    }
}

void HevcSpsUnit::vui_parameters()
{
    bool aspect_ratio_info_present_flag = m_reader.getBit();
    if (aspect_ratio_info_present_flag)
    {
        int aspect_ratio_idc = m_reader.getBits(8);
        if (aspect_ratio_idc == EXTENDED_SAR)
        {
            int sar_width = m_reader.getBits(16);
            int sar_height = m_reader.getBits(16);
        }
    }

    bool overscan_info_present_flag = m_reader.getBit();
    if (overscan_info_present_flag)
        m_reader.skipBit();  // overscan_appropriate_flag u(1)
    bool video_signal_type_present_flag = m_reader.getBit();
    if (video_signal_type_present_flag)
    {
        m_reader.skipBits(3);  // video_format u(3)
        m_reader.skipBit();    // video_full_range_flag u(1)
        bool colour_description_present_flag = m_reader.getBit();
        if (colour_description_present_flag)
        {
            colour_primaries = m_reader.getBits(8);
            transfer_characteristics = m_reader.getBits(8);
            matrix_coeffs = m_reader.getBits(8);
        }
    }

    bool chroma_loc_info_present_flag = m_reader.getBit();
    if (chroma_loc_info_present_flag)
    {
        chroma_sample_loc_type_top_field = extractUEGolombCode();
        chroma_sample_loc_type_bottom_field = extractUEGolombCode();
    }

    m_reader.skipBit();  // neutral_chroma_indication_flag u(1)
    m_reader.skipBit();  // field_seq_flag u(1)
    m_reader.skipBit();  // frame_field_info_present_flag u(1)
    bool default_display_window_flag = m_reader.getBit();
    if (default_display_window_flag)
    {
        extractUEGolombCode();  // def_disp_win_left_offset ue(v)
        extractUEGolombCode();  // def_disp_win_right_offset ue(v)
        extractUEGolombCode();  // def_disp_win_top_offset ue(v)
        extractUEGolombCode();  // def_disp_win_bottom_offset ue(v)
    }

    bool vui_timing_info_present_flag = m_reader.getBit();
    if (vui_timing_info_present_flag)
    {
        num_units_in_tick = m_reader.getBits(32);
        time_scale = m_reader.getBits(32);
        bool vui_poc_proportional_to_timing_flag = m_reader.getBit();
        if (vui_poc_proportional_to_timing_flag)
            extractUEGolombCode();  // vui_num_ticks_poc_diff_one_minus1 ue(v)
        bool vui_hrd_parameters_present_flag = m_reader.getBit();
        if (vui_hrd_parameters_present_flag)
            hrd_parameters(1, max_sub_layers - 1);
    }
    bool bitstream_restriction_flag = m_reader.getBit();
    if (bitstream_restriction_flag)
    {
        m_reader.skipBit();     // m_readtiles_fixed_structure_flag u(1)
        m_reader.skipBit();     // motion_vectors_over_pic_boundaries_flag u(1)
        m_reader.skipBit();     // restricted_ref_pic_lists_flag u(1)
        extractUEGolombCode();  // min_spatial_segmentation_idc ue(v)
        extractUEGolombCode();  // max_bytes_per_pic_denom ue(v)
        extractUEGolombCode();  // max_bits_per_min_cu_denom ue(v)
        extractUEGolombCode();  // log2_max_mv_length_horizontal ue(v)
        extractUEGolombCode();  // log2_max_mv_length_vertical ue(v)
    }
}

void HevcSpsUnit::short_term_ref_pic_set(int stRpsIdx)
{
    uint8_t rps_predict = 0;
    int delta_poc;
    int k0 = 0;
    int k1 = 0;
    int k = 0;
    int i;

    if (stRpsIdx != 0)
        rps_predict = m_reader.getBit();

    ShortTermRPS* rps = &st_rps[stRpsIdx];

    if (rps_predict)
    {
        const ShortTermRPS* rps_ridx;
        int delta_rps, abs_delta_rps;
        uint8_t use_delta_flag = 0;
        uint8_t delta_rps_sign;

        // rps_ridx = &st_rps[rps - st_rps - 1];
        rps_ridx = &st_rps[stRpsIdx - 1];

        delta_rps_sign = m_reader.getBit();
        abs_delta_rps = extractUEGolombCode() + 1;
        delta_rps = (1 - (delta_rps_sign << 1)) * abs_delta_rps;
        for (i = 0; i <= rps_ridx->num_delta_pocs; i++)
        {
            int used = rps->used[k] = m_reader.getBit();

            if (!used)
                use_delta_flag = m_reader.getBit();

            if (used || use_delta_flag)
            {
                if (i < rps_ridx->num_delta_pocs)
                    delta_poc = delta_rps + rps_ridx->delta_poc[i];
                else
                    delta_poc = delta_rps;
                rps->delta_poc[k] = delta_poc;
                if (delta_poc < 0)
                    k0++;
                else
                    k1++;
                k++;
            }
        }

        rps->num_delta_pocs = k;
        rps->num_negative_pics = k0;
        // sort in increasing order (smallest first)
        if (rps->num_delta_pocs != 0)
        {
            int used, tmp;
            for (i = 1; i < rps->num_delta_pocs; i++)
            {
                delta_poc = rps->delta_poc[i];
                used = rps->used[i];
                for (k = i - 1; k >= 0; k--)
                {
                    tmp = rps->delta_poc[k];
                    if (delta_poc < tmp)
                    {
                        rps->delta_poc[k + 1] = tmp;
                        rps->used[k + 1] = rps->used[k];
                        rps->delta_poc[k] = delta_poc;
                        rps->used[k] = used;
                    }
                }
            }
        }
        if ((rps->num_negative_pics >> 1) != 0)
        {
            int used;
            k = rps->num_negative_pics - 1;
            // flip the negative values to largest first
            for (i = 0; i<rps->num_negative_pics>> 1; i++)
            {
                delta_poc = rps->delta_poc[i];
                used = rps->used[i];
                rps->delta_poc[i] = rps->delta_poc[k];
                rps->used[i] = rps->used[k];
                rps->delta_poc[k] = delta_poc;
                rps->used[k] = used;
                k--;
            }
        }
    }
    else
    {
        unsigned int prev, nb_positive_pics;
        rps->num_negative_pics = extractUEGolombCode();
        nb_positive_pics = extractUEGolombCode();

        rps->num_delta_pocs = rps->num_negative_pics + nb_positive_pics;
        if (rps->num_delta_pocs)
        {
            prev = 0;
            for (i = 0; i < rps->num_negative_pics; i++)
            {
                delta_poc = extractUEGolombCode() + 1;
                prev -= delta_poc;
                rps->delta_poc[i] = prev;
                rps->used[i] = m_reader.getBit();
            }
            prev = 0;
            for (i = 0; i < nb_positive_pics; i++)
            {
                delta_poc = extractUEGolombCode() + 1;
                prev += delta_poc;
                rps->delta_poc[rps->num_negative_pics + i] = prev;
                rps->used[rps->num_negative_pics + i] = m_reader.getBit();
            }
        }
    }
}

/*
void HevcSpsUnit::short_term_ref_pic_set(int stRpsIdx )
{
    bool inter_ref_pic_set_prediction_flag = false;
    if( stRpsIdx != 0 )
        inter_ref_pic_set_prediction_flag = m_reader.getBit();
    if( inter_ref_pic_set_prediction_flag )
    {
        int delta_idx_minus1 = 0;
        if( stRpsIdx == num_short_term_ref_pic_sets )
            delta_idx_minus1 = extractUEGolombCode();
        int delta_rps_sign = m_reader.getBit();
        int abs_delta_rps_minus1 = extractUEGolombCode();

        int RefRpsIdx = stRpsIdx - ( delta_idx_minus1 + 1 );
        int deltaRps = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);


        int NumDeltaPocs = NumNegativePics[ RefRpsIdx ] + NumPositivePics[ RefRpsIdx ];

        int i = 0;
        for(int j = NumPositivePics[ RefRpsIdx ] - 1; j >= 0; j-- )
        {
            int dPoc = DeltaPocS1[ RefRpsIdx ][ j ] + deltaRps;
            if( dPoc < 0 && use_delta_flag[ NumNegativePics[ RefRpsIdx ] + j ] ) {
                DeltaPocS0[ stRpsIdx ][ i ] = dPoc;
                UsedByCurrPicS0[ stRpsIdx ][ i++ ] = used_by_curr_pic_flag[ NumNegativePics[ RefRpsIdx ] + j ];
            }
        }
        if( deltaRps < 0 && use_delta_flag[ NumDeltaPocs ] )
        {
            DeltaPocS0[ stRpsIdx ][ i ] = deltaRps;
            UsedByCurrPicS0[ stRpsIdx ][ i++ ] = used_by_curr_pic_flag[ NumDeltaPocs ];
        }
        for(int j = 0; j < NumNegativePics[ RefRpsIdx ]; j++ )
        {
            int dPoc = DeltaPocS0[ RefRpsIdx ][ j ] + deltaRps;
            if( dPoc < 0 && use_delta_flag[ j ] ) {
                DeltaPocS0[ stRpsIdx ][ i ] = dPoc;
                UsedByCurrPicS0[ stRpsIdx ][ i++ ] = used_by_curr_pic_flag[ j ];
            }
        }
        NumNegativePics[ stRpsIdx ] = i;

        i = 0;
        for(int j = NumNegativePics[ RefRpsIdx ] - 1; j >= 0; j-- ) {
            int dPoc = DeltaPocS0[ RefRpsIdx ][ j ] + deltaRps;
            if( dPoc > 0 && use_delta_flag[ j ] ) {
                DeltaPocS1[ stRpsIdx ][ i ] = dPoc;
                UsedByCurrPicS1[ stRpsIdx ][ i++ ] = used_by_curr_pic_flag[ j ];
            }
        }
        if( deltaRps > 0 && use_delta_flag[ NumDeltaPocs ] )
        {
            DeltaPocS1[ stRpsIdx ][ i ] = deltaRps;
            UsedByCurrPicS1[ stRpsIdx ][ i++ ] = used_by_curr_pic_flag[ NumDeltaPocs ];
        }
        for(int j = 0; j < NumPositivePics[ RefRpsIdx ]; j++) {
            int dPoc = DeltaPocS1[ RefRpsIdx ][ j ] + deltaRps;
            if( dPoc > 0 && use_delta_flag[ NumNegativePics[ RefRpsIdx ] + j ] ) {
                DeltaPocS1[ stRpsIdx ][ i ] = dPoc;
                    UsedByCurrPicS1[ stRpsIdx ][ i++ ] = used_by_curr_pic_flag[ NumNegativePics[ RefRpsIdx ] + j ];
            }
        }
        NumPositivePics[ stRpsIdx ] = i;

        for(int j = 0; j <= NumDeltaPocs; j++ )
        {
            used_by_curr_pic_flag[j] = m_reader.getBit();
            if( !used_by_curr_pic_flag[ j ] )
                use_delta_flag[j] = m_reader.getBit();
            else
                use_delta_flag[j] = 1;
        }
    }
    else {
        NumNegativePics[stRpsIdx] = extractUEGolombCode();
        NumPositivePics[stRpsIdx] = extractUEGolombCode();

        for(int i = 0; i < NumNegativePics[ stRpsIdx ]; i++ ) {
            int delta_poc_s0_minus1 = extractUEGolombCode();
            UsedByCurrPicS0[ stRpsIdx ][ i ] = m_reader.getBit();
            if (i > 0)
                DeltaPocS0[ stRpsIdx ][ i ] = DeltaPocS0[ stRpsIdx ][ i - 1 ] - ( delta_poc_s0_minus1 + 1 );
            else
                DeltaPocS0[ stRpsIdx ][ i ] = -( delta_poc_s0_minus1 + 1 );
        }

        for(int i = 0; i < NumPositivePics[ stRpsIdx ]; i++ ) {
            int delta_poc_s1_minus1 = extractUEGolombCode();
            UsedByCurrPicS1[ stRpsIdx ][ i ] = m_reader.getBit();
            if (i > 0)
                DeltaPocS1[ stRpsIdx ][ i ] = DeltaPocS1[ stRpsIdx ][ i - 1 ] - ( delta_poc_s1_minus1 + 1 );
            else
                DeltaPocS1[ stRpsIdx ][ i ] = -( delta_poc_s1_minus1 + 1 );
        }

        int pics = NumNegativePics[ stRpsIdx ] + NumPositivePics[stRpsIdx];
        use_delta_flag.resize(pics+2);
        used_by_curr_pic_flag.resize(pics+2);
        for (int i = 0; i < use_delta_flag.size(); ++i)
            use_delta_flag[i] = 1;
    }
}
*/

void HevcSpsUnit::scaling_list_data()
{
    for (int sizeId = 0; sizeId < 4; sizeId++)
    {
        for (int matrixId = 0; matrixId < ((sizeId == 3) ? 2 : 6); matrixId++)
        {
            bool flag = m_reader.getBit();
            if (!flag)
                extractUEGolombCode();  // scaling_list_pred_matrix_id_delta[ sizeId ][ matrixId ]
            else
            {
                int nextCoef = 8;
                int coefNum = FFMIN(64, (1 << (4 + (sizeId << 1))));
                if (sizeId > 1)
                {
                    int value = extractSEGolombCode();  // scaling_list_dc_coef_minus8[ sizeId - 2 ][ matrixId ]
                    nextCoef = value + 8;
                }
                for (int i = 0; i < coefNum; i++)
                {
                    int scaling_list_delta_coef = extractSEGolombCode();
                    nextCoef = (nextCoef + scaling_list_delta_coef + 256) % 256;
                    // ScalingList[ sizeId ][ matrixId ][ i ] = nextCoef;
                }
            }
        }
    }
}

int HevcSpsUnit::deserialize()
{
    int rez = HevcUnit::deserialize();
    if (rez)
        return rez;
    try
    {
        vps_id = m_reader.getBits(4);
        max_sub_layers = m_reader.getBits(3) + 1;
        int temporal_id_nesting_flag = m_reader.getBit();
        profile_tier_level(max_sub_layers);
        sps_id = extractUEGolombCode();
        chromaFormat = extractUEGolombCode();
        if (chromaFormat == 3)
            separate_colour_plane_flag = m_reader.getBit();
        pic_width_in_luma_samples = extractUEGolombCode();
        pic_height_in_luma_samples = extractUEGolombCode();

        bool conformance_window_flag = m_reader.getBit();
        if (conformance_window_flag)
        {
            extractUEGolombCode();  // conf_win_left_offset ue(v)
            extractUEGolombCode();  // conf_win_right_offset ue(v)
            extractUEGolombCode();  // conf_win_top_offset ue(v)
            extractUEGolombCode();  // conf_win_bottom_offset ue(v)
        }

        bit_depth_luma_minus8 = extractUEGolombCode();
        bit_depth_chroma_minus8 = extractUEGolombCode();

        log2_max_pic_order_cnt_lsb = extractUEGolombCode() + 4;
        bool sps_sub_layer_ordering_info_present_flag = m_reader.getBit();
        for (int i = (sps_sub_layer_ordering_info_present_flag ? 0 : max_sub_layers - 1); i <= max_sub_layers - 1; i++)
        {
            extractUEGolombCode();  // sps_max_dec_pic_buffering_minus1[ i ] ue(v)
            extractUEGolombCode();  // sps_max_num_reorder_pics[ i ] ue(v)
            extractUEGolombCode();  // sps_max_latency_increase_plus1[ i ] ue(v)
        }

        int log2_min_luma_coding_block_size_minus3 = extractUEGolombCode();
        int log2_diff_max_min_luma_coding_block_size = extractUEGolombCode();
        extractUEGolombCode();  // log2_min_transform_block_size_minus2 ue(v)
        extractUEGolombCode();  // log2_diff_max_min_transform_block_size ue(v)
        extractUEGolombCode();  // max_transform_hierarchy_depth_inter ue(v)
        extractUEGolombCode();  // max_transform_hierarchy_depth_intra ue(v)

        int MinCbLog2SizeY = log2_min_luma_coding_block_size_minus3 + 3;
        int CtbLog2SizeY = MinCbLog2SizeY + log2_diff_max_min_luma_coding_block_size;
        int CtbSizeY = 1 << CtbLog2SizeY;
        int PicWidthInCtbsY = ceilDiv(pic_width_in_luma_samples, CtbSizeY);
        int PicHeightInCtbsY = ceilDiv(pic_height_in_luma_samples, CtbSizeY);
        int PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;
        PicSizeInCtbsY_bits = 0;
        int count1bits = 0;
        while (PicSizeInCtbsY)
        {
            count1bits += PicSizeInCtbsY & 1;
            PicSizeInCtbsY_bits++;
            PicSizeInCtbsY >>= 1;
        }
        if (count1bits == 1)
            PicSizeInCtbsY_bits -= 1;  // in case PicSizeInCtbsY is power of 2

        bool scaling_list_enabled_flag = m_reader.getBit();
        if (scaling_list_enabled_flag)
        {
            bool sps_scaling_list_data_present_flag = m_reader.getBit();
            if (sps_scaling_list_data_present_flag)
                scaling_list_data();
        }

        m_reader.skipBit();  // amp_enabled_flag u(1)
        m_reader.skipBit();  // sample_adaptive_offset_enabled_flag u(1)
        bool pcm_enabled_flag = m_reader.getBit();
        if (pcm_enabled_flag)
        {
            m_reader.skipBits(4);   // pcm_sample_bit_depth_luma_minus1 u(4)
            m_reader.skipBits(4);   // pcm_sample_bit_depth_chroma_minus1 u(4)
            extractUEGolombCode();  // log2_min_pcm_luma_coding_block_size_minus3 ue(v)
            extractUEGolombCode();  // log2_diff_max_min_pcm_luma_coding_block_size ue(v)
            m_reader.skipBit();     // m_rpcm_loop_filter_disabled_flag u(1)
        }
        num_short_term_ref_pic_sets = extractUEGolombCode();
        /*
        NumNegativePics.resize(num_short_term_ref_pic_sets);
        NumPositivePics.resize(num_short_term_ref_pic_sets);
        UsedByCurrPicS0.resize(num_short_term_ref_pic_sets);
        UsedByCurrPicS1.resize(num_short_term_ref_pic_sets);
        DeltaPocS0.resize(num_short_term_ref_pic_sets);
        DeltaPocS1.resize(num_short_term_ref_pic_sets);
        for (int i = 0; i < num_short_term_ref_pic_sets; ++i) {
            UsedByCurrPicS0[i].resize(MAX_REFS);
            DeltaPocS0[i].resize(MAX_REFS);
            UsedByCurrPicS1[i].resize(MAX_REFS);
            DeltaPocS1[i].resize(MAX_REFS);
        }
        */
        st_rps.resize(num_short_term_ref_pic_sets);

        for (int i = 0; i < num_short_term_ref_pic_sets; i++) short_term_ref_pic_set(i);
        bool long_term_ref_pics_present_flag = m_reader.getBit();
        if (long_term_ref_pics_present_flag)
        {
            int num_long_term_ref_pics_sps = extractUEGolombCode();
            for (int i = 0; i < num_long_term_ref_pics_sps; i++)
            {
                m_reader.skipBits(log2_max_pic_order_cnt_lsb);  // lt_ref_pic_poc_lsb_sps[ i ] u(v)
                m_reader.skipBit();                             // used_by_curr_pic_lt_sps_flag[ i ] u(1)
            }
        }
        m_reader.skipBit();  // sps_temporal_mvp_enabled_flag u(1)
        m_reader.skipBit();  // strong_intra_smoothing_enabled_flag u(1)
        bool vui_parameters_present_flag = m_reader.getBit();
        if (vui_parameters_present_flag)
            vui_parameters();
        bool sps_extension_flag = m_reader.getBit();

        int gg = m_reader.getBitsLeft();
        gg = gg;

        return rez;
    }
    catch (VodCoreException& e)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

double HevcSpsUnit::getFPS() const { return num_units_in_tick ? time_scale / (float)num_units_in_tick : 0; }

string HevcSpsUnit::getDescription() const
{
    string result = getProfileString();
    result += string(" Resolution: ") + int32ToStr(pic_width_in_luma_samples) + string(":") +
              int32ToStr(pic_height_in_luma_samples);
    result += (interlaced_source_flag ? string("i") : string("p"));

    double fps = getFPS();
    result += "  Frame rate: ";
    result += (fps ? doubleToStr(fps) : string("not found"));
    return result;
}

// ----------------------- HevcPpsUnit ------------------------
HevcPpsUnit::HevcPpsUnit()
    : pps_id(-1),
      sps_id(-1),
      dependent_slice_segments_enabled_flag(false),
      output_flag_present_flag(false),
      num_extra_slice_header_bits(0)
{
}

int HevcPpsUnit::deserialize()
{
    int rez = HevcUnit::deserialize();
    if (rez)
        return rez;

    try
    {
        pps_id = extractUEGolombCode();
        sps_id = extractUEGolombCode();
        dependent_slice_segments_enabled_flag = m_reader.getBit();
        output_flag_present_flag = m_reader.getBit();
        num_extra_slice_header_bits = m_reader.getBits(3);

        return 0;
    }
    catch (VodCoreException& e)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

// ----------------------- HevcHdrUnit ------------------------
HevcHdrUnit::HevcHdrUnit()
  : isHDR10(false),
    isHDR10plus(false),
    isDVRPU(false),
    isDVEL(false),
    DVCompatibility(0)
{
}

int HevcHdrUnit::deserialize()
{
    int rez = HevcUnit::deserialize();
    if (rez)
        return rez;
    try
    {
        do
        {
            int payloadType = 0;
            int payloadSize = 0;
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
            else if (payloadType == 4 && !isHDR10plus)
            {                           // HDR10Plus Metadata
                m_reader.skipBits(8);   // country_code
                m_reader.skipBits(32);  // terminal_provider
                int application_identifier = m_reader.getBits(8);
                int application_version = m_reader.getBits(8);
                int num_windows = m_reader.getBits(2);
                m_reader.skipBits(6);
                if (application_identifier == 4 && application_version == 1 && num_windows == 1)
                {
                    isHDR10plus = true;
                    V3_flags |= HDR10PLUS;
                }
                payloadSize -= 8;
                for (int i = 0; i < payloadSize; i++) m_reader.skipBits(8);
            }
            else
                for (int i = 0; i < payloadSize; i++) m_reader.skipBits(8);
        } while (m_reader.showBits(8) != 0x80);

        return 0;
    }
    catch (VodCoreException& e)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

// -----------------------  HevcSliceHeader() -------------------------------------

HevcSliceHeader::HevcSliceHeader() : HevcUnit(), first_slice(false), pps_id(-1), slice_type(-1), pic_order_cnt_lsb(0) {}

int HevcSliceHeader::deserialize(const HevcSpsUnit* sps, const HevcPpsUnit* pps)
{
    int rez = HevcUnit::deserialize();
    if (rez)
        return rez;

    try
    {
        pic_order_cnt_lsb = 0;
        first_slice = m_reader.getBit();
        if (nal_unit_type >= NAL_BLA_W_LP && nal_unit_type <= NAL_RSV_IRAP_VCL23)
            m_reader.skipBit();  // no_output_of_prior_pics_flag u(1)
        pps_id = extractUEGolombCode();
        bool dependent_slice_segment_flag = false;
        if (!first_slice)
        {
            if (pps->dependent_slice_segments_enabled_flag)
                dependent_slice_segment_flag = m_reader.getBit();
            int slice_segment_address = m_reader.getBits(sps->PicSizeInCtbsY_bits);  // Ceil( Log2( PicSizeInCtbsY ) )
        }
        if (!dependent_slice_segment_flag)
        {
            for (int i = 0; i < pps->num_extra_slice_header_bits; i++)
                m_reader.skipBit();  // slice_reserved_flag[ i ] u(1)
            slice_type = extractUEGolombCode();
            if (pps->output_flag_present_flag)
                m_reader.skipBit();  // pic_output_flag u(1)
            if (sps->separate_colour_plane_flag == 1)
                m_reader.skipBits(2);  // colour_plane_id u(2)
            if (!isIDR())
            {
                pic_order_cnt_lsb = m_reader.getBits(sps->log2_max_pic_order_cnt_lsb);
            }
        }
        // LTRACE(LT_INFO, 2, pic_order_cnt_lsb);

        return 0;
    }
    catch (VodCoreException& e)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

bool HevcSliceHeader::isIDR() const { return nal_unit_type == NAL_IDR_W_RADL || nal_unit_type == NAL_IDR_N_LP; }

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
        int type = *src++;
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
                for (int i = 0; i < nalSize; ++i, ++src) spsPps.rbegin()->push_back(*src);
            }
        }
    }

    return spsPps;
}
