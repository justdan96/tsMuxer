#ifndef __VVC_H_
#define __VVC_H_

#include "nalUnits.h"

enum VvcSliceTypes
{
    VVC_BFRAME_SLICE = 0,
    VVC_PFRAME_SLICE = 1,
    VVC_IFRAME_SLICE = 2
};

enum VVCUnitType
{
    V_TRAIL = 0,  // first slice
    V_STSA = 1,
    V_RADL = 2,
    V_RASL = 3,
    V_IDR_W_RADL = 7,
    V_IDR_N_LP = 8,
    V_CRA = 9,
    V_GDR = 10,
    V_RSV_IRAP_11 = 11,
    V_OPI = 12,
    V_DCI = 13,
    V_VPS = 14,
    V_SPS = 15,
    V_PPS = 16,
    V_PREFIX_APS = 17,
    V_SUFFIX_APS = 18,
    V_PH = 19,
    V_AUD = 20,
    V_EOS = 21,
    V_EOB = 22,
    V_PREFIX_SEI = 23,
    V_SUFFIX_SEI = 24,
    V_FD = 25
};

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
    VvcUnit() : nal_unit_type(0), nuh_layer_id(0), nuh_temporal_id_plus1(0), m_nalBuffer(0), m_nalBufferLen(0) {}

    void decodeBuffer(const uint8_t* buffer, const uint8_t* end);
    virtual int deserialize();
    int serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd) const;

    int nalBufferLen() const { return m_nalBufferLen; }

   public:
    int nal_unit_type;
    int nuh_layer_id;
    int nuh_temporal_id_plus1;

   protected:
    unsigned extractUEGolombCode();
    int extractSEGolombCode();
    void updateBits(int bitOffset, int bitLen, int value);
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
    int level_idc;

   protected:
    int profile_tier_level(bool profileTierPresentFlag, int MaxNumSubLayersMinus1);
};

struct VvcVpsUnit : public VvcUnitWithProfile
{
    VvcVpsUnit();
    int deserialize() override;
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
    int deserialize() override;
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

    int colour_primaries;
    int transfer_characteristics;
    int matrix_coeffs;
    bool full_range_flag;

   private:
    int ref_pic_list_struct(int rplsIdx);
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
    int deserialize() override;

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
