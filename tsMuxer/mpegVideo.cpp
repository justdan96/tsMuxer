
#include "mpegVideo.h"

#include <fs/systemlog.h>

#include <cmath>
#include <cstring>

#include "vodCoreException.h"
#include "vod_common.h"

static constexpr double FRAME_RATE_EPS = 3e-5;
static constexpr int MB_ESCAPE_CODE = -1;

// --------------------- MPEGRawDataHeader ---------------------

MPEGRawDataHeader::MPEGRawDataHeader(const int maxBufferLen)
    : m_headerIncludedToBuff(false), m_data_buffer_len(0), m_max_data_len(maxBufferLen)
{
    if (maxBufferLen > 0)
        m_data_buffer = new uint8_t[maxBufferLen];
    else
        m_data_buffer = nullptr;
}

MPEGRawDataHeader::~MPEGRawDataHeader() { delete[] m_data_buffer; }

uint32_t MPEGRawDataHeader::serialize(uint8_t* buffer)
{
    if (!m_headerIncludedToBuff)
    {
        // buffer does not include pictureHeader. we're serializing it.
        // todo not implemented. this branch is currently never entered.
    }
    // the buffer includes everything - the pictureHeader as well
    memcpy(buffer, m_data_buffer, m_data_buffer_len);

    return m_data_buffer_len;
}

bool MPEGRawDataHeader::addRawData(uint8_t* buffer, const int len, const bool headerIncluded, bool isHeader)
{
    if (m_data_buffer_len + len > m_max_data_len)
    {
        return false;
    }
    m_headerIncludedToBuff = headerIncluded;
    memcpy(m_data_buffer + m_data_buffer_len, buffer, len);
    m_data_buffer_len += len;
    return true;
}

// --------------------- MPEGSequenceHeader --------------------

MPEGSequenceHeader::MPEGSequenceHeader(const int bufferSize)
    : MPEGRawDataHeader(bufferSize),
      width(0),
      height(0),
      aspect_ratio_info(0),
      frame_rate_index(0),
      bit_rate(0),
      rc_buffer_size(0),
      vbv_buffer_size(0),
      constParameterFlag(0),
      load_intra_matrix(false),
      load_non_intra_matrix(false),
      intra_matrix(),
      non_intra_matrix(),
      chroma_format(0),
      horiz_size_ext(0),
      vert_size_ext(0),
      bit_rate_ext(0),
      low_delay(false),
      video_format(0),
      color_primaries(0),
      transfer_characteristics(0),
      matrix_coefficients(0),
      pan_scan_width(0),
      pan_scan_height(0)
{
    progressive_sequence = false;
    profile = -1;
    level = -1;
}

uint8_t* MPEGSequenceHeader::deserialize(uint8_t* buf, const int64_t buf_size)
{
    BitStreamReader bitReader{};
    bitReader.setBuffer(buf, buf + buf_size);

    width = bitReader.getBits<int16_t>(12);
    if (width == 4096)
    {
        width = bitReader.getBits<int16_t>(12);
    }
    height = bitReader.getBits<int16_t>(12);
    if (width <= 0 || height <= 0 || (width % 2) != 0 || (height % 2) != 0)
        return nullptr;
    aspect_ratio_info = bitReader.getBits<uint8_t>(4);
    if (aspect_ratio_info == 0)
        return nullptr;
    frame_rate_index = bitReader.getBits<uint8_t>(4);
    if (frame_rate_index == 0 || frame_rate_index > 13)
        return nullptr;

    bit_rate = bitReader.getBits(18) * 400;
    if (bitReader.getBit() == 0) /* marker */
        return nullptr;

    rc_buffer_size = bitReader.getBits(10) * 1024 * 16;
    bitReader.skipBit();  // constrained_parameter_flag

    /* get matrix */
    load_intra_matrix = bitReader.getBit() != 0;
    if (load_intra_matrix)
    {
        for (auto& i : intra_matrix)
        {
            i = bitReader.getBits<uint8_t>(8);
            if (i == 0)
            {
                LTRACE(LT_ERROR, 1, "mpeg sequence header: intra matrix damaged");
                return nullptr;
            }
        }
    }

    load_non_intra_matrix = bitReader.getBit() != 0;
    if (load_non_intra_matrix)
    {
        for (auto& i : non_intra_matrix)
        {
            i = bitReader.getBits<uint8_t>(8);
            if (i == 0)
            {
                LTRACE(LT_ERROR, 1, "mpeg sequence header: non-intra matrix damaged");
                return nullptr;
            }
        }
    }

    return skipProcessedBytes(bitReader);
}

uint8_t* MPEGSequenceHeader::deserializeExtension(BitStreamReader& bitReader)
{
    bitReader.skipBit(); /* profile and level esc*/
    profile = bitReader.getBits<int8_t>(3);
    level = bitReader.getBits<int8_t>(4);

    progressive_sequence = bitReader.getBit(); /* progressive_sequence */
    chroma_format = bitReader.getBits<uint8_t>(2); /* chroma_format 1=420, 2=422, 3=444 */

    horiz_size_ext = bitReader.getBits<uint8_t>(2);
    vert_size_ext = bitReader.getBits<uint8_t>(2);
    width |= horiz_size_ext << 12;
    height |= vert_size_ext << 12;

    bit_rate_ext = bitReader.getBits<int16_t>(12); /* XXX: handle it */
    bit_rate += (bit_rate_ext << 18) * 400;

    bitReader.skipBit();  // marker

    rc_buffer_size += bitReader.getBits(8) * 1024 * 16 << 10;

    low_delay = bitReader.getBit();  // disable b-frame if true
    frame_rate_ext.num = bitReader.getBits<uint8_t>(2) + 1;
    frame_rate_ext.den = bitReader.getBits<uint8_t>(5) + 1;

    // return bitContext.buffer_ptr;
    return skipProcessedBytes(bitReader);
}

uint8_t* MPEGSequenceHeader::deserializeMatrixExtension(const BitStreamReader& bitReader)
{
    return bitReader.getBuffer();
}

uint8_t* MPEGSequenceHeader::deserializeDisplayExtension(BitStreamReader& bitReader)
{
    // MpegEncContext *s= &s1->mpeg_enc_ctx;
    video_format = bitReader.getBits<uint8_t>(3); /* video format */

    if (bitReader.getBit())  // color_description
    {
        color_primaries = bitReader.getBits<uint8_t>(8);  /* color primaries */
        transfer_characteristics = bitReader.getBits<uint8_t>(8); /* transfer_characteristics */
        matrix_coefficients = bitReader.getBits<uint8_t>(8);      /* matrix_coefficients */
    }

    pan_scan_width = bitReader.getBits(14) << 4;
    bitReader.skipBit();  // marker
    pan_scan_height = bitReader.getBits(14) << 4;
    bitReader.skipBit();  // marker

    // return bitContext.buffer_ptr;
    return skipProcessedBytes(bitReader);
}

std::string MPEGSequenceHeader::getStreamDescr() const
{
    std::ostringstream rez;
    rez << "Profile: ";
    switch (profile)
    {
    case 0:
        rez << "Reserved@";
        break;
    case 1:
        rez << "High@";
        break;
    case 2:
        rez << "Spatially Scalable@";
        break;
    case 3:
        rez << "SNR Scalable@";
        break;
    case 4:
        rez << "Main@";
        break;
    case 5:
        rez << "Simple@";
        break;
    case -1:
        rez << "Unspecified. ";
        break;
    default:
        rez << "Unknown@";
        break;
    }
    if (level >= 0)
        rez << level << ". ";
    rez << "Resolution: " << width << ':' << height;
    rez << (progressive_sequence ? 'p' : 'i') << ". ";
    rez << "Frame rate: ";
    const double fps = getFrameRate();
    rez << fps;
    return rez.str();
}

double MPEGSequenceHeader::getFrameRate() const
{
    if (frame_rate_index > 0 && frame_rate_index < sizeof(frame_rates) / sizeof(double))
        return frame_rates[frame_rate_index];
    return 0;
}

void MPEGSequenceHeader::setFrameRate(uint8_t* buff, const double fps)
{
    for (uint8_t i = 1; i < 9; i++)
        if (std::abs(frame_rates[i] - fps) < FRAME_RATE_EPS)
        {
            frame_rate_index = i;
            buff[3] = (buff[3] & 0xf0) + i;
            return;
        }
    THROW(ERR_MPEG2_ERR_FPS,
          "Can't set fps to value " << fps << ". Only standard fps values allowed for mpeg2 streams.")
}

void MPEGSequenceHeader::setAspectRatio(uint8_t* buff, VideoAspectRatio ar)
{
    buff[3] = static_cast<uint8_t>((buff[3] & 0x0f) | static_cast<int>(ar) << 4);
}

// --------------- gop header ------------------

MPEGGOPHeader::MPEGGOPHeader()
    : drop_frame_flag(0),
      time_code_hours(0),
      time_code_minutes(0),
      time_code_seconds(0),
      time_code_pictures(0),
      close_gop(0),
      broken_link(0)
{
}

MPEGGOPHeader::~MPEGGOPHeader() = default;

uint8_t* MPEGGOPHeader::deserialize(uint8_t* buf, const int64_t buf_size)
{
    BitStreamReader bitReader{};
    bitReader.setBuffer(buf, buf + buf_size);

    drop_frame_flag = bitReader.getBit();

    time_code_hours = bitReader.getBits<uint8_t>(5);
    time_code_minutes = bitReader.getBits<uint8_t>(6);
    bitReader.skipBit();  // marker bit
    time_code_seconds = bitReader.getBits<uint8_t>(6);
    time_code_pictures = bitReader.getBits<uint8_t>(6);

    close_gop = bitReader.getBit();
    /*broken_link indicate that after editing the
      reference frames of the first B-Frames after GOP I-Frame
      are missing (open gop)*/
    broken_link = bitReader.getBit();

    return skipProcessedBytes(bitReader);
}

uint32_t MPEGGOPHeader::serialize(uint8_t* buffer) const
{
    BitStreamWriter bitWriter{};
    bitWriter.setBuffer(buffer, buffer + 8);
    bitWriter.putBits(1, drop_frame_flag);

    bitWriter.putBits(5, time_code_hours);
    bitWriter.putBits(6, time_code_minutes);
    bitWriter.putBits(1, 1);  // marker bit
    bitWriter.putBits(6, time_code_seconds);
    bitWriter.putBits(6, time_code_pictures);

    bitWriter.putBits(1, close_gop);
    /*broken_link indicate that after editing the
  reference frames of the first B-Frames after GOP I-Frame
  are missing (open gop)*/
    bitWriter.putBits(1, broken_link);
    bitWriter.flushBits();
    const uint32_t bitCnt = bitWriter.getBitsCount();
    return (bitCnt >> 3) + (bitCnt & 7 ? 1 : 0);
}

// ----------------- picture headers -----------

MPEGPictureHeader::MPEGPictureHeader(const int bufferSize)
    : MPEGRawDataHeader(bufferSize),
      ref(0),
      pict_type(PictureCodingType::FORBIDDEN),
      vbv_delay(0),
      full_pel(),
      f_code(0),
      extra_bit(0),
      mpeg_f_code(),
      intra_dc_precision(0),
      picture_structure(0),
      top_field_first(0),
      frame_pred_frame_dct(0),
      concealment_motion_vectors(0),
      q_scale_type(0),
      intra_vlc_format(0),
      alternate_scan(0),
      chroma_420_type(0),
      progressive_frame(0),
      composite_display_flag(0),
      v_axis(0),
      field_sequence(0),
      sub_carrier(0),
      burst_amplitude(0),
      sub_carrier_phase(0),
      horizontal_offset(0),
      vertical_offset(0),
      m_headerSize(0),
      m_picture_data_len(0)
{
    repeat_first_field = 0;
    repeat_first_field_bitpos = 0;
    top_field_first_bitpos = 0;
}

void MPEGPictureHeader::buildHeader()
{
    BitStreamWriter bitWriter{};
    bitWriter.setBuffer(m_data_buffer, m_data_buffer + m_max_data_len);

    bitWriter.putBits(16, 0);
    bitWriter.putBits(16, PICTURE_START_CODE);

    bitWriter.putBits(10, ref);
    bitWriter.putBits(3, static_cast<int>(pict_type));
    bitWriter.putBits(16, vbv_delay);

    if (pict_type == PictureCodingType::P_FRAME || pict_type == PictureCodingType::B_FRAME)
    {
        bitWriter.putBits(1, full_pel[0]);
        bitWriter.putBits(3, mpeg_f_code[0][0]);
    }
    if (pict_type == PictureCodingType::B_FRAME)
    {
        bitWriter.putBits(1, full_pel[1]);
        bitWriter.putBits(3, mpeg_f_code[1][0]);
    }
    bitWriter.flushBits();
    const int bitCnt = bitWriter.getBitsCount();
    m_data_buffer_len = (bitCnt >> 3) + (bitCnt & 7 ? 1 : 0);
}

void MPEGPictureHeader::buildCodingExtension()
{
    BitStreamWriter bitWriter{};
    uint8_t* bufPtr = m_data_buffer + m_data_buffer_len;
    bitWriter.setBuffer(bufPtr, bufPtr + m_max_data_len - m_data_buffer_len);

    bitWriter.putBits(16, 0);
    bitWriter.putBits(16, EXT_START_CODE);

    bitWriter.putBits(4, PICTURE_CODING_EXT);

    bitWriter.putBits(4, mpeg_f_code[0][0]);
    bitWriter.putBits(4, mpeg_f_code[0][1]);
    bitWriter.putBits(4, mpeg_f_code[1][0]);
    bitWriter.putBits(4, mpeg_f_code[1][1]);

    bitWriter.putBits(2, intra_dc_precision);

    bitWriter.putBits(2, picture_structure);
    bitWriter.putBits(1, top_field_first);
    bitWriter.putBits(1, frame_pred_frame_dct);
    bitWriter.putBits(1, concealment_motion_vectors);
    bitWriter.putBits(1, q_scale_type);
    bitWriter.putBits(1, intra_vlc_format);

    bitWriter.putBits(1, alternate_scan);
    bitWriter.putBits(1, repeat_first_field);
    bitWriter.putBits(1, chroma_420_type);
    bitWriter.putBits(1, progressive_frame);
    bitWriter.putBits(1, composite_display_flag);
    if (composite_display_flag)
    {
        bitWriter.putBits(1, v_axis);
        bitWriter.putBits(3, field_sequence);
        bitWriter.putBits(1, sub_carrier);
        bitWriter.putBits(7, burst_amplitude);
        bitWriter.putBits(8, sub_carrier_phase);
    }
    bitWriter.flushBits();
    const int bitCnt = bitWriter.getBitsCount();
    m_data_buffer_len += (bitCnt >> 3) + (bitCnt & 7 ? 1 : 0);
}

uint8_t* MPEGPictureHeader::deserialize(uint8_t* buf, const int64_t buf_size)
{
    bitReader.setBuffer(buf, buf + buf_size);

    ref = bitReader.getBits<int16_t>(10); /* temporal ref */
    pict_type = static_cast<PictureCodingType>(bitReader.getBits(3));
    if (pict_type == PictureCodingType::FORBIDDEN || pict_type == PictureCodingType::D_FRAME)
        return nullptr;

    vbv_delay = bitReader.getBits<int16_t>(16);

    if (pict_type == PictureCodingType::P_FRAME || pict_type == PictureCodingType::B_FRAME)
    {
        full_pel[0] = bitReader.getBit();
        f_code = bitReader.getBits<uint8_t>(3);
        mpeg_f_code[0][0] = f_code;
        mpeg_f_code[0][1] = f_code;
    }
    if (pict_type == PictureCodingType::B_FRAME)
    {
        full_pel[1] = bitReader.getBit();
        f_code = bitReader.getBits<uint8_t>(3);
        mpeg_f_code[1][0] = f_code;
        mpeg_f_code[1][1] = f_code;
    }
    uint8_t* new_buff = skipProcessedBytes(bitReader);
    m_headerSize = static_cast<int>(new_buff - buf);
    return new_buff;
}

// static void mpeg_decode_picture_coding_extension(MpegEncContext *s)
uint8_t* MPEGPictureHeader::deserializeCodingExtension(BitStreamReader& reader)
{
    full_pel[0] = full_pel[1] = 0;

    mpeg_f_code[0][0] = reader.getBits<uint8_t>(4);
    mpeg_f_code[0][1] = reader.getBits<uint8_t>(4);
    mpeg_f_code[1][0] = reader.getBits<uint8_t>(4);
    mpeg_f_code[1][1] = reader.getBits<uint8_t>(4);

    intra_dc_precision = reader.getBits<uint8_t>(2);
    picture_structure = reader.getBits<uint8_t>(2);
    top_field_first_bitpos = reader.getBitsCount();
    top_field_first = reader.getBit();
    frame_pred_frame_dct = reader.getBit();
    concealment_motion_vectors = reader.getBit();
    q_scale_type = reader.getBit();
    intra_vlc_format = reader.getBit();
    alternate_scan = reader.getBit();
    repeat_first_field_bitpos = reader.getBitsCount();
    repeat_first_field = reader.getBit();
    chroma_420_type = reader.getBit();
    progressive_frame = reader.getBit();
    composite_display_flag = reader.getBit();

    if (composite_display_flag)
    {
        v_axis = reader.getBit();
        field_sequence = reader.getBits<uint8_t>(3);
        sub_carrier = reader.getBit();
        burst_amplitude = reader.getBits<uint8_t>(7);
        sub_carrier_phase = reader.getBits<uint8_t>(8);
    }

    return skipProcessedBytes(reader);
}

void MPEGPictureHeader::setTempRef(const uint16_t number)
{
    if (m_headerIncludedToBuff)
    {
        m_data_buffer[4] = static_cast<uint8_t>(number >> 2);
        m_data_buffer[5] = (m_data_buffer[5] & 0x3f) + ((number << 6) & 0xc0);
    }
    else
        ref = number;
}

void MPEGPictureHeader::setVbvDelay(const uint16_t val)
{
    if (m_headerIncludedToBuff)
    {
        uint8_t* vbv_delay_ptr = &m_data_buffer[5];  // skip sequence header and 1-st byte of ref
        vbv_delay_ptr[0] &= 0xF8;
        vbv_delay_ptr[0] |= vbv_delay >> 13;
        vbv_delay_ptr[1] = static_cast<uint8_t>(vbv_delay >> 5);
        vbv_delay_ptr[2] &= 0x07;
        vbv_delay_ptr[2] |= vbv_delay << 3;
    }
    else
        vbv_delay = val;
}

uint32_t MPEGPictureHeader::getPictureSize() const { return m_headerSize + m_data_buffer_len; }

bool MPEGPictureHeader::addRawData(uint8_t* buffer, const int len, const bool headerIncluded, const bool isHeader)

{
    const bool rez = MPEGRawDataHeader::addRawData(buffer, len, headerIncluded, isHeader);
    if (!isHeader)
        m_picture_data_len += len;
    return rez;
}

uint32_t MPEGPictureHeader::serialize(uint8_t* buffer)
{
    const uint32_t rez = MPEGRawDataHeader::serialize(buffer);
    m_picture_data_len = 0;
    return rez;
}

void MPEGSliceHeader::deserialize(uint8_t* buf, const int buf_size)
{
    BitStreamReader reader{};
    reader.setBuffer(buf, buf + buf_size);
    reader.skipBits(5);  // quantiser_scale_code
    if (reader.getBit())
    {
        reader.skipBits(9);  // intra_slice_flag, intra_slice, reserved_bits
    }
    reader.skipBit();  // extra_bit_slice
    macroblocks(reader);
}

void MPEGSliceHeader::macroblocks(BitStreamReader& reader)
{
    int increment = readMacroblockAddressIncrement(reader);
    while (increment == MB_ESCAPE_CODE)
    {
        increment = readMacroblockAddressIncrement(reader);
    }
}

int MPEGSliceHeader::readMacroblockAddressIncrement(BitStreamReader& reader)
{
    unsigned cnt = 0;
    for (; reader.getBit() == 0; cnt++)
        ;
    if (cnt > INT_BIT)
        THROW_BITSTREAM_ERR;
    switch (cnt)
    {
    case 0:
        return 1;
    case 1:
        return 3 - reader.getBit();
    case 2:
        return 5 - reader.getBit();
    case 3:
        return 7 - reader.getBit();
    case 4:
        return reader.getBit() ? 9 - reader.getBits<uint8_t>(1) : 13 - reader.getBits<int>(2);
    case 5:
        if (reader.getBit())
            return 15 - reader.getBit();

        if (reader.getBit())
        {  // last bit of first byte
            return 19 - reader.getBits<uint8_t>(2);
        }
        if (reader.getBit())
            return 21 - reader.getBit();
        return 25 - reader.getBits<uint8_t>(2);
    case 6:
        reader.skipBit();
        return 33 - reader.getBits<uint8_t>(3);
    case 7:
        return MB_ESCAPE_CODE;
    // avoid compiler warning: control reaches end of non-void function
    default:
        assert(0);
        return 0;
    }
}
