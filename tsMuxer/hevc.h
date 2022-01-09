#ifndef __HEVC_H_
#define __HEVC_H_

#include "nalUnits.h"

struct HevcUnit
{
    HevcUnit()
        : nal_unit_type(), nuh_layer_id(0), nuh_temporal_id_plus1(0), m_nalBuffer(0), m_nalBufferLen(0), m_reader()
    {
    }

   public:
    enum class NalType
    {
        TRAIL_N = 0,  // first slice
        TRAIL_R = 1,
        TSA_N = 2,
        TSA_R = 3,
        STSA_N = 4,
        STSA_R = 5,
        RADL_N = 6,
        RADL_R = 7,
        RASL_N = 8,
        RASL_R = 9,
        BLA_W_LP = 16,
        BLA_W_RADL = 17,
        BLA_N_LP = 18,
        IDR_W_RADL = 19,
        IDR_N_LP = 20,
        CRA = 21,
        RSV_IRAP_VCL22 = 22,  // reserved
        RSV_IRAP_VCL23 = 23,  // reserved, last slice

        VPS = 32,
        SPS = 33,
        PPS = 34,
        AUD = 35,
        EOS = 36,
        EOB = 37,
        FD = 38,
        SEI_PREFIX = 39,
        SEI_SUFFIX = 40,
        RSV_NVCL45 = 45,
        RSV_NVCL47 = 47,
        UNSPEC56 = 56,
        DVRPU = 62,
        DVEL = 63,
    };


    void decodeBuffer(const uint8_t* buffer, const uint8_t* end);
    virtual int deserialize();
    int serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd) const;

    int nalBufferLen() const { return m_nalBufferLen; }

   public:
    NalType nal_unit_type;
    int nuh_layer_id;
    int nuh_temporal_id_plus1;

   protected:
    unsigned extractUEGolombCode();
    int extractSEGolombCode();
    void updateBits(int bitOffset, int bitLen, int value);

   protected:
    uint8_t* m_nalBuffer;
    int m_nalBufferLen;
    BitStreamReader m_reader;
};

struct HevcUnitWithProfile : public HevcUnit
{
    HevcUnitWithProfile();
    std::string getProfileString() const;

   public:
    int profile_idc;
    int level_idc;
    int interlaced_source_flag;

   protected:
    int profile_tier_level(int subLayers);
};

struct HevcVpsUnit : public HevcUnitWithProfile
{
    HevcVpsUnit();
    int deserialize() override;
    double getFPS() const;
    void setFPS(double value);
    std::string getDescription() const;

   public:
    int vps_id;
    unsigned num_units_in_tick;
    unsigned time_scale;
    int num_units_in_tick_bit_pos;
};

static const int MAX_REFS = 64;

struct ShortTermRPS
{
    ShortTermRPS() : num_negative_pics(0), num_delta_pocs(0)
    {
        memset(delta_poc, 0, sizeof(delta_poc));
        memset(used, 0, sizeof(used));
    }
    unsigned num_negative_pics;
    int num_delta_pocs;
    int32_t delta_poc[MAX_REFS];
    uint8_t used[MAX_REFS];
};

struct HevcSpsUnit : public HevcUnitWithProfile
{
    HevcSpsUnit();
    int deserialize() override;
    double getFPS() const;
    std::string getDescription() const;

   public:
    unsigned vps_id;
    int max_sub_layers;
    unsigned sps_id;
    unsigned chromaFormat;
    bool separate_colour_plane_flag;
    unsigned pic_width_in_luma_samples;
    unsigned pic_height_in_luma_samples;
    unsigned bit_depth_luma_minus8;
    unsigned bit_depth_chroma_minus8;
    unsigned log2_max_pic_order_cnt_lsb;
    bool nal_hrd_parameters_present_flag;
    bool vcl_hrd_parameters_present_flag;
    bool sub_pic_hrd_params_present_flag;

    std::vector<unsigned> cpb_cnt_minus1;
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

    int colour_primaries;
    int transfer_characteristics;
    int matrix_coeffs;
    unsigned chroma_sample_loc_type_top_field;
    unsigned chroma_sample_loc_type_bottom_field;

    unsigned num_short_term_ref_pic_sets;
    unsigned num_units_in_tick;
    unsigned time_scale;
    unsigned PicSizeInCtbsY_bits;

   private:
    int hrd_parameters(bool commonInfPresentFlag, int maxNumSubLayersMinus1);
    int sub_layer_hrd_parameters(int subLayerId);
    int short_term_ref_pic_set(int stRpsIdx);
    int vui_parameters();
    int scaling_list_data();
};

struct HevcPpsUnit : public HevcUnit
{
    HevcPpsUnit();
    int deserialize() override;

   public:
    unsigned pps_id;
    unsigned sps_id;
    bool dependent_slice_segments_enabled_flag;
    bool output_flag_present_flag;
    int num_extra_slice_header_bits;
};

struct HevcHdrUnit : public HevcUnit
{
    HevcHdrUnit();
    int deserialize() override;

   public:
    bool isHDR10;
    bool isHDR10plus;
    bool isDVRPU;
    bool isDVEL;
};

struct HevcSliceHeader : public HevcUnit
{
    HevcSliceHeader();
    int deserialize(const HevcSpsUnit* sps, const HevcPpsUnit* pps);
    bool isIDR() const;

   public:
    bool first_slice;
    unsigned pps_id;
    unsigned slice_type;
    int pic_order_cnt_lsb;
};

std::vector<std::vector<uint8_t>> hevc_extract_priv_data(const uint8_t* buff, int size, int* nal_size);

#endif  // __HEVC_H_
