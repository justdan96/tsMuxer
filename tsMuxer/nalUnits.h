#ifndef NAL_UNITS_H_
#define NAL_UNITS_H_

#include <memory.h>
#include <types/types.h>

#include <map>
#include <set>
#include <unordered_set>
#include <vector>

#include "bitStream.h"

static constexpr int Extended_SAR = 255;
static constexpr double h264_ar_coeff[] = {
    0.0,         1.0,          12.0 / 11.0, 10.0 / 11.0, 16.0 / 11.0, 40.0 / 33.0,
    24.0 / 11.0, 20.0 / 11.0,  32.0 / 11.0, 80.0 / 33.0, 18.0 / 11.0, 15.0 / 11.0,
    64.0 / 33.0, 160.0 / 99.0, 4.0 / 3.0,   3.0 / 2.0,   2.0 / 1.0,
};

static constexpr int SEI_MSG_BUFFERING_PERIOD = 0;
static constexpr int SEI_MSG_PIC_TIMING = 1;
static constexpr int SEI_MSG_MVC_SCALABLE_NESTING = 37;

class NALUnit
{
   public:
    enum class NALType
    {
        nuUnspecified,
        nuSliceNonIDR,
        nuSliceA,
        nuSliceB,
        nuSliceC,  // 0..4
        nuSliceIDR,
        nuSEI,
        nuSPS,
        nuPPS,
        nuDelimiter,  // 5..9
        nuEOSeq,
        nuEOStream,
        nuFillerData,
        nuSPSExt,  // 10..13
        nuNalPrefix,
        nuSubSPS,  // 14.. 15 (new)
        nuReserved3,
        nuReserved4,
        nuReserved5,
        nuSliceWithoutPartitioning,  // 16..19
        nuSliceExt,                  // 20 (new)
        nuReserved7,
        nuReserved8,
        nuReserved9,  // 21..23
        STAP_A = 24,
        nuDRD = 24,  // blu-ray delimiter for MVC part instead of 0x09
        STAP_B,
        MTAP16,
        MTAP24,
        FU_A,
        FU_B,
        nuDummy
    };

    static constexpr int SPS_OR_PPS_NOT_READY = 1;
    // const static int NOT_ENOUGH_BUFFER = 2;
    static constexpr int UNSUPPORTED_PARAM = 3;
    static constexpr int NOT_FOUND = 4;

    NALType nal_unit_type;
    int nal_ref_idc;

    uint8_t* m_nalBuffer;
    int m_nalBufferLen;

    NALUnit(uint8_t nalUnitType) : nal_unit_type(), nal_ref_idc(0), m_nalBuffer(nullptr), m_nalBufferLen(0), bitReader()
    {
    }
    NALUnit() : nal_unit_type(), nal_ref_idc(0), m_nalBuffer(nullptr), m_nalBufferLen(0), bitReader() {}
    // NALUnit(const NALUnit& other);
    virtual ~NALUnit() { delete[] m_nalBuffer; }
    static uint8_t* findNextNAL(uint8_t* buffer, uint8_t* end);
    static uint8_t* findNALWithStartCode(uint8_t* buffer, uint8_t* end, bool longCodesAllowed);
    static int encodeNAL(uint8_t* srcBuffer, uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize);
    static int decodeNAL(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize);
    static int decodeNAL2(uint8_t* srcBuffer, uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize,
                          bool* keepSrcBuffer);  // do not copy buffer if nothink to decode
    virtual int deserialize(uint8_t* buffer, uint8_t* end);
    virtual int serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd, bool writeStartCode) const;
    virtual int serialize(uint8_t* dstBuffer);
    // void setBuffer(uint8_t* buffer, uint8_t* end);
    void decodeBuffer(const uint8_t* buffer, const uint8_t* end);
    static uint8_t* addStartCode(uint8_t* buffer, uint8_t* boundStart);

    static unsigned extractUEGolombCode(uint8_t* buffer, uint8_t* bufEnd);
    static unsigned extractUEGolombCode(BitStreamReader& bitReader);
    static void writeUEGolombCode(BitStreamWriter& bitWriter, uint32_t value);
    static void writeSEGolombCode(BitStreamWriter& bitWriter, int32_t value);
    const BitStreamReader& getBitReader() const { return bitReader; }
    static void write_rbsp_trailing_bits(BitStreamWriter& writer);
    static void write_byte_align_bits(BitStreamWriter& writer);

    static int calcNalLenInBits(const uint8_t* nalBuffer, const uint8_t* end);  // for NAL with rbspTrailingBits only

   protected:
    // GetBitContext getBitContext;
    BitStreamReader bitReader;
    inline unsigned extractUEGolombCode();
    inline int extractSEGolombCode();
    void updateBits(int bitOffset, int bitLen, int value) const;
};

class NALDelimiter : public NALUnit
{
   public:
    static constexpr int PCT_I_FRAMES = 0;
    static constexpr int PCT_IP_FRAMES = 1;
    static constexpr int PCT_IPB_FRAMES = 2;
    static constexpr int PCT_SI_FRAMES = 3;
    static constexpr int PCT_SI_SP_FRAMES = 4;
    static constexpr int PCT_I_SI_FRAMES = 5;
    static constexpr int PCT_I_SI_P_SP_FRAMES = 6;
    static constexpr int PCT_I_SI_P_SP_B_FRAMES = 7;
    int primary_pic_type;
    NALDelimiter() : NALUnit(), primary_pic_type(0) {}
    int deserialize(uint8_t* buffer, uint8_t* end) override;
    int serialize(uint8_t* buffer) override;
};

class PPSUnit final : public NALUnit
{
   public:
    unsigned pic_parameter_set_id;
    unsigned seq_parameter_set_id;
    int entropy_coding_mode_flag;
    int pic_order_present_flag;

    PPSUnit()
        : pic_parameter_set_id(0),
          seq_parameter_set_id(0),
          entropy_coding_mode_flag(true),
          pic_order_present_flag(true),
          m_ready(false)
    {
    }
    ~PPSUnit() override = default;

    bool isReady() const { return m_ready; }
    int deserialize();

   private:
    bool m_ready;
    using NALUnit::deserialize;
};

struct HRDParams
{
    HRDParams();
    ~HRDParams();
    void resetDefault(bool mvc);

    bool isPresent;
    int bitLen;

    unsigned cpb_cnt_minus1;
    int bit_rate_scale;
    int cpb_size_scale;
    std::vector<unsigned> bit_rate_value_minus1;
    std::vector<unsigned> cpb_size_value_minus1;
    std::vector<uint8_t> cbr_flag;

    int initial_cpb_removal_delay_length_minus1;
    int cpb_removal_delay_length_minus1;
    int dpb_output_delay_length_minus1;
    int time_offset_length;
};

class SPSUnit final : public NALUnit
{
   public:
    bool m_ready;
    int sar_width;
    int sar_height;
    uint32_t num_units_in_tick;
    uint32_t time_scale;
    int fixed_frame_rate_flag;
    int num_units_in_tick_bit_pos;
    int pic_struct_present_flag;
    unsigned frame_crop_left_offset;
    unsigned frame_crop_right_offset;
    unsigned frame_crop_top_offset;
    unsigned frame_crop_bottom_offset;
    int full_sps_bit_len;
    int vui_parameters_bit_pos;
    int low_delay_hrd_flag;
    int separate_colour_plane_flag;
    int profile_idc;
    int num_views;

    int level_idc;
    std::vector<int> level_idc_ext;
    unsigned seq_parameter_set_id;
    unsigned chroma_format_idc;
    unsigned log2_max_frame_num;
    unsigned pic_order_cnt_type;
    unsigned log2_max_pic_order_cnt_lsb;
    int delta_pic_order_always_zero_flag;
    int offset_for_non_ref_pic;
    // int offset_for_ref_frame[256];
    unsigned num_ref_frames;
    unsigned pic_width_in_mbs;
    unsigned pic_height_in_map_units;
    int frame_mbs_only_flag;
    int vui_parameters_present_flag;

    int timing_info_present_flag;
    int aspect_ratio_info_present_flag;
    int aspect_ratio_idc;

    int hrdParamsBitPos;
    HRDParams nalHrdParams;
    HRDParams vclHrdParams;
    std::vector<int> mvcHrdParamsBitPos;
    std::vector<HRDParams> mvcNalHrdParams;
    std::vector<HRDParams> mvcVclHrdParams;

    // subSPS (SVC/MVC) extension
    std::vector<unsigned> view_id;

    std::string getStreamDescr() const;
    unsigned getWidth() const { return pic_width_in_mbs * 16 - getCropX(); }
    unsigned getHeight() const { return (2 - frame_mbs_only_flag) * pic_height_in_map_units * 16 - getCropY(); }
    double getFPS() const;
    void setFps(double fps);

    SPSUnit();
    ~SPSUnit() override = default;

    bool isReady() const { return m_ready; }
    int deserialize();
    using NALUnit::deserialize;
    void insertHrdParameters();
    void updateTimingInfo();
    int getMaxBitrate() const;
    int hrd_parameters(HRDParams& params);
    int deserializeVuiParameters();
    int getCropY() const;
    int getCropX() const;
    void scaling_list(int* scalingList, int sizeOfScalingList, bool& useDefaultScalingMatrixFlag);
    static void serializeHRDParameters(BitStreamWriter& writer, const HRDParams& params);

    int deserializeSubSPS();
    int seq_parameter_set_mvc_extension();
    static void seq_parameter_set_svc_extension();
    static void svc_vui_parameters_extension();
    int mvc_vui_parameters_extension();

   private:
    void insertHrdData(int bitPos, int nal_hrd_len, int vcl_hrd_len, bool addVuiHeader, const HRDParams& params);
};

const static char* sliceTypeStr[5] = {"P_TYPE", "B_TYPE", "I_TYPE", "SP_TYPE", "SI_TYPE"};

class SEIUnit final : public NALUnit
{
   public:
    SEIUnit()
        : pic_struct(0),
          cpb_removal_delay(0),
          dpb_output_delay(0),
          initial_cpb_removal_delay(),
          initial_cpb_removal_delay_offset(),
          number_of_offset_sequences(-1),
          metadataPtsOffset(0),
          m_mvcHeaderLen(0),
          m_mvcHeaderStart(nullptr)
    {
    }
    ~SEIUnit() override = default;

    void deserialize(SPSUnit& sps, int orig_hrd_parameters_present_flag);
    using NALUnit::deserialize;

    void serialize_pic_timing_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader) const;
    void serialize_buffering_period_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader) const;
    int removePicTimingSEI(SPSUnit& sps);

    static void updateMetadataPts(uint8_t* metadataPtsPtr, int64_t pts);
    int isMVCSEI();

   public:
    int pic_struct;
    std::unordered_set<int> m_processedMessages;

    int cpb_removal_delay;
    int dpb_output_delay;
    int initial_cpb_removal_delay[32];
    int initial_cpb_removal_delay_offset[32];
    int number_of_offset_sequences;  // used for bluray MVC metadata
    int metadataPtsOffset;
    int m_mvcHeaderLen;
    uint8_t* m_mvcHeaderStart;

    bool hasProcessedMessage(const int msg) const { return m_processedMessages.find(msg) != m_processedMessages.end(); }

   private:
    void sei_payload(SPSUnit& sps, int payloadType, uint8_t* curBuff, int payloadSize,
                     int orig_hrd_parameters_present_flag);
    static void buffering_period(int payloadSize);
    void pic_timing(SPSUnit& sps, bool orig_hrd_parameters_present_flag);
    void pic_timing(SPSUnit& sps, uint8_t* curBuff, int payloadSize, bool orig_hrd_parameters_present_flag);
    static void pan_scan_rect(int payloadSize);
    static void filler_payload(int payloadSize);
    static void user_data_registered_itu_t_t35(int payloadSize);
    static void user_data_unregistered(int payloadSize);
    static void recovery_point(int payloadSize);
    static void dec_ref_pic_marking_repetition(int payloadSize);
    static void spare_pic(int payloadSize);
    static void scene_info(int payloadSize);
    static void sub_seq_info(int payloadSize);
    static void sub_seq_layer_characteristics(int payloadSize);
    static void sub_seq_characteristics(int payloadSize);
    static void full_frame_freeze(int payloadSize);
    static void full_frame_freeze_release(int payloadSize);
    static void full_frame_snapshot(int payloadSize);
    static void progressive_refinement_segment_start(int payloadSize);
    static void progressive_refinement_segment_end(int payloadSize);
    static void motion_constrained_slice_group_set(int payloadSize);
    static void film_grain_characteristics(int payloadSize);
    int mvc_scalable_nesting(SPSUnit& sps, uint8_t* curBuf, int size, int orig_hrd_parameters_present_flag);
    void processBlurayOffsetMetadata();
    static void processBlurayGopStructure();
    static void deblocking_filter_display_preference(int payloadSize);
    static void stereo_video_info(int payloadSize);
    static void reserved_sei_message(int payloadSize);
    static int getNumClockTS(int pic_struct);
};

class SliceUnit : public NALUnit
{
   public:
    enum SliceType
    {
        P_TYPE,
        B_TYPE,
        I_TYPE,
        SP_TYPE,
        SI_TYPE,
        DUMMY_TYPE
    };

    int m_field_pic_flag;
    int non_idr_flag;
    int memory_management_control_operation;

    unsigned first_mb_in_slice;
    unsigned slice_type;
    unsigned orig_slice_type;
    unsigned pic_parameter_set_id;
    int frame_num;
    int bottom_field_flag;
    int pic_order_cnt_lsb;
    int anchor_pic_flag;

    SliceUnit();
    ~SliceUnit() override = default;

    int deserialize(uint8_t* buffer, uint8_t* end, const std::map<uint32_t, SPSUnit*>& spsMap,
                    const std::map<uint32_t, PPSUnit*>& ppsMap);
    using NALUnit::deserialize;

    const SPSUnit* getSPS() const { return sps; }
    const PPSUnit* getPPS() const { return pps; }
    bool isIDR() const;
    bool isIFrame() const;
    int deserializeSliceType(uint8_t* buffer, uint8_t* end);
    int deserializeSliceHeader(const std::map<uint32_t, SPSUnit*>& spsMap, const std::map<uint32_t, PPSUnit*>& ppsMap);
    void nal_unit_header_svc_extension();
    void nal_unit_header_mvc_extension();

   private:
    const PPSUnit* pps;
    const SPSUnit* sps;
};

#endif
