#include "vvc.h"

#include <cmath>

#include <algorithm>

#include "tsMuxer.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;
static constexpr int EXTENDED_SAR = 255;

// ------------------------- VvcUnit -------------------

unsigned VvcUnit::extractUEGolombCode()
{
    int cnt = 0;
    for (; m_reader.getBits(1) == 0; cnt++)
        ;
    if (cnt > INT_BIT)
        THROW_BITSTREAM_ERR;
    return (1 << cnt) - 1 + m_reader.getBits(cnt);
}

int VvcUnit::extractSEGolombCode()
{
    const unsigned rez = extractUEGolombCode();
    if (rez % 2 == 0)
        return -static_cast<int>(rez / 2);
    return static_cast<int>((rez + 1) / 2);
}

void VvcUnit::decodeBuffer(const uint8_t* buffer, const uint8_t* end)
{
    delete[] m_nalBuffer;
    m_nalBuffer = new uint8_t[end - buffer];
    m_nalBufferLen = NALUnit::decodeNAL(buffer, end, m_nalBuffer, end - buffer);
}

int VvcUnit::deserialize()
{
    m_reader.setBuffer(m_nalBuffer, m_nalBuffer + m_nalBufferLen);
    try
    {
        m_reader.skipBits(2);  // forbidden_zero_bit, nuh_reserved_zero_bit
        nuh_layer_id = m_reader.getBits(6);
        nal_unit_type = static_cast<NalType>(m_reader.getBits(5));
        nuh_temporal_id_plus1 = m_reader.getBits(3);
        if (nuh_temporal_id_plus1 == 0 ||
            (nuh_temporal_id_plus1 != 1 && ((nal_unit_type >= NalType::OPI && nal_unit_type <= NalType::SPS) ||
                                            nal_unit_type == NalType::EOS || nal_unit_type == NalType::EOB)))
            return 1;
        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

void VvcUnit::updateBits(const int bitOffset, const int bitLen, const int value) const
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

int VvcUnit::serializeBuffer(uint8_t* dstBuffer, const uint8_t* dstEnd) const
{
    if (m_nalBufferLen == 0)
        return 0;
    const int encodeRez = NALUnit::encodeNAL(m_nalBuffer, m_nalBuffer + m_nalBufferLen, dstBuffer, dstEnd - dstBuffer);
    if (encodeRez == -1)
        return -1;
    return encodeRez;
}

bool VvcUnit::dpb_parameters(const int MaxSubLayersMinus1, const bool subLayerInfoFlag)
{
    for (int i = (subLayerInfoFlag ? 0 : MaxSubLayersMinus1); i <= MaxSubLayersMinus1; i++)
    {
        const unsigned dpb_max_dec_pic_buffering_minus1 = extractUEGolombCode();
        const unsigned dpb_max_num_reorder_pics = extractUEGolombCode();
        if (dpb_max_num_reorder_pics > dpb_max_dec_pic_buffering_minus1)
            return true;
        const unsigned dpb_max_latency_increase_plus1 = extractUEGolombCode();
        if (dpb_max_latency_increase_plus1 == 0xffffffff)
            return true;
    }
    return false;
}

// ------------------------- VvcUnitWithProfile  -------------------

VvcUnitWithProfile::VvcUnitWithProfile()
    : profile_idc(0),
      tier_flag(0),
      level_idc(0),
      ptl_frame_only_constraint_flag(true),
      ptl_num_sub_profiles(0),
      general_sub_profile_idc(0)
{
}

int VvcUnitWithProfile::profile_tier_level(const bool profileTierPresentFlag, const int MaxNumSubLayersMinus1)
{
    try
    {
        if (profileTierPresentFlag)
        {
            profile_idc = m_reader.getBits(7);
            tier_flag = m_reader.getBit();
        }
        level_idc = m_reader.getBits(8);
        ptl_frame_only_constraint_flag = m_reader.getBit();
        m_reader.skipBit();  // ptl_multilayer_enabled_flag

        if (profileTierPresentFlag)
        {                           // general_constraints_info()
            if (m_reader.getBit())  // gci_present_flag
            {
                m_reader.skipBits(32);
                m_reader.skipBits(32);
                m_reader.skipBits(7);
                const int gci_num_reserved_bits = m_reader.getBits(8);
                for (int i = 0; i < gci_num_reserved_bits; i++) m_reader.skipBit();  // gci_reserved_zero_bit[i]
            }
            m_reader.skipBits(m_reader.getBitsLeft() % 8);  // gci_alignment_zero_bit
        }
        vector<int> ptl_sublayer_level_present_flag;
        ptl_sublayer_level_present_flag.resize(MaxNumSubLayersMinus1);

        for (int i = MaxNumSubLayersMinus1 - 1; i >= 0; i--) ptl_sublayer_level_present_flag[i] = m_reader.getBit();

        m_reader.skipBits(m_reader.getBitsLeft() % 8);  // ptl_reserved_zero_bit

        for (int i = MaxNumSubLayersMinus1 - 1; i >= 0; i--)
            if (ptl_sublayer_level_present_flag[i])
                m_reader.skipBits(8);  // sublayer_level_idc[i]
        if (profileTierPresentFlag)
        {
            ptl_num_sub_profiles = m_reader.getBits(8);
            general_sub_profile_idc.resize(ptl_num_sub_profiles);
            for (auto& i : general_sub_profile_idc) i = m_reader.getBits(32);
        }
        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

std::string VvcUnitWithProfile::getProfileString() const
{
    string rez("Profile: ");
    if (profile_idc == 1)
        rez += string("Main10");
    else if (profile_idc == 65)
        rez += string("Main10StillPicture");
    else if (profile_idc == 33)
        rez += string("Main10_4:4:4");
    else if (profile_idc == 97)
        rez += string("Main10_4:4:4_StillPicture");
    else if (profile_idc == 17)
        rez += string("Main10_Multilayer");
    else if (profile_idc == 49)
        rez += string("Main10_Multilayer_4:4:4");
    else if (profile_idc == 0)
        rez += string("Not defined");
    else
        rez += "Unknown";
    if (level_idc)
    {
        rez += string("@");
        rez += int32ToStr(level_idc / 16);
        rez += string(".");
        rez += int32ToStr((level_idc % 16) / 3);
    }
    return rez;
}

// ------------------------- VvcVpsUnit -------------------

VvcVpsUnit::VvcVpsUnit()
    : vps_id(0),
      vps_max_layers(0),
      vps_max_sublayers(0),
      num_units_in_tick(0),
      time_scale(0),
      num_units_in_tick_bit_pos(-1)
{
}

int VvcVpsUnit::deserialize()
{
    int rez = VvcUnit::deserialize();
    if (rez)
        return rez;
    try
    {
        vps_id = m_reader.getBits(4);
        vps_max_layers = m_reader.getBits(6) + 1;
        vector vps_direct_ref_layer_flag(vps_max_layers, vector<bool>(vps_max_layers));

        vps_max_sublayers = m_reader.getBits(3) + 1;
        bool vps_default_ptl_dpb_hrd_max_tid_flag =
            (vps_max_layers > 1 && vps_max_sublayers > 1) ? m_reader.getBit() : true;
        int vps_all_independent_layers_flag = (vps_max_layers > 1) ? m_reader.getBit() : 1;
        for (int i = 0; i < vps_max_layers; i++)
        {
            m_reader.skipBits(6);  // vps_layer_id[i]
            if (i > 0 && !vps_all_independent_layers_flag)
            {
                if (!m_reader.getBit())  // vps_independent_layer_flag[i]
                {
                    bool vps_max_tid_ref_present_flag = m_reader.getBit();
                    for (int j = 0; j < i; j++)
                    {
                        vps_direct_ref_layer_flag[i][j] = m_reader.getBit();
                        if (vps_max_tid_ref_present_flag && vps_direct_ref_layer_flag[i][j])
                            m_reader.skipBits(3);  // vps_max_tid_il_ref_pics_plus1[i][j]
                    }
                }
            }
        }

        bool vps_each_layer_is_an_ols_flag = true;
        int vps_num_ptls = 1;
        int vps_ols_mode_idc = 2;
        int olsModeIdc = 4;
        int TotalNumOlss = vps_max_layers;
        int vps_num_output_layer_sets = 0;

        vector<vector<bool>> vps_ols_output_layer_flag;

        if (vps_max_layers > 1)
        {
            vps_each_layer_is_an_ols_flag = (vps_all_independent_layers_flag ? m_reader.getBit() : 0);
            if (!vps_each_layer_is_an_ols_flag)
            {
                if (!vps_all_independent_layers_flag)
                    vps_ols_mode_idc = m_reader.getBits(2);
                olsModeIdc = vps_ols_mode_idc;

                if (vps_ols_mode_idc == 2)
                {
                    vps_num_output_layer_sets = m_reader.getBits(8) + 2;
                    vps_ols_output_layer_flag.resize(vps_num_output_layer_sets, vector<bool>(vps_max_layers));
                    if (olsModeIdc == 2)
                        TotalNumOlss = vps_num_output_layer_sets;
                    for (int i = 1; i < vps_num_output_layer_sets; i++)
                        for (int j = 0; j < vps_max_layers; j++) vps_ols_output_layer_flag[i][j] = m_reader.getBit();
                }
            }
            vps_num_ptls = m_reader.getBits(8) + 1;
        }

        vector vps_pt_present_flag(vps_num_ptls, true);
        vector vps_ptl_max_tid(vps_num_ptls, vps_max_sublayers - 1);

        if (!vps_default_ptl_dpb_hrd_max_tid_flag)
            vps_ptl_max_tid[0] = m_reader.getBits(3);

        for (int i = 1; i < vps_num_ptls; i++)
        {
            vps_pt_present_flag[i] = m_reader.getBit();
            if (!vps_default_ptl_dpb_hrd_max_tid_flag)
                vps_ptl_max_tid[i] = m_reader.getBits(3);
        }

        m_reader.skipBits(m_reader.getBitsLeft() % 8);  // vps_ptl_alignment_zero_bit

        for (int i = 0; i < vps_num_ptls; i++)
            if (profile_tier_level(vps_pt_present_flag[i], vps_ptl_max_tid[i]) != 0)
                return 1;

        for (int i = 0; i < TotalNumOlss; i++)
            if (vps_num_ptls > 1 && vps_num_ptls != TotalNumOlss)
                m_reader.skipBits(8);  // vps_ols_ptl_idx[i]

        if (!vps_each_layer_is_an_ols_flag)
        {
            vector<vector<bool>> layerIncludedInOlsFlag;

            if (vps_ols_mode_idc == 2)
            {
                layerIncludedInOlsFlag.resize(TotalNumOlss, vector<bool>(vps_max_layers));
                vector dependencyFlag(vps_max_layers, vector<bool>(vps_max_layers));
                vector ReferenceLayerIdx(vps_max_layers, vector<bool>(vps_max_layers));
                vector OutputLayerIdx(TotalNumOlss, vector<bool>(vps_max_layers));
                vector NumRefLayers(vps_max_layers, 0);

                for (int i = 0; i < vps_max_layers; i++)
                {
                    for (int j = 0; j < vps_max_layers; j++)
                    {
                        dependencyFlag[i][j] = vps_direct_ref_layer_flag[i][j];
                        for (int k = 0; k < i; k++)
                            if (vps_direct_ref_layer_flag[i][k] && dependencyFlag[k][j])
                                dependencyFlag[i][j] = true;
                    }
                }

                int r = 0;
                for (int i = 0; i < vps_max_layers; i++)
                {
                    for (int j = 0; j < vps_max_layers; j++)
                    {
                        if (dependencyFlag[i][j])
                            ReferenceLayerIdx[i][r++] = j;
                    }
                    NumRefLayers[i] = r;
                }

                for (int i = 1; i < TotalNumOlss; i++)
                {
                    for (int j = 0; j < vps_max_layers; j++) layerIncludedInOlsFlag[i][j] = false;
                    int j = 0;
                    for (int k = 0; k < vps_max_layers; k++)
                    {
                        if (vps_ols_output_layer_flag[i][k])
                        {
                            layerIncludedInOlsFlag[i][k] = true;
                            OutputLayerIdx[i][j++] = k;
                        }
                    }

                    for (int l = 0; l < j; l++)
                    {
                        int idx = OutputLayerIdx[i][l];
                        for (int k = 0; k < NumRefLayers[idx]; k++)
                        {
                            if (!layerIncludedInOlsFlag[i][ReferenceLayerIdx[idx][k]])
                                layerIncludedInOlsFlag[i][ReferenceLayerIdx[idx][k]] = true;
                        }
                    }
                }
            }

            unsigned NumMultiLayerOlss = 0;

            for (int i = 1; i < TotalNumOlss; i++)
            {
                int NumLayersInOls = 0;
                if (vps_each_layer_is_an_ols_flag)
                    NumLayersInOls = 1;
                else if (vps_ols_mode_idc == 0 || vps_ols_mode_idc == 1)
                    NumLayersInOls = i + 1;
                else if (vps_ols_mode_idc == 2)
                {
                    int j = 0;
                    for (int k = 0; k < vps_max_layers; k++) j += layerIncludedInOlsFlag[i][k];
                    NumLayersInOls = j;
                }

                if (NumLayersInOls > 1)
                    NumMultiLayerOlss++;
            }

            unsigned vps_num_dpb_params = extractUEGolombCode() + 1;
            if (vps_num_dpb_params >= NumMultiLayerOlss)
                return 1;
            unsigned VpsNumDpbParams = (vps_each_layer_is_an_ols_flag ? 0 : vps_num_dpb_params);

            bool vps_sublayer_dpb_params_present_flag = (vps_max_sublayers > 1) ? m_reader.getBit() : false;

            for (size_t i = 0; i < VpsNumDpbParams; i++)
            {
                int vps_dpb_max_tid =
                    (vps_default_ptl_dpb_hrd_max_tid_flag ? vps_max_sublayers - 1 : m_reader.getBits(3));

                if (dpb_parameters(vps_dpb_max_tid, vps_sublayer_dpb_params_present_flag))
                    return 1;
            }

            for (size_t i = 0; i < NumMultiLayerOlss; i++)
            {
                extractUEGolombCode();          // vps_ols_dpb_pic_width
                extractUEGolombCode();          // vps_ols_dpb_pic_height
                m_reader.skipBits(2);           // vps_ols_dpb_chroma_format
                if (extractUEGolombCode() > 2)  // vps_ols_dpb_bitdepth_minus8
                    return 1;
                if (VpsNumDpbParams > 1 && VpsNumDpbParams != NumMultiLayerOlss)
                {
                    if (extractUEGolombCode() >= VpsNumDpbParams)  // vps_ols_dpb_params_idx
                        return 1;
                }
            }
            if (m_reader.getBit())  // vps_timing_hrd_params_present_flag
            {
                if (general_timing_hrd_parameters(m_vps_hrd))
                    return 1;
                bool vps_sublayer_cpb_params_present_flag = (vps_max_sublayers > 1) ? m_reader.getBit() : false;
                unsigned vps_num_ols_timing_hrd_params = extractUEGolombCode() + 1;
                if (vps_num_ols_timing_hrd_params > NumMultiLayerOlss)
                    return 1;
                for (size_t i = 0; i <= vps_num_ols_timing_hrd_params; i++)
                {
                    int vps_hrd_max_tid =
                        (vps_default_ptl_dpb_hrd_max_tid_flag ? vps_max_sublayers - 1 : m_reader.getBits(3));

                    int firstSubLayer = vps_sublayer_cpb_params_present_flag ? 0 : vps_hrd_max_tid;
                    ols_timing_hrd_parameters(m_vps_hrd, firstSubLayer, vps_hrd_max_tid);
                }
                if (vps_num_ols_timing_hrd_params > 1 && vps_num_ols_timing_hrd_params != NumMultiLayerOlss)
                {
                    for (size_t i = 0; i < NumMultiLayerOlss; i++)
                    {
                        if (extractUEGolombCode() >= vps_num_ols_timing_hrd_params)  // vps_ols_timing_hrd_idx
                            return 1;
                    }
                }
            }
        }

        return rez;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

void VvcVpsUnit::setFPS(const double fps)
{
    time_scale = static_cast<uint32_t>(fps + 0.5) * 1000000;
    num_units_in_tick = static_cast<int>(time_scale / fps + 0.5);

    // num_units_in_tick = time_scale/2 / fps;
    assert(num_units_in_tick_bit_pos > 0);
    updateBits(num_units_in_tick_bit_pos, 32, num_units_in_tick);
    updateBits(num_units_in_tick_bit_pos + 32, 32, time_scale);
}

double VvcVpsUnit::getFPS() const { return num_units_in_tick ? time_scale / static_cast<float>(num_units_in_tick) : 0; }

string VvcVpsUnit::getDescription() const
{
    string rez("Frame rate: ");
    const double fps = getFPS();
    if (fps != 0.0)
        rez += doubleToStr(fps);
    else
        rez += string("not found");

    return rez;
}

// ------------------------- VvcSpsUnit ------------------------------

VvcSpsUnit::VvcSpsUnit()
    : sps_id(0),
      vps_id(0),
      max_sublayers_minus1(0),
      chroma_format_idc(0),
      pic_width_max_in_luma_samples(0),
      pic_height_max_in_luma_samples(0),
      bitdepth_minus8(0),
      log2_max_pic_order_cnt_lsb(0),
      progressive_source_flag(true),
      interlaced_source_flag(true),
      non_packed_constraint_flag(true),
      colour_primaries(2),
      transfer_characteristics(2),
      matrix_coeffs(2),  // 2 = unspecified
      full_range_flag(false),
      sps_num_ref_pic_lists(0),
      weighted_pred_flag(false),
      weighted_bipred_flag(false),
      long_term_ref_pics_flag(false),
      inter_layer_prediction_enabled_flag(false)
{
}

int VvcSpsUnit::deserialize()
{
    const int rez = VvcUnit::deserialize();
    if (rez)
        return rez;
    try
    {
        sps_id = m_reader.getBits(4);
        vps_id = m_reader.getBits(4);
        max_sublayers_minus1 = m_reader.getBits(3);
        if (max_sublayers_minus1 == 7)
            return 1;
        chroma_format_idc = m_reader.getBits(2);
        const int sps_log2_ctu_size_minus5 = m_reader.getBits(2);
        if (sps_log2_ctu_size_minus5 > 2)
            return 1;
        const int CtbLog2SizeY = sps_log2_ctu_size_minus5 + 5;
        const unsigned CtbSizeY = 1 << CtbLog2SizeY;
        const bool sps_ptl_dpb_hrd_params_present_flag = m_reader.getBit();
        if (sps_id == 0 && !sps_ptl_dpb_hrd_params_present_flag)
            return 1;
        if (sps_ptl_dpb_hrd_params_present_flag)
            if (profile_tier_level(true, max_sublayers_minus1) != 0)
                return 1;
        m_reader.skipBit();      // sps_gdr_enabled_flag
        if (m_reader.getBit())   // sps_ref_pic_resampling_enabled_flag
            m_reader.skipBit();  // sps_res_change_in_clvs_allowed_flag
        pic_width_max_in_luma_samples = extractUEGolombCode();
        pic_height_max_in_luma_samples = extractUEGolombCode();
        const unsigned tmpWidthVal = (pic_width_max_in_luma_samples + CtbSizeY - 1) / CtbSizeY;
        const unsigned tmpHeightVal = (pic_height_max_in_luma_samples + CtbSizeY - 1) / CtbSizeY;

        if (m_reader.getBit())  // sps_conformance_window_flag
        {
            extractUEGolombCode();  // sps_conf_win_left_offset
            extractUEGolombCode();  // sps_conf_win_right_offset
            extractUEGolombCode();  // sps_conf_win_top_offset
            extractUEGolombCode();  // sps_conf_win_bottom_offset
        }
        if (m_reader.getBit())  // sps_subpic_info_present_flag
        {
            const unsigned sps_num_subpics_minus1 = extractUEGolombCode();
            if (sps_num_subpics_minus1 > 600)
                return 1;
            if (sps_num_subpics_minus1 > 0)
            {
                const bool sps_independent_subpics_flag = m_reader.getBit();
                const bool sps_subpic_same_size_flag = m_reader.getBit();
                for (size_t i = 0; i <= sps_num_subpics_minus1; i++)
                {
                    if (!sps_subpic_same_size_flag || i == 0)
                    {
                        if (i != 0 && pic_width_max_in_luma_samples > CtbSizeY)
                            m_reader.skipBits(
                                static_cast<unsigned>(ceil(log2(tmpWidthVal))));  // sps_subpic_ctu_top_left_x[i]
                        if (i != 0 && pic_height_max_in_luma_samples > CtbSizeY)
                            m_reader.skipBits(
                                static_cast<unsigned>(ceil(log2(tmpHeightVal))));  // sps_subpic_ctu_top_left_y[i]
                        if (i < sps_num_subpics_minus1 && pic_width_max_in_luma_samples > CtbSizeY)
                            m_reader.skipBits(
                                static_cast<unsigned>(ceil(log2(tmpWidthVal))));  // sps_subpic_width_minus1[i]
                        if (i < sps_num_subpics_minus1 && pic_height_max_in_luma_samples > CtbSizeY)
                            m_reader.skipBits(
                                static_cast<unsigned>(ceil(log2(tmpHeightVal))));  // sps_subpic_height_minus1[i]
                    }
                    if (!sps_independent_subpics_flag)
                        m_reader.skipBits(
                            2);  // sps_subpic_treated_as_pic_flag, sps_loop_filter_across_subpic_enabled_flag
                }
            }
            const unsigned sps_subpic_id_len = extractUEGolombCode() + 1;
            if (sps_subpic_id_len > 16 || static_cast<unsigned>(1 << sps_subpic_id_len) < (sps_num_subpics_minus1 + 1))
                return 1;
            if (m_reader.getBit())  // sps_subpic_id_mapping_explicitly_signalled_flag
            {
                if (m_reader.getBit())  // sps_subpic_id_mapping_present_flag
                    for (size_t i = 0; i <= sps_num_subpics_minus1; i++)
                        m_reader.skipBits(sps_subpic_id_len);  // sps_subpic_id[i]
            }
        }
        bitdepth_minus8 = extractUEGolombCode();
        if (bitdepth_minus8 > 2)
            return 1;
        const int QpBdOffset = 6 * bitdepth_minus8;
        m_reader.skipBits(2);  // sps_entropy_coding_sync_enabled_flag, vsps_entry_point_offsets_present_flag
        log2_max_pic_order_cnt_lsb = m_reader.getBits(4) + 4;
        if (log2_max_pic_order_cnt_lsb > 16)
            return 1;
        if (m_reader.getBit())  // sps_poc_msb_cycle_flag
        {
            if (extractUEGolombCode() /* sps_poc_msb_cycle_len_minus1 */ > 23U - log2_max_pic_order_cnt_lsb)
                return 1;
        }
        const int sps_num_extra_ph_bytes = m_reader.getBits(2);
        for (int i = 0; i < sps_num_extra_ph_bytes; i++) m_reader.skipBits(8);  // sps_extra_ph_bit_present_flag[i]
        const int sps_num_extra_sh_bytes = m_reader.getBits(2);
        for (int i = 0; i < sps_num_extra_sh_bytes; i++) m_reader.skipBits(8);  // sps_extra_sh_bit_present_flag[i]
        if (sps_ptl_dpb_hrd_params_present_flag)
        {
            const bool sps_sublayer_dpb_params_flag = (max_sublayers_minus1 > 0) ? m_reader.getBit() : false;
            if (dpb_parameters(max_sublayers_minus1, sps_sublayer_dpb_params_flag))
                return 1;
        }
        const unsigned sps_log2_min_luma_coding_block_size_minus2 = extractUEGolombCode();
        if (sps_log2_min_luma_coding_block_size_minus2 >
            static_cast<unsigned>(min(4, static_cast<int>(sps_log2_ctu_size_minus5) + 3)))
            return 1;
        const unsigned MinCbLog2SizeY = sps_log2_min_luma_coding_block_size_minus2 + 2;
        m_reader.skipBit();  // sps_partition_constraints_override_enabled_flag
        const unsigned sps_log2_diff_min_qt_min_cb_intra_slice_luma = extractUEGolombCode();
        if (sps_log2_diff_min_qt_min_cb_intra_slice_luma > min(6, CtbLog2SizeY) - MinCbLog2SizeY)
            return 1;
        const unsigned MinQtLog2SizeIntraY = sps_log2_diff_min_qt_min_cb_intra_slice_luma + MinCbLog2SizeY;
        const unsigned sps_max_mtt_hierarchy_depth_intra_slice_luma = extractUEGolombCode();
        if (sps_max_mtt_hierarchy_depth_intra_slice_luma > 2 * (CtbLog2SizeY - MinCbLog2SizeY))
            return 1;
        if (sps_max_mtt_hierarchy_depth_intra_slice_luma != 0)
        {
            if (extractUEGolombCode() >  // sps_log2_diff_max_bt_min_qt_intra_slice_luma
                CtbLog2SizeY - MinQtLog2SizeIntraY)
                return 1;
            if (extractUEGolombCode() >  // sps_log2_diff_max_tt_min_qt_intra_slice_luma
                min(6, CtbLog2SizeY) - MinQtLog2SizeIntraY)
                return 1;
        }
        const bool sps_qtbtt_dual_tree_intra_flag = (chroma_format_idc != 0 ? m_reader.getBit() : 0);
        if (sps_qtbtt_dual_tree_intra_flag)
        {
            const unsigned sps_log2_diff_min_qt_min_cb_intra_slice_chroma = extractUEGolombCode();
            if (sps_log2_diff_min_qt_min_cb_intra_slice_chroma > min(6, CtbLog2SizeY) - MinCbLog2SizeY)
                return 1;
            const unsigned MinQtLog2SizeIntraC = sps_log2_diff_min_qt_min_cb_intra_slice_chroma + MinCbLog2SizeY;
            const unsigned sps_max_mtt_hierarchy_depth_intra_slice_chroma = extractUEGolombCode();
            if (sps_max_mtt_hierarchy_depth_intra_slice_chroma > 2 * (CtbLog2SizeY - MinCbLog2SizeY))
                return 1;
            if (sps_max_mtt_hierarchy_depth_intra_slice_chroma != 0)
            {
                if (extractUEGolombCode() >  // sps_log2_diff_max_bt_min_qt_intra_slice_chroma
                    min(6, CtbLog2SizeY) - MinQtLog2SizeIntraC)
                    return 1;
                if (extractUEGolombCode() >  // sps_log2_diff_max_tt_min_qt_intra_slice_chroma
                    min(6, CtbLog2SizeY) - MinQtLog2SizeIntraC)
                    return 1;
            }
        }
        const unsigned sps_log2_diff_min_qt_min_cb_inter_slice = extractUEGolombCode();
        if (sps_log2_diff_min_qt_min_cb_inter_slice > min(6, CtbLog2SizeY) - MinCbLog2SizeY)
            return 1;
        const unsigned MinQtLog2SizeInterY = sps_log2_diff_min_qt_min_cb_inter_slice + MinCbLog2SizeY;
        const unsigned sps_max_mtt_hierarchy_depth_inter_slice = extractUEGolombCode();
        if (sps_max_mtt_hierarchy_depth_inter_slice > 2 * (CtbLog2SizeY - MinCbLog2SizeY))
            return 1;
        if (sps_max_mtt_hierarchy_depth_inter_slice != 0)
        {
            if (extractUEGolombCode() >  // sps_log2_diff_max_bt_min_qt_inter_slice
                CtbLog2SizeY - MinQtLog2SizeInterY)
                return 1;
            if (extractUEGolombCode() >  // sps_log2_diff_max_tt_min_qt_inter_slice
                min(6, CtbLog2SizeY) - MinQtLog2SizeInterY)
                return 1;
        }
        const bool sps_max_luma_transform_size_64_flag = (CtbSizeY > 32 ? m_reader.getBit() : 0);
        const bool sps_transform_skip_enabled_flag = m_reader.getBit();
        if (sps_transform_skip_enabled_flag)
        {
            if (extractUEGolombCode() > 3)  // sps_log2_transform_skip_max_size_minus2
                return 1;
            m_reader.skipBit();  // sps_bdpcm_enabled_flag
        }
        if (m_reader.getBit())     // sps_mts_enabled_flag
            m_reader.skipBits(2);  // sps_explicit_mts_intra_enabled_flag, sps_explicit_mts_inter_enabled_flag

        const bool sps_lfnst_enabled_flag = m_reader.getBit();
        if (chroma_format_idc != 0)
        {
            const bool sps_joint_cbcr_enabled_flag = m_reader.getBit();
            const int numQpTables =
                m_reader.getBit() /* sps_same_qp_table_for_chroma_flag */ ? 1 : (sps_joint_cbcr_enabled_flag ? 3 : 2);
            for (int i = 0; i < numQpTables; i++)
            {
                const int sps_qp_table_start_minus26 = extractSEGolombCode();
                if (sps_qp_table_start_minus26 < (-26 - QpBdOffset) || sps_qp_table_start_minus26 > 36)
                    return 1;
                const unsigned sps_num_points_in_qp_table_minus1 = extractUEGolombCode();
                if (sps_num_points_in_qp_table_minus1 > static_cast<unsigned>(36 - sps_qp_table_start_minus26))
                    return 1;
                for (size_t j = 0; j <= sps_num_points_in_qp_table_minus1; j++)
                {
                    extractUEGolombCode();  // sps_delta_qp_in_val_minus1
                    extractUEGolombCode();  // sps_delta_qp_diff_val
                }
            }
        }
        m_reader.skipBit();  // sps_sao_enabled_flag
        if (m_reader.getBit() /* sps_alf_enabled_flag */ && chroma_format_idc != 0)
            m_reader.skipBit();  // sps_ccalf_enabled_flag
        m_reader.skipBit();      // sps_lmcs_enabled_flag
        weighted_pred_flag = m_reader.getBit();
        weighted_bipred_flag = m_reader.getBit();
        long_term_ref_pics_flag = m_reader.getBit();
        inter_layer_prediction_enabled_flag = (sps_id != 0) ? m_reader.getBit() : false;
        m_reader.skipBit();  // sps_idr_rpl_present_flag
        const bool sps_rpl1_same_as_rpl0_flag = m_reader.getBit();
        for (size_t i = 0; i < (sps_rpl1_same_as_rpl0_flag ? 1 : 2); i++)
        {
            sps_num_ref_pic_lists = extractUEGolombCode();
            if (sps_num_ref_pic_lists > 64)
                return 1;
            for (size_t j = 0; j < sps_num_ref_pic_lists; j++) ref_pic_list_struct(j);
        }
        m_reader.skipBit();  // sps_ref_wraparound_enabled_flag
        const unsigned sps_sbtmvp_enabled_flag =
            (m_reader.getBit()) /* sps_temporal_mvp_enabled_flag */ ? m_reader.getBit() : 0;
        const bool sps_amvr_enabled_flag = m_reader.getBit();
        if (m_reader.getBit())   // sps_bdof_enabled_flag
            m_reader.skipBit();  // sps_bdof_control_present_in_ph_flag
        m_reader.skipBit();      // sps_smvd_enabled_flag
        if (m_reader.getBit())   // sps_dmvr_enabled_flag
            m_reader.skipBit();  // sps_dmvr_control_present_in_ph_flag
        if (m_reader.getBit())   // sps_mmvd_enabled_flag
            m_reader.skipBit();  // sps_mmvd_fullpel_only_enabled_flag
        const unsigned sps_six_minus_max_num_merge_cand = extractUEGolombCode();
        if (sps_six_minus_max_num_merge_cand > 5)
            return 1;
        const unsigned MaxNumMergeCand = 6 - sps_six_minus_max_num_merge_cand;
        m_reader.skipBit();     // sps_sbt_enabled_flag
        if (m_reader.getBit())  // sps_affine_enabled_flag
        {
            const unsigned sps_five_minus_max_num_subblock_merge_cand = extractUEGolombCode();
            if (sps_five_minus_max_num_subblock_merge_cand + sps_sbtmvp_enabled_flag > 5)
                return 1;
            m_reader.skipBit();  // sps_6param_affine_enabled_flag
            if (sps_amvr_enabled_flag)
                m_reader.skipBit();  // sps_affine_amvr_enabled_flag
            if (m_reader.getBit())   // sps_affine_prof_enabled_flag
                m_reader.skipBit();  // sps_prof_control_present_in_ph_flag
        }
        m_reader.skipBits(2);  // sps_bcw_enabled_flag, sps_ciip_enabled_flag
        if (MaxNumMergeCand >= 2)
        {
            if (m_reader.getBit() /* sps_gpm_enabled_flag */ && MaxNumMergeCand >= 3)
            {
                if (extractUEGolombCode() + 2 > MaxNumMergeCand)  // sps_max_num_merge_cand_minus_max_num_gpm_cand
                    return 1;
            }
        }
        if (extractUEGolombCode() + 2 > static_cast<unsigned>(CtbLog2SizeY))  // sps_log2_parallel_merge_level_minus2
            return 1;

        m_reader.skipBits(3);  // sps_isp_enabled_flag, sps_mrl_enabled_flag, sps_mip_enabled_flag
        if (chroma_format_idc != 0)
            m_reader.skipBit();  // sps_cclm_enabled_flag
        if (chroma_format_idc == 1)
            m_reader.skipBits(2);  // sps_chroma_horizontal_collocated_flag, sps_chroma_vertical_collocated_flag

        const bool sps_palette_enabled_flag = m_reader.getBit();
        const bool sps_act_enabled_flag =
            (chroma_format_idc == 3 && !sps_max_luma_transform_size_64_flag) ? m_reader.getBit() : false;
        if (sps_transform_skip_enabled_flag || sps_palette_enabled_flag)
        {
            if (extractUEGolombCode() > 8)  // sps_min_qp_prime_ts
                return 1;
        }
        if (m_reader.getBit())  // sps_ibc_enabled_flag
        {
            if (extractUEGolombCode() > 5)  // sps_six_minus_max_num_ibc_merge_cand
                return 1;
        }
        if (m_reader.getBit())  // sps_ladf_enabled_flag
        {
            const int sps_num_ladf_intervals_minus2 = m_reader.getBits(2);
            const int sps_ladf_lowest_interval_qp_offset = extractSEGolombCode();
            if (sps_ladf_lowest_interval_qp_offset < -63 || sps_ladf_lowest_interval_qp_offset > 63)
                return 1;
            for (int i = 0; i < sps_num_ladf_intervals_minus2 + 1; i++)
            {
                const int sps_ladf_qp_offset = extractSEGolombCode();
                if (sps_ladf_qp_offset < -63 || sps_ladf_qp_offset > 63)
                    return 1;
                if (extractUEGolombCode() > (1U << (bitdepth_minus8 + 8)) - 3)  // sps_ladf_delta_threshold_minus1
                    return 1;
            }
        }
        const bool sps_explicit_scaling_list_enabled_flag = m_reader.getBit();
        if (sps_lfnst_enabled_flag && sps_explicit_scaling_list_enabled_flag)
            m_reader.skipBit();  // sps_scaling_matrix_for_lfnst_disabled_flag
        const bool sps_scaling_matrix_for_alternative_colour_space_disabled_flag =
            (sps_act_enabled_flag && sps_explicit_scaling_list_enabled_flag) ? m_reader.getBit() : false;
        if (sps_scaling_matrix_for_alternative_colour_space_disabled_flag)
            m_reader.skipBit();  // sps_scaling_matrix_designated_colour_space_flag
        m_reader.skipBits(2);    // sps_dep_quant_enabled_flag, sps_sign_data_hiding_enabled_flag
        if (m_reader.getBit())   // sps_virtual_boundaries_enabled_flag
        {
            if (m_reader.getBit())  // sps_virtual_boundaries_present_flag
            {
                const unsigned sps_num_ver_virtual_boundaries = extractUEGolombCode();
                if (sps_num_ver_virtual_boundaries > static_cast<unsigned>(pic_width_max_in_luma_samples <= 8 ? 0 : 3))
                    return 1;
                for (size_t i = 0; i < sps_num_ver_virtual_boundaries; i++)
                {
                    if (extractUEGolombCode() >  // sps_virtual_boundary_pos_x_minus1
                        ceil(pic_width_max_in_luma_samples / 8) - 2)
                        return 1;
                }
                const unsigned sps_num_hor_virtual_boundaries = extractUEGolombCode();
                if (sps_num_hor_virtual_boundaries > static_cast<unsigned>(pic_height_max_in_luma_samples <= 8 ? 0 : 3))
                    return 1;
                for (size_t i = 0; i < sps_num_hor_virtual_boundaries; i++)
                {
                    if (extractUEGolombCode() >  // sps_virtual_boundary_pos_y_minus1
                        ceil(pic_height_max_in_luma_samples / 8) - 2)
                        return 1;
                }
            }
        }
        if (sps_ptl_dpb_hrd_params_present_flag)
        {
            if (m_reader.getBit())  // sps_timing_hrd_params_present_flag
            {
                if (general_timing_hrd_parameters(m_sps_hrd))
                    return 1;
                const int sps_sublayer_cpb_params_present_flag = (max_sublayers_minus1 > 0) ? m_reader.getBit() : 0;
                const int firstSubLayer = sps_sublayer_cpb_params_present_flag ? 0 : max_sublayers_minus1;
                ols_timing_hrd_parameters(m_sps_hrd, firstSubLayer, max_sublayers_minus1);
            }
        }
        m_reader.skipBit();     // sps_field_seq_flag
        if (m_reader.getBit())  // sps_vui_parameters_present_flag
        {
            if (extractUEGolombCode() > 1023)  // sps_vui_payload_size_minus1
                return 1;
            m_reader.skipBits(m_reader.getBitsLeft() % 8);  // sps_vui_alignment_zero_bit
            vui_parameters();
        }
        m_reader.skipBit();  // sps_extension_flag
        return 0;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

int VvcSpsUnit::ref_pic_list_struct(const size_t rplsIdx)
{
    const unsigned num_ref_entries = extractUEGolombCode();
    bool ltrp_in_header_flag = true;
    if (long_term_ref_pics_flag && rplsIdx < sps_num_ref_pic_lists && num_ref_entries > 0)
        ltrp_in_header_flag = m_reader.getBit();
    for (size_t i = 0; i < num_ref_entries; i++)
    {
        const bool inter_layer_ref_pic_flag = (inter_layer_prediction_enabled_flag) ? m_reader.getBit() : false;
        if (!inter_layer_ref_pic_flag)
        {
            const bool st_ref_pic_flag = (long_term_ref_pics_flag) ? m_reader.getBit() : true;
            if (st_ref_pic_flag)
            {
                const unsigned abs_delta_poc_st = extractUEGolombCode();
                if (abs_delta_poc_st > (2 << 14) - 1)
                    return 1;
                unsigned AbsDeltaPocSt = abs_delta_poc_st + 1;
                if ((weighted_pred_flag || weighted_bipred_flag) && i != 0)
                    AbsDeltaPocSt -= 1;
                if (AbsDeltaPocSt > 0)
                    m_reader.skipBit();  // strp_entry_sign_flag
            }
            else if (!ltrp_in_header_flag)
                m_reader.skipBits(log2_max_pic_order_cnt_lsb);  // rpls_poc_lsb_lt
        }
        else
            extractUEGolombCode();  // ilrp_idx
    }
    return 0;
}

double VvcSpsUnit::getFPS() const
{
    return m_sps_hrd.num_units_in_tick ? m_sps_hrd.time_scale / static_cast<float>(m_sps_hrd.num_units_in_tick) : 0;
}

string VvcSpsUnit::getDescription() const
{
    string result = getProfileString();
    result += string(" Resolution: ") + int32ToStr(pic_width_max_in_luma_samples) + string(":") +
              int32ToStr(pic_height_max_in_luma_samples) + string("p");

    const double fps = getFPS();
    result += "  Frame rate: ";
    result += (fps ? doubleToStr(fps) : string("not found"));
    return result;
}

/* Specified in Rec. ITU-T H.274 */
int VvcSpsUnit::vui_parameters()
{
    progressive_source_flag = m_reader.getBit();
    interlaced_source_flag = m_reader.getBit();
    non_packed_constraint_flag = m_reader.getBit();
    m_reader.skipBit();  // non_projected_constraint_flag

    if (m_reader.getBit())  // aspect_ratio_info_present_flag
    {
        if (m_reader.getBits(8) == EXTENDED_SAR)  // aspect_ratio_idc
            m_reader.skipBits(32);                // sar_width, sar_height
    }

    if (m_reader.getBit())   // overscan_info_present_flag
        m_reader.skipBit();  // overscan_appropriate_flag
    if (m_reader.getBit())   // colour_description_present_flag
    {
        colour_primaries = m_reader.getBits(8);
        transfer_characteristics = m_reader.getBits(8);
        matrix_coeffs = m_reader.getBits(8);
        full_range_flag = m_reader.getBit();
    }

    if (m_reader.getBit())  // chroma_loc_info_present_flag
    {
        if (progressive_source_flag && !interlaced_source_flag)
        {
            if (extractUEGolombCode() > 6)  // chroma_sample_loc_type_frame
                return 1;
        }
        else
        {
            if (extractUEGolombCode() > 6 ||  // chroma_sample_loc_type_top_field
                extractUEGolombCode() > 6)    // chroma_sample_loc_type_bottom_field
                return 1;
        }
    }
    return 0;
}

// ----------------------- VvcPpsUnit ------------------------
VvcPpsUnit::VvcPpsUnit() : pps_id(-1), sps_id(-1) {}

int VvcPpsUnit::deserialize()
{
    const int rez = VvcUnit::deserialize();
    if (rez)
        return rez;

    try
    {
        pps_id = m_reader.getBits(6);
        sps_id = m_reader.getBits(4);
        // bool pps_mixed_nalu_types_in_pic_flag= m_reader.getBit();
        // int pps_pic_width_in_luma_samples = extractUEGolombCode();
        // int pps_pic_height_in_luma_samples = extractUEGolombCode();

        return 0;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

// ----------------------- VvcHrdUnit ------------------------
VvcHrdUnit::VvcHrdUnit()
    : num_units_in_tick(0),
      time_scale(0),
      general_nal_hrd_params_present_flag(false),
      general_vcl_hrd_params_present_flag(false),
      general_du_hrd_params_present_flag(false),
      hrd_cpb_cnt_minus1(0)
{
}

bool VvcUnit::general_timing_hrd_parameters(VvcHrdUnit& m_hrd)
{
    m_hrd.num_units_in_tick = m_reader.get32Bits();
    m_hrd.time_scale = m_reader.get32Bits();
    m_hrd.general_nal_hrd_params_present_flag = m_reader.getBit();
    m_hrd.general_vcl_hrd_params_present_flag = m_reader.getBit();
    if (m_hrd.general_nal_hrd_params_present_flag || m_hrd.general_vcl_hrd_params_present_flag)
    {
        m_reader.skipBit();  // general_same_pic_timing_in_all_ols_flag
        m_hrd.general_du_hrd_params_present_flag = m_reader.getBit();
        if (m_hrd.general_du_hrd_params_present_flag)
            m_reader.skipBits(8);  // tick_divisor_minus2
        m_reader.skipBits(8);      // bit_rate_scale, cpb_size_scale
        if (m_hrd.general_du_hrd_params_present_flag)
            m_reader.skipBits(4);  // cpb_size_du_scale
        m_hrd.hrd_cpb_cnt_minus1 = extractUEGolombCode();
        if (m_hrd.hrd_cpb_cnt_minus1 > 31)
            return true;
    }
    return false;
}

bool VvcUnit::ols_timing_hrd_parameters(const VvcHrdUnit m_hrd, const int firstSubLayer, const int MaxSubLayersVal)
{
    for (int i = firstSubLayer; i <= MaxSubLayersVal; i++)
    {
        const bool fixed_pic_rate_within_cvs_flag =
            m_reader.getBit() /* fixed_pic_rate_general_flag) */ ? true : m_reader.getBit();
        if (fixed_pic_rate_within_cvs_flag)
        {
            if (extractUEGolombCode() > 2047)  // elemental_duration_in_tc_minus1
                return true;
        }
        else if ((m_hrd.general_nal_hrd_params_present_flag || m_hrd.general_vcl_hrd_params_present_flag) &&
                 m_hrd.hrd_cpb_cnt_minus1 == 0)
            m_reader.skipBit();  // low_delay_hrd_flag
        if (m_hrd.general_nal_hrd_params_present_flag)
            sublayer_hrd_parameters(m_hrd);
        if (m_hrd.general_vcl_hrd_params_present_flag)
            sublayer_hrd_parameters(m_hrd);
    }
    return false;
}

bool VvcUnit::sublayer_hrd_parameters(const VvcHrdUnit m_hrd)
{
    for (int j = 0; j <= m_hrd.hrd_cpb_cnt_minus1; j++)
    {
        if (extractUEGolombCode() == 0xffffffff ||  // bit_rate_value_minus1
            extractUEGolombCode() == 0xffffffff)    // cpb_size_value_minus1
            return true;
        if (m_hrd.general_du_hrd_params_present_flag)
        {
            if (extractUEGolombCode() == 0xffffffff ||  // cpb_size_du_value_minus1
                extractUEGolombCode() == 0xffffffff)    // bit_rate_du_value_minus1
                return true;
        }
        m_reader.skipBit();  // cbr_flag
    }
    return false;
}

// -----------------------  VvcSliceHeader() -------------------------------------

VvcSliceHeader::VvcSliceHeader() : ph_pps_id(-1), pic_order_cnt_lsb(0) {}

int VvcSliceHeader::deserialize(const VvcSpsUnit* sps, const VvcPpsUnit* pps)
{
    const int rez = VvcUnit::deserialize();
    if (rez)
        return rez;

    try
    {
        if (m_reader.getBit())  // sh_picture_header_in_slice_header_flag
        {
            const bool ph_gdr_or_irap_pic_flag = m_reader.getBit();
            m_reader.skipBit();  // ph_non_ref_pic_flag
            if (ph_gdr_or_irap_pic_flag)
                m_reader.skipBit();  // ph_gdr_pic_flag
            if (m_reader.getBit())   // ph_inter_slice_allowed_flag
                m_reader.skipBit();  // ph_intra_slice_allowed_flag
            const unsigned ph_pps_id = extractUEGolombCode();
            if (ph_pps_id > 63)
                return 1;
            pic_order_cnt_lsb = m_reader.getBits(sps->log2_max_pic_order_cnt_lsb);
            ;
        }

        return 0;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

bool VvcSliceHeader::isIDR() const
{
    return nal_unit_type == NalType::IDR_W_RADL || nal_unit_type == NalType::IDR_N_LP;
}

vector<vector<uint8_t>> vvc_extract_priv_data(const uint8_t* buff, int size, int* nal_size)
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
            THROW(ERR_MOV_PARSE, "Invalid VVC extra data format");
        src++;  // type
        int cnt = AV_RB16(src);
        src += 2;

        for (int j = 0; j < cnt; ++j)
        {
            if (src + 2 > end)
                THROW(ERR_MOV_PARSE, "Invalid VVC extra data format");
            int nalSize = (src[0] << 8) + src[1];
            src += 2;
            if (src + nalSize > end)
                THROW(ERR_MOV_PARSE, "Invalid VVC extra data format");
            if (nalSize > 0)
            {
                spsPps.emplace_back();
                for (int k = 0; k < nalSize; ++k, ++src) spsPps.rbegin()->push_back(*src);
            }
        }
    }

    return spsPps;
}
