#ifndef __MPEG_VIDEO_H
#define __MPEG_VIDEO_H

/* Start codes. */

#include <types/types.h>

#include <iostream>

#include "avPacket.h"
#include "bitStream.h"
#include "vod_common.h"

static const double frame_rates[] = {0.0,  23.97602397602397, 24.0, 25.0, 29.97002997002997, 30,
                                     50.0, 59.94005994005994, 60.0};

const static unsigned SEQ_END_CODE = 0x00001b7;
const static unsigned SEQ_START_CODE = 0x00001b3;
const static unsigned GOP_START_CODE = 0x00001b8;
const static unsigned PICTURE_START_CODE = 0x0000100;
const static unsigned SLICE_MIN_START_CODE = 0x0000101;
const static unsigned SLICE_MAX_START_CODE = 0x00001af;
const static unsigned EXT_START_CODE = 0x00001b5;
const static unsigned USER_START_CODE = 0x00001b2;

const static unsigned SEQ_END_SHORT_CODE = 0xb7;
const static unsigned SEQ_START_SHORT_CODE = 0xb3;
const static unsigned GOP_START_SHORT_CODE = 0xb8;
const static unsigned PICTURE_START_SHORT_CODE = 0x00;
const static unsigned SLICE_MIN_START_SHORT_CODE = 0x01;
const static unsigned SLICE_MAX_START_SHORT_CODE = 0xaf;
const static unsigned EXT_START_SHORT_CODE = 0xb5;
const static unsigned USER_START_SHORT_CODE = 0xb2;

const static unsigned PICTURE_CODING_EXT = 0x08;
const static unsigned SEQUENCE_EXT = 0x01;
const static unsigned SEQUENCE_DISPLAY_EXT = 0x02;

static const unsigned MAX_PICTURE_SIZE = 256 * 1024;
static const unsigned MAX_HEADER_SIZE = 1024 * 4;

class MPEGHeader
{
   public:
    inline static uint8_t* findNextMarker(uint8_t* buffer, uint8_t* end)
    {
        // uint8_t* bufStart = buffer;
        for (buffer += 2; buffer < end;)
        {
            if (*buffer > 1)
                buffer += 3;
            else if (*buffer == 0)
                buffer++;
            else if (buffer[-2] == 0 && buffer[-1] == 0)
            {
                return buffer - 2;
            }
            else
                buffer += 3;
        }
        return end;
    }

   protected:
    MPEGHeader(){};
    virtual ~MPEGHeader(){};
    inline static uint8_t* skipProcessedBytes(BitStreamReader& bitContext)
    {
        // int bytes_readed = ((get_bits_count(&bitContext) | 0x7) + 1) >> 3;
        int bytes_readed = ((bitContext.getBitsCount() | 0x7) + 1) >> 3;
        return (uint8_t*)bitContext.getBuffer() + bytes_readed;
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
    uint32_t getDataBufferLen() { return m_data_buffer_len; }
    void clearRawBuffer() { m_data_buffer_len = 0; }

   protected:
    bool m_headerIncludedToBuff;
    int m_data_buffer_len;
    int m_max_data_len;
    // buffer for general copying into the output stream
};

class MPEGSequenceHeader : public MPEGRawDataHeader
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
    ~MPEGSequenceHeader() override{};
    uint8_t* deserialize(uint8_t* buf, int64_t buf_size);
    uint8_t* deserializeExtension(BitStreamReader& bitContext);
    uint8_t* deserializeMatrixExtension(BitStreamReader& bitContext);
    uint8_t* deserializeDisplayExtension(BitStreamReader& bitContext);
    double getFrameRate();
    void setFrameRate(uint8_t* buff, double fps);
    void setAspectRatio(uint8_t* buff, VideoAspectRatio ar);
    std::string getStreamDescr();
};

class MPEGGOPHeader : public MPEGHeader
{
   public:
    MPEGGOPHeader();
    ~MPEGGOPHeader();
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
    uint32_t serialize(uint8_t* buffer);
};

enum class PictureCodingType
{
    FORBIDDEN,
    I_FRAME,
    P_FRAME,
    B_FRAME,
    D_FRAME
};

class MPEGPictureHeader : public MPEGRawDataHeader
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

    void clearRawBuffer() { m_picture_data_len = m_headerSize = m_data_buffer_len = 0; }

    BitStreamReader bitReader;

    // methods
    MPEGPictureHeader(int bufferSize);
    ~MPEGPictureHeader() override{};

    uint8_t* deserialize(uint8_t* buf, int64_t buf_size);
    uint8_t* deserializeCodingExtension(BitStreamReader& bitContext);

    uint32_t serialize(uint8_t* buffer) override;
    uint32_t getPictureSize();
    void setTempRef(uint32_t number);
    void setVbvDelay(uint16_t val);

    bool addRawData(uint8_t* buffer, int len, bool headerIncluded, bool isHeader) override;
    void buildHeader();
    void buildCodingExtension();
};

class MPEGSliceHeader : public MPEGHeader
{
   public:
    void deserialize(uint8_t* buf, int buf_size);

   private:
    void macroblocks(BitStreamReader& reader);
    int readMacroblockAddressIncrement(BitStreamReader& reader);
};

class MPEGUserDataHeader : public MPEGHeader
{
};

#endif
