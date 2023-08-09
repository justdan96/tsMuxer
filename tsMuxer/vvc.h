#ifndef __VVC_H_
#define __VVC_H_

#include "nalUnits.h"

struct VvcHrdUnit
{
    VvcHrdUnit();

   public:
    unsigned num_units_in_tick;
    unsigned time_scale;
    bool general_nal_hrd_params_present_flag;
    bool general_vcl_hrd_params_present_flag;
    bool general_du_hrd_params_present_flag;
    int hrd_cpb_cnt_minus1;
};

struct VvcUnit
{
    VvcUnit()
        : nal_unit_type(), nuh_layer_id(0), nuh_temporal_id_plus1(0), m_nalBuffer(0), m_nalBufferLen(0), m_reader()
    {
    }

   public:
    enum class NalType
    {
        TRAIL = 0,
        STSA = 1,
        RADL = 2,
        RASL = 3,
        IDR_W_RADL = 7,
        IDR_N_LP = 8,
        CRA = 9,
        GDR = 10,
        OPI = 12,
        VPS = 14,
        SPS = 15,
        PPS = 16,
        SUFFIX_APS = 18,
        AUD = 20,
        EOS = 21,
        EOB = 22,
        SUFFIX_SEI = 24,
        FD = 25,
        RSV_NVCL_27 = 27,
        UNSPEC_30 = 30,
        UNSPEC_31 = 31
    };

    void decodeBuffer(const uint8_t* buffer, const uint8_t* end);
    int deserialize();
    int serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd) const;

    int nalBufferLen() const { return m_nalBufferLen; }

   public:
    NalType nal_unit_type;
    int nuh_layer_id;
    int nuh_temporal_id_plus1;

   protected:
    unsigned extractUEGolombCode();
    int extractSEGolombCode();
    void updateBits(int bitOffset, int bitLen, int value) const;
    bool dpb_parameters(int MaxSubLayersMinus1, bool subLayerInfoFlag);
    bool general_timing_hrd_parameters(VvcHrdUnit& m_hrd);
    bool ols_timing_hrd_parameters(VvcHrdUnit m_hrd, int firstSubLayer, int MaxSubLayersVal);
    bool sublayer_hrd_parameters(VvcHrdUnit m_hrd);

   protected:
    uint8_t* m_nalBuffer;
    int m_nalBufferLen;
    BitStreamReader m_reader;
};

struct VvcUnitWithProfile : public VvcUnit
{
    VvcUnitWithProfile();
    std::string getProfileString() const;

   public:
    int profile_idc;
    int tier_flag;
    int level_idc;
    bool ptl_frame_only_constraint_flag;
    int ptl_num_sub_profiles;
    std::vector<int> general_sub_profile_idc;

   protected:
    int profile_tier_level(bool profileTierPresentFlag, int MaxNumSubLayersMinus1);
};

struct VvcVpsUnit : public VvcUnitWithProfile
{
    VvcVpsUnit();
    int deserialize();
    double getFPS() const;
    void setFPS(double value);
    std::string getDescription() const;

   public:
    int vps_id;
    int vps_max_layers;
    int vps_max_sublayers;
    int num_units_in_tick;
    int time_scale;
    int num_units_in_tick_bit_pos;
    VvcHrdUnit m_vps_hrd;
};

struct VvcSpsUnit : public VvcUnitWithProfile
{
    VvcSpsUnit();
    int deserialize();
    double getFPS() const;
    std::string getDescription() const;

   public:
    int sps_id;
    int vps_id;
    int max_sublayers_minus1;
    int chroma_format_idc;
    unsigned pic_width_max_in_luma_samples;
    unsigned pic_height_max_in_luma_samples;
    unsigned bitdepth_minus8;
    unsigned log2_max_pic_order_cnt_lsb;
    VvcHrdUnit m_sps_hrd;

    std::vector<unsigned> cpb_cnt_minus1;

    // T-Rec. H.274 7.2 vui_parameters()
    bool progressive_source_flag;
    bool interlaced_source_flag;
    bool non_packed_constraint_flag;
    int colour_primaries;
    int transfer_characteristics;
    int matrix_coeffs;
    bool full_range_flag;

   private:
    int ref_pic_list_struct(size_t rplsIdx);
    unsigned sps_num_ref_pic_lists;
    bool weighted_pred_flag;
    bool weighted_bipred_flag;
    bool long_term_ref_pics_flag;
    bool inter_layer_prediction_enabled_flag;
    int vui_parameters();
};

struct VvcPpsUnit : public VvcUnit
{
    VvcPpsUnit();
    int deserialize();

   public:
    int pps_id;
    int sps_id;
};

struct VvcSliceHeader : public VvcUnit
{
    VvcSliceHeader();
    int deserialize(const VvcSpsUnit* sps, const VvcPpsUnit* pps);
    bool isIDR() const;

   public:
    unsigned ph_pps_id;
    int pic_order_cnt_lsb;
};

std::vector<std::vector<uint8_t>> vvc_extract_priv_data(const uint8_t* buff, int size, int* nal_size);

#endif  // __VVC_H_
