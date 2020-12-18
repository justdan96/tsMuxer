#ifndef __NAL_UNITS_H
#define __NAL_UNITS_H

#include <memory.h>
#include <types/types.h>

#include <map>
#include <set>
#include <unordered_set>
#include <vector>

#include "bitStream.h"

const static int Extended_SAR = 255;
const static double h264_ar_coeff[] = {0.0,         1.0,          12.0 / 11.0, 10.0 / 11.0, 16.0 / 11.0, 40.0 / 33.0,
                                       24.0 / 11.0, 20.0 / 11.0,  32.0 / 11.0, 80.0 / 33.0, 18.0 / 11.0, 15.0 / 11.0,
                                       64.0 / 33.0, 160.0 / 99.0, 4.0 / 3.0,   3.0 / 2.0,   2.0 / 1.0};

enum NALUnitType
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

const static int SEI_MSG_BUFFERING_PERIOD = 0;
const static int SEI_MSG_PIC_TIMING = 1;
const static int SEI_MSG_MVC_SCALABLE_NESTING = 37;

#if 0
const static char* NALUnitDescr[30] =
                   {"nuUnspecified", "nuSliceNonIDR", "nuSliceA", "nuSliceB", "nuSliceC", 
                    "nuSliceIDR","nuSEI","nuSPS","nuPPS","nuAUD", 
					"nuEOSeq","nuEOStream","nuFillerData","nuSPSExt","nuReserved1", 
					"nuReserved2","nuReserved3","nuReserved4","nuReserved5","nuSliceWithoutPartitioning", 
					"nuReserved6","nuReserved7","nuReserved8"," nuReserved9",
					// ------------------- additional RTP nal units -------------
					"STAP-A","STAP-B","MTAP16","MTAP24","FU-A","FU-B"
                   };

#endif

class NALUnit
{
   public:
    const static int SPS_OR_PPS_NOT_READY = 1;
    // const static int NOT_ENOUGH_BUFFER = 2;
    const static int UNSUPPORTED_PARAM = 3;
    const static int NOT_FOUND = 4;

    int nal_unit_type;
    int nal_ref_idc;

    uint8_t* m_nalBuffer;
    int m_nalBufferLen;

    NALUnit(uint8_t nalUnitType) : nal_unit_type(nalUnitType), nal_ref_idc(0), m_nalBuffer(0), m_nalBufferLen(0) {}
    NALUnit() : nal_unit_type(0), nal_ref_idc(0), m_nalBuffer(0), m_nalBufferLen(0) {}
    // NALUnit(const NALUnit& other);
    virtual ~NALUnit() { delete[] m_nalBuffer; }
    static uint8_t* findNextNAL(uint8_t* buffer, uint8_t* end);
    static uint8_t* findNALWithStartCode(const uint8_t* buffer, const uint8_t* end, bool longCodesAllowed);
    static int encodeNAL(uint8_t* srcBuffer, uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize);
    static int decodeNAL(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize);
    int decodeNAL2(uint8_t* srcBuffer, uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize,
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
    void writeSEGolombCode(BitStreamWriter& bitWriter, int32_t value);
    const BitStreamReader& getBitReader() const { return bitReader; }
    void write_rbsp_trailing_bits(BitStreamWriter& writer);
    void write_byte_align_bits(BitStreamWriter& writer);

    int calcNalLenInBits(const uint8_t* nalBuffer, const uint8_t* end);  // for NAL with rbspTrailingBits only
    int deserializeSliceType(uint8_t* buff, uint8_t* bufEnd);

   protected:
    // GetBitContext getBitContext;
    BitStreamReader bitReader;
    inline unsigned extractUEGolombCode();
    inline int extractSEGolombCode();
    void updateBits(int bitOffset, int bitLen, int value);
};

class NALDelimiter : public NALUnit
{
   public:
    const static int PCT_I_FRAMES = 0;
    const static int PCT_IP_FRAMES = 1;
    const static int PCT_IPB_FRAMES = 2;
    const static int PCT_SI_FRAMES = 3;
    const static int PCT_SI_SP_FRAMES = 4;
    const static int PCT_I_SI_FRAMES = 5;
    const static int PCT_I_SI_P_SP_FRAMES = 6;
    const static int PCT_I_SI_P_SP_B_FRAMES = 7;
    int primary_pic_type;
    NALDelimiter() : NALUnit(nuDelimiter) {}
    int deserialize(uint8_t* buffer, uint8_t* end) override;
    int serialize(uint8_t* dstBuffer) override;
};

class PPSUnit : public NALUnit
{
   private:
    bool m_ready;
    // int m_ppsLenInMbit;
    // int entropy_coding_mode_BitPos;
   public:
    unsigned pic_parameter_set_id;
    unsigned seq_parameter_set_id;
    int entropy_coding_mode_flag;
    int pic_order_present_flag;
    /*
        int num_slice_groups;
        int num_ref_idx_l0_active_minus1;
        int num_ref_idx_l1_active_minus1;
        int weighted_pred_flag;
        int weighted_bipred_idc;
        int pic_init_qp_minus26;
        int pic_init_qs_minus26;
        int chroma_qp_index_offset;
        int deblocking_filter_control_present_flag;
        int constrained_intra_pred_flag;
        int redundant_pic_cnt_present_flag;
        int run_length_minus1[256];
        int top_left[256];
        int bottom_right[256];
        int slice_group_id[256];
        int slice_group_change_direction_flag;
        int slice_group_change_rate;
        int num_slice_groups_minus1;
        int slice_group_map_type;
    */

    PPSUnit() : NALUnit(), m_ready(false) {}
    ~PPSUnit() override {}
    bool isReady() { return m_ready; }
    int deserialize();
    // duplicate PPS and change ppsID and cabac parameter for new PPS
    // void duplicatePPS(PPSUnit& oldPPS, int ppsID, bool cabac);
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

class SPSUnit : public NALUnit
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
    int mb_adaptive_frame_field_flag;
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

    int constraint_set0_flag0;
    int constraint_set0_flag1;
    int constraint_set0_flag2;
    int constraint_set0_flag3;
    int level_idc;
    std::vector<int> level_idc_ext;
    unsigned seq_parameter_set_id;
    unsigned chroma_format_idc;
    unsigned log2_max_frame_num;
    unsigned pic_order_cnt_type;
    unsigned log2_max_pic_order_cnt_lsb;
    int delta_pic_order_always_zero_flag;
    int offset_for_non_ref_pic;
    unsigned num_ref_frames_in_pic_order_cnt_cycle;
    // int offset_for_ref_frame[256];
    unsigned num_ref_frames;
    unsigned pic_width_in_mbs;
    unsigned pic_height_in_map_units;
    int frame_mbs_only_flag;
    int frame_cropping_flag;
    int vui_parameters_present_flag;
    // int field_pic_flag;
    unsigned pic_size_in_map_units;
    // int orig_hrd_parameters_present_flag;

    int timing_info_present_flag;
    int aspect_ratio_info_present_flag;
    int aspect_ratio_idc;

    int hrdParamsBitPos;
    HRDParams nalHrdParams;
    HRDParams vclHrdParams;
    std::vector<int> mvcHrdParamsBitPos;
    std::vector<HRDParams> mvcNalHrdParams;
    std::vector<HRDParams> mvcVclHrdParams;

    /*
    int nal_hrd_parameters_bit_pos;
    int nal_hrd_parameters_present_flag;
    int vcl_hrd_parameters_present_flag;
    std::vector<int> vui_mvc_nal_hrd_parameters_present_flag;
    std::vector<int> vui_mvc_vcl_hrd_parameters_present_flag;
    std::vector<int> vui_mvc_nal_hrd_parameters_bit_pos;
    std::vector<int> vui_mvc_nal_hrd_len;
    std::vector<int> vui_mvc_vcl_hrd_len;
    int nal_hrd_len;
    int vcl_hrd_len;
    */

    // bool m_pulldown;

    // subSPS (SVC/MVC) extension
    std::vector<unsigned> view_id;

    std::string getStreamDescr();
    int getWidth() { return pic_width_in_mbs * 16 - getCropX(); }
    int getHeight() { return (2 - frame_mbs_only_flag) * pic_height_in_map_units * 16 - getCropY(); }
    double getFPS() const;
    void setFps(double fps);

    SPSUnit();
    ~SPSUnit() override {}
    bool isReady() { return m_ready; }
    int deserialize();
    void insertHrdParameters();
    void updateTimingInfo();
    int getMaxBitrate();
    int hrd_parameters(HRDParams& params);
    int deserializeVuiParameters();
    int getCropY();
    int getCropX();
    void scaling_list(int* scalingList, int sizeOfScalingList, bool& useDefaultScalingMatrixFlag);
    void serializeHRDParameters(BitStreamWriter& writer, const HRDParams& params);

    int deserializeSubSPS();
    int seq_parameter_set_mvc_extension();
    void seq_parameter_set_svc_extension();
    void svc_vui_parameters_extension();
    int mvc_vui_parameters_extension();

   private:
    void insertHrdData(int bitPos, int nal_hrd_len, int vcl_hrd_len, bool addVuiHeader, const HRDParams& params);
};

const static char* sliceTypeStr[5] = {"P_TYPE", "B_TYPE", "I_TYPE", "SP_TYPE", "SI_TYPE"};

class SEIUnit : public NALUnit
{
   public:
    SEIUnit()
        : NALUnit(),
          pic_struct(0),
          m_cpb_removal_delay_baseaddr(0),
          m_cpb_removal_delay_bitpos(0),
          number_of_offset_sequences(-1),
          metadataPtsOffset(0),
          m_mvcHeaderLen(0),
          m_mvcHeaderStart(0)
    {
    }
    ~SEIUnit() override {}
    void deserialize(SPSUnit& sps, int orig_hrd_parameters_present_flag);

    void serialize_pic_timing_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader);
    void serialize_buffering_period_message(const SPSUnit& sps, BitStreamWriter& writer, bool seiHeader);
    int removePicTimingSEI(SPSUnit& sps);

    static void updateMetadataPts(uint8_t* metadataPtsPtr, int64_t pts);
    int isMVCSEI();

   public:
    int pic_struct;
    uint8_t* m_cpb_removal_delay_baseaddr;
    int m_cpb_removal_delay_bitpos;
    std::unordered_set<int> m_processedMessages;

    int cpb_removal_delay;
    int dpb_output_delay;
    int initial_cpb_removal_delay[32];
    int initial_cpb_removal_delay_offset[32];
    int number_of_offset_sequences;  // used for bluray MVC metadata
    int metadataPtsOffset;
    int m_mvcHeaderLen;
    uint8_t* m_mvcHeaderStart;

    bool hasProcessedMessage(int msg) const { return m_processedMessages.find(msg) != m_processedMessages.end(); }

   private:
    void sei_payload(SPSUnit& sps, int payloadType, uint8_t* curBuf, int payloadSize,
                     int orig_hrd_parameters_present_flag);
    void buffering_period(int payloadSize);
    void pic_timing(SPSUnit& sps, bool orig_hrd_parameters_present_flag);
    void pic_timing(SPSUnit& sps, uint8_t* curBuf, int payloadSize, bool orig_hrd_parameters_present_flag);
    void pan_scan_rect(int payloadSize);
    void filler_payload(int payloadSize);
    void user_data_registered_itu_t_t35(int payloadSize);
    void user_data_unregistered(int payloadSize);
    void recovery_point(int payloadSize);
    void dec_ref_pic_marking_repetition(int payloadSize);
    void spare_pic(int payloadSize);
    void scene_info(int payloadSize);
    void sub_seq_info(int payloadSize);
    void sub_seq_layer_characteristics(int payloadSize);
    void sub_seq_characteristics(int payloadSize);
    void full_frame_freeze(int payloadSize);
    void full_frame_freeze_release(int payloadSize);
    void full_frame_snapshot(int payloadSize);
    void progressive_refinement_segment_start(int payloadSize);
    void progressive_refinement_segment_end(int payloadSize);
    void motion_constrained_slice_group_set(int payloadSize);
    void film_grain_characteristics(int payloadSize);
    int mvc_scalable_nesting(SPSUnit& sps, uint8_t* curBuf, int size, int orig_hrd_parameters_present_flag);
    void processBlurayOffsetMetadata();
    void processBlurayGopStructure();
    void deblocking_filter_display_preference(int payloadSize);
    void stereo_video_info(int payloadSize);
    void reserved_sei_message(int payloadSize);
    int getNumClockTS(int pic_struct) const;
};

class SliceUnit : public NALUnit
{
   private:
    int m_frameNumBitPos;
    int m_frameNumBits;

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
    int svc_extension_flag;
    int non_idr_flag;
    int memory_management_control_operation;

    unsigned first_mb_in_slice;
    unsigned slice_type;
    unsigned orig_slice_type;
    unsigned pic_parameter_set_id;
    int frame_num;
    int bottom_field_flag;
    unsigned idr_pic_id;
    int pic_order_cnt_lsb;
    int delta_pic_order_cnt_bottom;
    int m_picOrderBitPos;
    int m_picOrderNumBits;
    int anchor_pic_flag;

    SliceUnit();
    ~SliceUnit() override { ; }
    int deserialize(uint8_t* buffer, uint8_t* end, const std::map<uint32_t, SPSUnit*>& spsMap,
                    const std::map<uint32_t, PPSUnit*>& ppsMap);

    const SPSUnit* getSPS() const { return sps; }
    const PPSUnit* getPPS() const { return pps; }
    bool isIDR() const;
    bool isIFrame() const;
    int deserializeSliceType(uint8_t* buffer, uint8_t* end);
#if 0

	int slice_qp_delta;
	int disable_deblocking_filter_idc;
	int slice_alpha_c0_offset_div2;
	int slice_beta_offset_div2;
	int delta_pic_order_cnt[2];
	int slice_qs_delta;
	int redundant_pic_cnt;
	int slice_group_change_cycle;
	int num_ref_idx_l0_active_minus1;
	int num_ref_idx_l1_active_minus1;
	int direct_spatial_mv_pred_flag;
	int num_ref_idx_active_override_flag;
	int sp_for_switch_flag;
	int ref_pic_list_reordering_flag_l0;
	int abs_diff_pic_num_minus1;
	int cabac_init_idc;
	int no_output_of_prior_pics_flag;
	int long_term_reference_flag;
	int adaptive_ref_pic_marking_mode_flag;

	int ref_pic_list_reordering_flag_l1;
	int long_term_pic_num;
	int luma_log2_weight_denom;
	int chroma_log2_weight_denom;

	std::vector<int> m_ref_pic_vect;
	std::vector<int> m_ref_pic_vect2;
	std::vector<int> dec_ref_pic_vector;
	std::vector<int> luma_weight_l0;
	std::vector<int> luma_offset_l0;
	std::vector<int> chroma_weight_l0;
	std::vector<int> chroma_offset_l0;
	std::vector<int> luma_weight_l1;
	std::vector<int> luma_offset_l1;
	std::vector<int> chroma_weight_l1;
	std::vector<int> chroma_offset_l1;

	bool m_shortDeserializeMode;

	void copyFrom(const SliceUnit& other) {
		delete m_nalBuffer;
		//memcpy(this, &other, sizeof(SliceUnit));
		m_nalBuffer = 0;
		m_nalBufferLen = 0;

		nal_ref_idc = other.nal_ref_idc;
		nal_unit_type = other.nal_unit_type;

		slice_qp_delta = other.slice_qp_delta;
		disable_deblocking_filter_idc = other.disable_deblocking_filter_idc;
		slice_alpha_c0_offset_div2 = other.slice_alpha_c0_offset_div2;
		slice_beta_offset_div2 = other.slice_beta_offset_div2;
		delta_pic_order_cnt[0] = other.delta_pic_order_cnt[0];
		delta_pic_order_cnt[1] = other.delta_pic_order_cnt[1];
		slice_qs_delta = other.slice_qs_delta;
		redundant_pic_cnt = other.redundant_pic_cnt;
		slice_group_change_cycle = other.slice_group_change_cycle;
		num_ref_idx_l0_active_minus1 = other.num_ref_idx_l0_active_minus1;
		num_ref_idx_l1_active_minus1 = other.num_ref_idx_l1_active_minus1;
		direct_spatial_mv_pred_flag = other.direct_spatial_mv_pred_flag;
		num_ref_idx_active_override_flag = other.num_ref_idx_active_override_flag;
		sp_for_switch_flag = other.sp_for_switch_flag;
		ref_pic_list_reordering_flag_l0 = other.ref_pic_list_reordering_flag_l0;
		abs_diff_pic_num_minus1 = other.abs_diff_pic_num_minus1;
		cabac_init_idc = other.cabac_init_idc;
		no_output_of_prior_pics_flag = other.no_output_of_prior_pics_flag;
		long_term_reference_flag = other.long_term_reference_flag;
		adaptive_ref_pic_marking_mode_flag = other.adaptive_ref_pic_marking_mode_flag;

		ref_pic_list_reordering_flag_l1 = other.ref_pic_list_reordering_flag_l1;
		long_term_pic_num = other.long_term_pic_num;
		luma_log2_weight_denom = other.luma_log2_weight_denom;
		chroma_log2_weight_denom = other.chroma_log2_weight_denom;
		m_ref_pic_vect = other.m_ref_pic_vect;
		m_ref_pic_vect2 = other.m_ref_pic_vect2;
		dec_ref_pic_vector = other.dec_ref_pic_vector;
		luma_weight_l0 = other.luma_weight_l0;
		luma_offset_l0 = other.luma_offset_l0;
		chroma_weight_l0 = other.chroma_weight_l0;
		chroma_offset_l0 = other.chroma_offset_l0;
		luma_weight_l1 = other.luma_weight_l1;
		luma_offset_l1 = other.luma_offset_l1;
		chroma_weight_l1 = other.chroma_weight_l1;
		chroma_offset_l1 = other.chroma_offset_l1;
		m_shortDeserializeMode = other.m_shortDeserializeMode;

		first_mb_in_slice = other.first_mb_in_slice;
		slice_type = other.slice_type;
		orig_slice_type = other.orig_slice_type;
		pic_parameter_set_id = other.pic_parameter_set_id;

		frame_num = other.frame_num;
		bottom_field_flag = other.bottom_field_flag;
		idr_pic_id = other.idr_pic_id;
		pic_order_cnt_lsb = other.pic_order_cnt_lsb;
		delta_pic_order_cnt_bottom = other.delta_pic_order_cnt_bottom;
		m_picOrderBitPos = other.m_picOrderBitPos;
		m_picOrderNumBits = other.m_picOrderNumBits;
		m_field_pic_flag = other.m_field_pic_flag;
		memory_management_control_operation = other.memory_management_control_operation;

	}
	void setFrameNum(int num);
	int deserializeSliceData();
	int serializeSliceHeader(BitStreamWriter& bitWriter, const std::map<uint32_t, SPSUnit*>& spsMap,
                                    const std::map<uint32_t, PPSUnit*>& ppsMap, uint8_t* dstBuffer, int dstBufferLen);
private:
	void write_pred_weight_table(BitStreamWriter& bitWriter);
	void write_dec_ref_pic_marking(BitStreamWriter& bitWriter);
	void write_ref_pic_list_reordering(BitStreamWriter& bitWriter);
	void macroblock_layer();
	void pred_weight_table();
	void dec_ref_pic_marking();
	void ref_pic_list_reordering();
	int NextMbAddress(int n);
#endif
    int deserializeSliceHeader(const std::map<uint32_t, SPSUnit*>& spsMap, const std::map<uint32_t, PPSUnit*>& ppsMap);
    void nal_unit_header_svc_extension();
    void nal_unit_header_mvc_extension();

   private:
    const PPSUnit* pps;
    const SPSUnit* sps;
};

#endif
