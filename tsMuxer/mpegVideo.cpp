
#ifndef _WIN32
#endif
#include "mpegVideo.h"

#include <fs/systemlog.h>
#include <memory.h>
#include <stdlib.h>

#include "vodCoreException.h"
#include "vod_common.h"

// static double MPEGSequenceHeader::frame_rates[] = {
// 0.0, 23.97602397602397, 24.0, 25.0, 29.97002997002997, 30, 50.0, 59.94005994005994, 60.0
//};

static const double FRAME_RATE_EPS = 3e-5;
static const int MB_ESCAPE_CODE = -1;

// --------------------- MPEGRawDataHeader ---------------------

MPEGRawDataHeader::MPEGRawDataHeader(int maxBufferLen)
    : MPEGHeader(), m_headerIncludedToBuff(0), m_data_buffer_len(0), m_max_data_len(maxBufferLen)
{
    if (maxBufferLen > 0)
        m_data_buffer = new uint8_t[maxBufferLen];
    else
        m_data_buffer = 0;
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

    uint32_t rez = m_data_buffer_len;
    return rez;
}

bool MPEGRawDataHeader::addRawData(uint8_t* buffer, int len, bool headerIncluded, bool isHeader)
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

MPEGSequenceHeader::MPEGSequenceHeader(int bufferSize) : MPEGRawDataHeader(bufferSize)
{
    progressive_sequence = false;
    profile = -1;
    level = -1;
}

uint8_t* MPEGSequenceHeader::deserialize(uint8_t* buf, int buf_size)
{
    BitStreamReader bitReader;
    bitReader.setBuffer(buf, buf + buf_size);

    width = bitReader.getBits(12);
    if (width == 4096)
    {
        width = bitReader.getBits(12);
    }
    height = bitReader.getBits(12);
    if (width <= 0 || height <= 0 || (width % 2) != 0 || (height % 2) != 0)
        return 0;
    aspect_ratio_info = bitReader.getBits(4);
    if (aspect_ratio_info == 0)
        return 0;
    frame_rate_index = bitReader.getBits(4);
    if (frame_rate_index == 0 || frame_rate_index > 13)
        return 0;

    bit_rate = bitReader.getBits(18) * 400;
    if (bitReader.getBit() == 0) /* marker */
        return 0;

    rc_buffer_size = bitReader.getBits(10) * 1024 * 16;
    bitReader.skipBits(1);  // constrained_parameter_flag

    /* get matrix */
    load_intra_matrix = bitReader.getBit() != 0;
    if (load_intra_matrix)
    {
        for (int i = 0; i < 64; i++)
        {
            intra_matrix[i] = bitReader.getBits(8);
            if (intra_matrix[i] == 0)
            {
                LTRACE(LT_ERROR, 1, "mpeg sequence header: intra matrix damaged");
                return 0;
            }
        }
    }

    return skipProcessedBytes(bitReader);
}

uint8_t* MPEGSequenceHeader::deserializeExtension(BitStreamReader& bitReader)
{
    // MpegEncContext *s= &s1->mpeg_enc_ctx;
    // int horiz_size_ext, vert_size_ext;
    // int bit_rate_ext;

    bitReader.skipBits(1); /* profil and level esc*/
    profile = bitReader.getBits(3);
    level = bitReader.getBits(4);

    progressive_sequence = bitReader.getBit(); /* progressive_sequence */
    chroma_format = bitReader.getBits(2);      /* chroma_format 1=420, 2=422, 3=444 */

    horiz_size_ext = bitReader.getBits(2);
    vert_size_ext = bitReader.getBits(2);
    width |= (horiz_size_ext << 12);
    height |= (vert_size_ext << 12);

    bit_rate_ext = bitReader.getBits(12); /* XXX: handle it */
    bit_rate += (bit_rate_ext << 18) * 400;

    // bitReader.skipBit(); /* marker */
    bitReader.skipBit();

    rc_buffer_size += bitReader.getBits(8) * 1024 * 16 << 10;

    // low_delay = bitReader.getBit(); // disable b-frame if true
    // frame_rate_ext.num = bitReader.getBits(2)+1;
    // frame_rate_ext.den = bitReader.getBits(5)+1;

    /*
s->codec_id= s->avctx->codec_id= CODEC_ID_MPEG2VIDEO;
s->avctx->sub_id = 2; // indicates mpeg2 found

if(s->avctx->debug & FF_DEBUG_PICT_INFO)
    av_log(s->avctx, AV_LOG_DEBUG, "profile: %d, level: %d vbv buffer: %d, bitrate:%d\n",
           s->avctx->profile, s->avctx->level, s->avctx->rc_buffer_size, s->bit_rate);
    */
    // return bitContext.buffer_ptr;
    return skipProcessedBytes(bitReader);
}

uint8_t* MPEGSequenceHeader::deserializeMatrixExtension(BitStreamReader& bitReader)
{
    return (uint8_t*)bitReader.getBuffer();

    /*
if (get_bits1(&s->gb)) {
    for(i=0;i<64;i++) {
        v = get_bits(&s->gb, 8);
        j= s->dsp.idct_permutation[ ff_zigzag_direct[i] ];
        s->intra_matrix[j] = v;
        s->chroma_intra_matrix[j] = v;
    }
}
if (get_bits1(&s->gb)) {
    for(i=0;i<64;i++) {
        v = get_bits(&s->gb, 8);
        j= s->dsp.idct_permutation[ ff_zigzag_direct[i] ];
        s->inter_matrix[j] = v;
        s->chroma_inter_matrix[j] = v;
    }
}
if (get_bits1(&s->gb)) {
    for(i=0;i<64;i++) {
        v = get_bits(&s->gb, 8);
        j= s->dsp.idct_permutation[ ff_zigzag_direct[i] ];
        s->chroma_intra_matrix[j] = v;
    }
}
if (get_bits1(&s->gb)) {
    for(i=0;i<64;i++) {
        v = get_bits(&s->gb, 8);
        j= s->dsp.idct_permutation[ ff_zigzag_direct[i] ];
        s->chroma_inter_matrix[j] = v;
    }
}
    */
}

uint8_t* MPEGSequenceHeader::deserializeDisplayExtension(BitStreamReader& bitReader)
{
    // MpegEncContext *s= &s1->mpeg_enc_ctx;
    int color_description, w, h;

    video_format = bitReader.getBits(3); /* video format */

    color_description = bitReader.getBit();
    if (color_description)
    {
        color_primaries = bitReader.getBits(8);          /* color primaries */
        transfer_characteristics = bitReader.getBits(8); /* transfer_characteristics */
        matrix_coefficients = bitReader.getBits(8);      /* matrix_coefficients */
    }

    w = bitReader.getBits(14);
    bitReader.skipBits(1);  // marker
    h = bitReader.getBits(14);
    bitReader.skipBits(1);  // marker

    pan_scan_width = 16 * w;
    pan_scan_height = 16 * h;

    /*
if(s->avctx->debug & FF_DEBUG_PICT_INFO)
    av_log(s->avctx, AV_LOG_DEBUG, "sde w:%d, h:%d\n", w, h);
    */

    // return bitContext.buffer_ptr;
    return skipProcessedBytes(bitReader);
}

std::string MPEGSequenceHeader::getStreamDescr()
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
        rez << "Unspeciffed. ";
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
    double fps = getFrameRate();
    rez << fps;
    return rez.str();
}

double MPEGSequenceHeader::getFrameRate()
{
    if (frame_rate_index > 0 && frame_rate_index < sizeof(frame_rates) / sizeof(double))
        return frame_rates[frame_rate_index];
    else
        return 0;
}

void MPEGSequenceHeader::setFrameRate(uint8_t* buff, double fps)
{
    // return; // todo delete this!!!
    for (int i = 1; i < sizeof(frame_rates) / sizeof(double); i++)
        if (abs(frame_rates[i] - fps) < FRAME_RATE_EPS)
        {
            frame_rate_index = i;
            buff[3] = (buff[3] & 0xf0) + i;
            return;
        }
    THROW(ERR_MPEG2_ERR_FPS,
          "Can't set fps to value " << fps << ". Only standard fps values allowed for mpeg2 streams.");
}

void MPEGSequenceHeader::setAspectRation(uint8_t* buff, VideoAspectRatio ar)
{
    // return; // todo delete this!!!
    buff[3] = (buff[3] & 0x0f) + (ar << 4);
}

// --------------- gop header ------------------
uint8_t* MPEGGOPHeader::deserialize(uint8_t* buf, int buf_size)
{
    /*
int drop_frame_flag;
int time_code_hours, time_code_minutes;
int time_code_seconds, time_code_pictures;
int broken_link;
    */

    BitStreamReader bitReader;
    bitReader.setBuffer(buf, buf + buf_size);

    drop_frame_flag = bitReader.getBit();

    time_code_hours = bitReader.getBits(5);
    time_code_minutes = bitReader.getBits(6);
    bitReader.skipBit();  // marker bit
    time_code_seconds = bitReader.getBits(6);
    time_code_pictures = bitReader.getBits(6);

    close_gop = bitReader.getBit();
    /*broken_link indicate that after editing the
      reference frames of the first B-Frames after GOP I-Frame
      are missing (open gop)*/
    broken_link = bitReader.getBit();

    /*
if(s->avctx->debug & FF_DEBUG_PICT_INFO)
    av_log(s->avctx, AV_LOG_DEBUG, "GOP (%2d:%02d:%02d.[%02d]) broken_link=%d\n",
        time_code_hours, time_code_minutes, time_code_seconds,
        time_code_pictures, broken_link);
    */
    return skipProcessedBytes(bitReader);
}

uint32_t MPEGGOPHeader::serialize(uint8_t* buffer)
{
    BitStreamWriter bitWriter;
    // init_bitWriter.putBits(buffer, 8*8); // 8*8 maximum gop header buffer size at bits
    bitWriter.setBuffer(buffer, buffer + 8);
    bitWriter.putBits(1, drop_frame_flag);

    bitWriter.putBits(5, time_code_hours);
    bitWriter.putBits(6, time_code_minutes);
    // skip_bits1(&pBitContext);//marker bit
    bitWriter.putBits(1, 1);
    bitWriter.putBits(6, time_code_seconds);
    bitWriter.putBits(6, time_code_pictures);

    bitWriter.putBits(1, close_gop);
    /*broken_link indicate that after editing the
  reference frames of the first B-Frames after GOP I-Frame
  are missing (open gop)*/
    bitWriter.putBits(1, broken_link);
    bitWriter.flushBits();
    // return ((bitWriter.getBitsCount() | 7) + 1) >> 3;
    uint32_t bitCnt = bitWriter.getBitsCount();
    return (bitCnt >> 3) + (bitCnt & 7 ? 1 : 0);
}

// ----------------- picture headers -----------

MPEGPictureHeader::MPEGPictureHeader(int bufferSize)
    : MPEGRawDataHeader(bufferSize), m_headerSize(0), m_picture_data_len(0)
{
    repeat_first_field = 0;
    repeat_first_field_bitpos = 0;
    top_field_first_bitpos = 0;
}

void MPEGPictureHeader::buildHeader()
{
    BitStreamWriter bitWriter;
    // init_bitWriter.putBits(m_data_buffer, m_max_data_len*8);
    bitWriter.setBuffer(m_data_buffer, m_data_buffer + m_max_data_len);

    bitWriter.putBits(16, 0);
    bitWriter.putBits(16, PICTURE_START_CODE);

    bitWriter.putBits(10, ref);
    bitWriter.putBits(3, pict_type);
    bitWriter.putBits(16, vbv_delay);

    if (pict_type == PCT_P_FRAME || pict_type == PCT_B_FRAME)
    {
        bitWriter.putBits(1, full_pel[0]);
        bitWriter.putBits(3, mpeg_f_code[0][0]);
    }
    if (pict_type == PCT_B_FRAME)
    {
        bitWriter.putBits(1, full_pel[1]);
        bitWriter.putBits(3, mpeg_f_code[1][0]);
    }
    bitWriter.flushBits();
    // m_data_buffer_len =  ((bitWriter.getBitsCount() | 7) + 1) >> 3;
    uint32_t bitCnt = bitWriter.getBitsCount();
    m_data_buffer_len = (bitCnt >> 3) + (bitCnt & 7 ? 1 : 0);
}

void MPEGPictureHeader::buildCodingExtension()
{
    BitStreamWriter bitWriter;
    // init_bitWriter.putBits(m_data_buffer + m_data_buffer_len, (m_max_data_len - m_data_buffer_len)*8);
    uint8_t* bufPtr = m_data_buffer + m_data_buffer_len;
    bitWriter.setBuffer(bufPtr, bufPtr + m_max_data_len - m_data_buffer_len);

    bitWriter.putBits(16, 0);
    bitWriter.putBits(16, EXT_START_CODE);

    bitWriter.putBits(4, PICTURE_CODING_EXT);

    bitWriter.putBits(4, mpeg_f_code[0][1]);
    bitWriter.putBits(4, mpeg_f_code[0][0]);
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
    uint32_t bitCnt = bitWriter.getBitsCount();
    m_data_buffer_len += (bitCnt >> 3) + (bitCnt & 7 ? 1 : 0);
}

uint8_t* MPEGPictureHeader::deserialize(uint8_t* buf, int buf_size)
{
    bitReader.setBuffer(buf, buf + buf_size);

    ref = bitReader.getBits(10); /* temporal ref */
    pict_type = bitReader.getBits(3);
    if (pict_type == 0 || pict_type > 3)
        return 0;

    vbv_delay = bitReader.getBits(16);

    if (pict_type == PCT_P_FRAME || pict_type == PCT_B_FRAME)
    {
        full_pel[0] = bitReader.getBit();
        f_code = bitReader.getBits(3);
        // if (f_code == 0 && avctx->error_resilience >= FF_ER_COMPLIANT)
        //    return 0;
        mpeg_f_code[0][0] = f_code;
        mpeg_f_code[0][1] = f_code;
    }
    if (pict_type == PCT_B_FRAME)
    {
        full_pel[1] = bitReader.getBit();
        f_code = bitReader.getBits(3);
        // if (f_code == 0 && avctx->error_resilience >= FF_ER_COMPLIANT)
        //    return -1;
        mpeg_f_code[1][0] = f_code;
        mpeg_f_code[1][1] = f_code;
    }
    // return bitContext.buffer_ptr;
    uint8_t* new_buff = skipProcessedBytes(bitReader);
    m_headerSize = (int)(new_buff - buf);
    return new_buff;
}

// static void mpeg_decode_picture_coding_extension(MpegEncContext *s)
// int gg = 0;
uint8_t* MPEGPictureHeader::deserializeCodingExtension(BitStreamReader& bitReader)
{
    full_pel[0] = full_pel[1] = 0;

    mpeg_f_code[0][0] = bitReader.getBits(4);
    mpeg_f_code[0][1] = bitReader.getBits(4);
    mpeg_f_code[1][0] = bitReader.getBits(4);
    mpeg_f_code[1][1] = bitReader.getBits(4);

    intra_dc_precision = bitReader.getBits(2);
    picture_structure = bitReader.getBits(2);
    top_field_first_bitpos = bitReader.getBitsCount();
    top_field_first = bitReader.getBit();
    frame_pred_frame_dct = bitReader.getBit();
    concealment_motion_vectors = bitReader.getBit();
    q_scale_type = bitReader.getBit();
    intra_vlc_format = bitReader.getBit();
    alternate_scan = bitReader.getBit();
    repeat_first_field_bitpos = bitReader.getBitsCount();
    repeat_first_field = bitReader.getBit();
    chroma_420_type = bitReader.getBit();
    progressive_frame = bitReader.getBit();

    composite_display_flag = bitReader.getBit();
    // if (progressive_frame == 0)
    //	gg++;
    if (composite_display_flag)
    {
        v_axis = bitReader.getBit();
        field_sequence = bitReader.getBits(3);
        sub_carrier = bitReader.getBit();
        burst_amplitude = bitReader.getBits(7);
        sub_carrier_phase = bitReader.getBits(8);
    }

    /*
if(s->picture_structure == PICT_FRAME){
    s->first_field=0;
    s->v_edge_pos= 16*s->mb_height;
}else{
    s->first_field ^= 1;
    s->v_edge_pos=  8*s->mb_height;
    memset(s->mbskip_table, 0, s->mb_stride*s->mb_height);
}

if(s->alternate_scan){
    ff_init_scantable(s->dsp.idct_permutation, &s->inter_scantable  , ff_alternate_vertical_scan);
    ff_init_scantable(s->dsp.idct_permutation, &s->intra_scantable  , ff_alternate_vertical_scan);
}else{
    ff_init_scantable(s->dsp.idct_permutation, &s->inter_scantable  , ff_zigzag_direct);
    ff_init_scantable(s->dsp.idct_permutation, &s->intra_scantable  , ff_zigzag_direct);
}


    //return bitContext.buffer_ptr;
    return skipProcessedBytes(bitReader);
}
uint8_t* MPEGPictureHeader::deserializeDisplayExtension(BitStreamReader& bitReader)
{
    horizontal_offset = bitReader.getBits(16);
    vertical_offset = bitReader.getBits(16);
//MpegEncContext *s= &s1->mpeg_enc_ctx;

int i,nofco;
nofco = 1;
if(s->progressive_sequence){
    if(s->repeat_first_field){
        nofco++;
        if(s->top_field_first)
            nofco++;
    }
}else{
    if(s->picture_structure == PICT_FRAME){
        nofco++;
        if(s->repeat_first_field)
            nofco++;
    }
}
for(i=0; i<nofco; i++){
    s1->pan_scan.position[i][0]= get_sbits(&s->gb, 16);
    skip_bits(&s->gb, 1); //marker
    s1->pan_scan.position[i][1]= get_sbits(&s->gb, 16);
    skip_bits(&s->gb, 1); //marker
}

if(s->avctx->debug & FF_DEBUG_PICT_INFO)
    av_log(s->avctx, AV_LOG_DEBUG, "pde (%d,%d) (%d,%d) (%d,%d)\n",
        s1->pan_scan.position[0][0], s1->pan_scan.position[0][1],
        s1->pan_scan.position[1][0], s1->pan_scan.position[1][1],
        s1->pan_scan.position[2][0], s1->pan_scan.position[2][1]
    );
    */
    // return bitContext.buffer_ptr;
    return skipProcessedBytes(bitReader);
}

void MPEGPictureHeader::setTempRef(uint32_t number)
{
    if (m_headerIncludedToBuff)
    {
        m_data_buffer[4] = number >> 2;
        m_data_buffer[5] = (m_data_buffer[5] & 0x3f) + ((number << 6) & 0xc0);
    }
    else
        ref = number;
}

void MPEGPictureHeader::setVbvDelay(uint16_t val)
{
    if (m_headerIncludedToBuff)
    {
        uint8_t* vbv_delay_ptr = &m_data_buffer[5];  // skip sequence header and 1-st byte of ref
        vbv_delay_ptr[0] &= 0xF8;
        vbv_delay_ptr[0] |= vbv_delay >> 13;
        vbv_delay_ptr[1] = vbv_delay >> 5;
        vbv_delay_ptr[2] &= 0x07;
        vbv_delay_ptr[2] |= vbv_delay << 3;
    }
    else
        vbv_delay = val;
}

uint32_t MPEGPictureHeader::getPictureSize() { return m_headerSize + m_data_buffer_len; }

bool MPEGPictureHeader::addRawData(uint8_t* buffer, int len, bool headerIncluded, bool isHeader)

{
    bool rez = MPEGRawDataHeader::addRawData(buffer, len, headerIncluded, isHeader);
    if (!isHeader)
        m_picture_data_len += len;
    return rez;
}

uint32_t MPEGPictureHeader::serialize(uint8_t* buffer)
{
    uint32_t rez = MPEGRawDataHeader::serialize(buffer);
    m_picture_data_len = 0;
    return rez;
}

void MPEGSliceHeader::deserialize(uint8_t* buf, int buf_size)
{
    BitStreamReader reader;
    reader.setBuffer(buf, buf + buf_size);
    quantiser_scale_code = reader.getBits(5);
    if (reader.getBit())
    {
        int intra_slice_flag = reader.getBit();
        int intra_slice = reader.getBit();
        reader.skipBits(7);  // reserved_bits
    }
    int extra_bit_slice = reader.getBit();
    macroblocks(reader);
}

void MPEGSliceHeader::macroblocks(BitStreamReader& reader)
{
    int increment = readMacroblockAddressIncrement(reader);
    int incSum = 0;
    while (increment == MB_ESCAPE_CODE)
    {
        incSum += 33;
        increment = readMacroblockAddressIncrement(reader);
    }
    incSum += increment;
}

int MPEGSliceHeader::readMacroblockAddressIncrement(BitStreamReader& reader)
{
    int cnt = 0;
    for (; reader.getBits(1) == 0; cnt++)
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
        switch (reader.getBit())
        {
        case 1:
            return 9 - reader.getBit();
        case 0:
            return 13 - reader.getBits(2);
        }
    case 5:
        switch (reader.getBit())
        {
        case 1:
            return 15 - reader.getBit();
        case 0:
            switch (reader.getBit())
            {  // last bit of first byte
            case 1:
                return 19 - reader.getBits(2);
            case 0:
                switch (reader.getBit())
                {
                case 1:
                    return 21 - reader.getBit();
                case 0:
                    return 25 - reader.getBits(2);
                }
            }
        }
    case 6:
        reader.skipBit();
        return 33 - reader.getBits(3);
    case 7:
        return MB_ESCAPE_CODE;
    }
}

/*
void MPEGSliceHeader::macroblock_modes(BitStreamReader& reader)
{
}
*/
