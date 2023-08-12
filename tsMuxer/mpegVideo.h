#ifndef MPEG_VIDEO_H_
#define MPEG_VIDEO_H_

/* Start codes. */

#include <types/types.h>

#include "avPacket.h"
#include "bitStream.h"
#include "vod_common.h"

static constexpr double frame_rates[] = {0.0,  23.97602397602397, 24.0, 25.0, 29.97002997002997, 30,
                                         50.0, 59.94005994005994, 60.0};

static constexpr unsigned SEQ_END_CODE = 0x00001b7;
static constexpr unsigned SEQ_START_CODE = 0x00001b3;
static constexpr unsigned GOP_START_CODE = 0x00001b8;
static constexpr unsigned PICTURE_START_CODE = 0x0000100;
static constexpr unsigned SLICE_MIN_START_CODE = 0x0000101;
static constexpr unsigned SLICE_MAX_START_CODE = 0x00001af;
static constexpr unsigned EXT_START_CODE = 0x00001b5;
static constexpr unsigned USER_START_CODE = 0x00001b2;

static constexpr unsigned SEQ_END_SHORT_CODE = 0xb7;
static constexpr unsigned SEQ_START_SHORT_CODE = 0xb3;
static constexpr unsigned GOP_START_SHORT_CODE = 0xb8;
static constexpr unsigned PICTURE_START_SHORT_CODE = 0x00;
static constexpr unsigned SLICE_MIN_START_SHORT_CODE = 0x01;
static constexpr unsigned SLICE_MAX_START_SHORT_CODE = 0xaf;
static constexpr unsigned EXT_START_SHORT_CODE = 0xb5;
static constexpr unsigned USER_START_SHORT_CODE = 0xb2;

static constexpr unsigned PICTURE_CODING_EXT = 0x08;
static constexpr unsigned SEQUENCE_EXT = 0x01;
static constexpr unsigned SEQUENCE_DISPLAY_EXT = 0x02;

static constexpr unsigned MAX_PICTURE_SIZE = 256 * 1024;
static constexpr unsigned MAX_HEADER_SIZE = 1024 * 4;

class MPEGHeader
{
   public:
    static uint8_t* findNextMarker(uint8_t* buffer, uint8_t* end)
    {
        // uint8_t* bufStart = buffer;
        for (buffer += 2; buffer < end;)
        {
            if (*buffer > 1)
                buffer += 3;
            else if (*buffer == 0)
                buffer++;
            else  // *buffer == 1
            {
                if (buffer[-2] == 0 && buffer[-1] == 0)
                    return buffer - 2;
                buffer += 3;
            }
        }
        return end;
    }

   protected:
    MPEGHeader() {}
    virtual ~MPEGHeader() = default;

    static uint8_t* skipProcessedBytes(const BitStreamReader& bitContext)
    {
        // int bytes_readed = ((get_bits_count(&bitContext) | 0x7) + 1) >> 3;
        const int bytes_readed = ((bitContext.getBitsCount() | 0x7) + 1) >> 3;
        return bitContext.getBuffer() + bytes_readed;
    }
};

class MPEGRawDataHeader : public MPEGHeader
{
   public:
    uint8_t* m_data_buffer;
    virtual bool addRawData(uint8_t* buffer, int len, bool headerIncluded, bool isHeader);
    MPEGRawDataHeader(int maxBufferLen);
    ~MPEGRawDataHeader() override;
    virtual uint32_t serialize(uint8_t* buffer);
    uint32_t getDataBufferLen() const { return m_data_buffer_len; }
    virtual void clearRawBuffer() { m_data_buffer_len = 0; }

   protected:
    bool m_headerIncludedToBuff;
    int m_data_buffer_len;
    int m_max_data_len;
    // buffer for general copying into the output stream
};

class MPEGSequenceHeader final : public MPEGRawDataHeader
{
   public:
    int width;
    int height;

    // int frameRate;
    int aspect_ratio_info;
    unsigned frame_rate_index;
    int bit_rate;

    int rc_buffer_size;

    int vbv_buffer_size : 10;
    int constParameterFlag : 1;

    bool load_intra_matrix;
    bool load_non_intra_matrix;

    uint8_t intra_matrix[64];
    uint8_t non_intra_matrix[64];

    // sequence extension
    int profile;
    int level;
    int progressive_sequence;
    int chroma_format;
    int horiz_size_ext;
    int vert_size_ext;
    int bit_rate_ext;
    int low_delay;
    AVRational frame_rate_ext;

    // sequence display extension
    int video_format;

    int color_primaries;
    int transfer_characteristics;
    int matrix_coefficients;

    int pan_scan_width;
    int pan_scan_height;
    MPEGSequenceHeader(int bufferSize);
    ~MPEGSequenceHeader() override = default;
    uint8_t* deserialize(uint8_t* buf, int64_t buf_size);
    uint8_t* deserializeExtension(BitStreamReader& bitReader);
    static uint8_t* deserializeMatrixExtension(const BitStreamReader& bitReader);
    uint8_t* deserializeDisplayExtension(BitStreamReader& bitReader);
    double getFrameRate() const;
    void setFrameRate(uint8_t* buff, double fps);
    static void setAspectRatio(uint8_t* buff, VideoAspectRatio ar);
    std::string getStreamDescr();
};

class MPEGGOPHeader final : public MPEGHeader
{
   public:
    MPEGGOPHeader();
    ~MPEGGOPHeader() override;
    int drop_frame_flag;
    uint8_t time_code_hours;
    uint8_t time_code_minutes;
    // int marker; // 1 bit = 1
    uint8_t time_code_seconds;
    uint8_t time_code_pictures;
    // int close_gop;
    int close_gop;
    int broken_link;
    uint8_t* deserialize(uint8_t* buf, int64_t buf_size);
    uint32_t serialize(uint8_t* buffer) const;
};

enum class PictureCodingType
{
    FORBIDDEN,
    I_FRAME,
    P_FRAME,
    B_FRAME,
    D_FRAME
};

class MPEGPictureHeader final : public MPEGRawDataHeader
{
   public:
    uint16_t ref;
    PictureCodingType pict_type;
    uint16_t vbv_delay;  // stop before reflecting the frame

    // used only for b-frame and p-frames
    int full_pel[2];
    uint8_t f_code;     // 3 bit value
    uint8_t extra_bit;  // always 0 for mpeg2
    uint8_t mpeg_f_code[2][2];

    // picture extension
    uint8_t intra_dc_precision;
    uint8_t picture_structure;

    uint8_t top_field_first;
    uint8_t frame_pred_frame_dct;
    uint8_t concealment_motion_vectors;
    uint8_t q_scale_type;
    uint8_t intra_vlc_format;
    uint8_t alternate_scan;
    uint8_t repeat_first_field;
    uint8_t chroma_420_type;
    uint8_t progressive_frame;

    uint8_t composite_display_flag;
    uint8_t v_axis;
    uint8_t field_sequence;
    uint8_t sub_carrier;
    uint8_t burst_amplitude;
    uint8_t sub_carrier_phase;

    // picture display extension
    uint16_t horizontal_offset;
    uint16_t vertical_offset;

    // internal buffer param
    int m_headerSize;
    uint32_t m_picture_data_len;
    int repeat_first_field_bitpos;
    int top_field_first_bitpos;

    void clearRawBuffer() override { m_picture_data_len = m_headerSize = m_data_buffer_len = 0; }

    BitStreamReader bitReader;

    // methods
    MPEGPictureHeader(int bufferSize);
    ~MPEGPictureHeader() override = default;

    uint8_t* deserialize(uint8_t* buf, int64_t buf_size);
    uint8_t* deserializeCodingExtension(BitStreamReader& bitReader);

    uint32_t serialize(uint8_t* buffer) override;
    uint32_t getPictureSize() const;
    void setTempRef(uint32_t number);
    void setVbvDelay(uint16_t val);

    bool addRawData(uint8_t* buffer, int len, bool headerIncluded, bool isHeader) override;
    void buildHeader();
    void buildCodingExtension();
};

class MPEGSliceHeader final : public MPEGHeader
{
   public:
    static void deserialize(uint8_t* buf, int buf_size);

   private:
    static void macroblocks(BitStreamReader& reader);
    static int readMacroblockAddressIncrement(BitStreamReader& reader);
};

class MPEGUserDataHeader final : public MPEGHeader
{
};

#endif
