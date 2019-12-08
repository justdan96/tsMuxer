#ifndef __HEVC_H_
#define __HEVC_H_

#include "nalUnits.h"

enum HevcSliceTypes
{
    HEVC_BFRAME_SLICE = 0,
    HEVC_PFRAME_SLICE = 1,
    HEVC_IFRAME_SLICE = 2
};

enum HEVCUnitType {
    NAL_TRAIL_N    = 0, // first slice
    NAL_TRAIL_R    = 1,
    NAL_TSA_N      = 2,
    NAL_TSA_R      = 3,
    NAL_STSA_N     = 4,
    NAL_STSA_R     = 5,
    NAL_RADL_N     = 6,
    NAL_RADL_R     = 7,
    NAL_RASL_N     = 8,
    NAL_RASL_R     = 9, 
    NAL_BLA_W_LP   = 16,
    NAL_BLA_W_RADL = 17,
    NAL_BLA_N_LP   = 18,
    NAL_IDR_W_RADL = 19,
    NAL_IDR_N_LP   = 20,
    NAL_CRA_NUT    = 21, 
    NAL_RSV_IRAP_VCL22 = 22, // reserved
    NAL_RSV_IRAP_VCL23 = 23, // reserved, last slice

    NAL_VPS        = 32,
    NAL_SPS        = 33,
    NAL_PPS        = 34,
    NAL_AUD        = 35,
    NAL_EOS_NUT    = 36,
    NAL_EOB_NUT    = 37,
    NAL_FD_NUT     = 38,
    NAL_SEI_PREFIX = 39,
    NAL_SEI_SUFFIX = 40,
};

struct HevcUnit
{
    HevcUnit(): nal_unit_type(0), nuh_layer_id(0), nuh_temporal_id_plus1(0), m_nalBuffer(0), m_nalBufferLen(0) {}

    void decodeBuffer(const uint8_t* buffer, const uint8_t* end);
    virtual int deserialize();
    int serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd) const;

    int nalBufferLen() const { return m_nalBufferLen; }
public:
    int nal_unit_type;
    int nuh_layer_id;
    int nuh_temporal_id_plus1;
protected:
    int extractUEGolombCode();
    int extractSEGolombCode();
    void updateBits(int bitOffset, int bitLen, int value);
protected:
    uint8_t* m_nalBuffer;
    int m_nalBufferLen;
    BitStreamReader m_reader;
};

struct HevcUnitWithProfile: public HevcUnit
{
    HevcUnitWithProfile();
    std::string getProfileString() const;
public:
    int profile_idc;
    int level_idc;
    int interlaced_source_flag;
    bool sub_layer_profile_present_flag[16];
    bool sub_layer_level_present_flag[16];
    uint8_t sub_layer_profile_space[16];
protected:
    void profile_tier_level(int subLayers);
};

struct HevcVpsUnit: public HevcUnitWithProfile
{
    HevcVpsUnit();
    int deserialize() override;
    double getFPS() const;
    void setFPS(double value);
    std::string getDescription() const;
public:    
    int vps_id;
    int vps_max_layers;
    int vps_max_sub_layers;
    int num_units_in_tick;
    int time_scale;
    int num_units_in_tick_bit_pos;
};

static const int MAX_REFS = 64;

struct ShortTermRPS
{
    ShortTermRPS(): num_negative_pics(0), num_delta_pocs(0)
    {
        memset(delta_poc, 0, sizeof(delta_poc));
        memset(used, 0, sizeof(used));
    }
    int num_negative_pics;
    int num_delta_pocs;
    int32_t delta_poc[MAX_REFS];
    uint8_t used[MAX_REFS];
};

struct HevcSpsUnit: public HevcUnitWithProfile
{
    HevcSpsUnit();
    int deserialize() override;

    std::string getDescription() const;
public:
    int vps_id;
    int max_sub_layers;
    int sps_id;
    int crhomaFormat;
    bool separate_colour_plane_flag;
    int pic_width_in_luma_samples;
    int pic_height_in_luma_samples;
    int bit_depth_luma_minus8;
    int bit_depth_chroma_minus8;
    int log2_max_pic_order_cnt_lsb;
    bool nal_hrd_parameters_present_flag;
    bool vcl_hrd_parameters_present_flag;
    bool sub_pic_hrd_params_present_flag;

    std::vector<int> cpb_cnt_minus1;
    /*
    std::vector<int> NumNegativePics;
    std::vector<int> NumPositivePics;
    std::vector<std::vector<int> > UsedByCurrPicS0;
    std::vector<std::vector<int> > UsedByCurrPicS1;
    std::vector<std::vector<int> > DeltaPocS0;
    std::vector<std::vector<int> > DeltaPocS1;
    std::vector<int> used_by_curr_pic_flag;
    std::vector<int> use_delta_flag;
    */
    std::vector<ShortTermRPS> st_rps;

    int num_short_term_ref_pic_sets;
    int num_units_in_tick;
    int time_scale;
    int PicSizeInCtbsY_bits;
    
private:
    void hrd_parameters(bool commonInfPresentFlag, int maxNumSubLayersMinus1);
    void sub_layer_hrd_parameters(int subLayerId );
    void short_term_ref_pic_set(int stRpsIdx );
    void vui_parameters();
    void scaling_list_data();
};

struct HevcPpsUnit: public HevcUnit
{
    HevcPpsUnit();
    int deserialize() override;
public:
    int pps_id;
    int sps_id;
    bool dependent_slice_segments_enabled_flag;
    bool output_flag_present_flag;
    int num_extra_slice_header_bits;
};

struct HevcSliceHeader: public HevcUnit
{
    HevcSliceHeader();
    int deserialize(const HevcSpsUnit* sps, const HevcPpsUnit* pps);
    bool isIDR() const;
public:
    bool first_slice;
    int pps_id;
    int slice_type;
    int pic_order_cnt_lsb;
};

std::vector<std::vector<uint8_t> > hevc_extract_priv_data(const uint8_t* buff, int size, int* nal_size);

#endif // __HEVC_H_
