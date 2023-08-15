#include "vc1Parser.h"

#include <cmath>
#include <sstream>

#include <fs/systemlog.h>

#include "nalUnits.h"
#include "vodCoreException.h"
#include "vod_common.h"
// ----------------------------------------------------------------------

using namespace std;

const char* pict_type_str[4] = {"I_TYPE", "P_TYPE", "B_TYPE", "BI_TYPE"};

namespace
{
int get_unary(BitStreamReader& bitReader, const bool stop, const int len)
{
    int i;
    for (i = 0; i < len && bitReader.getBit() != stop; i++)
        ;
    return i;
}

int decode012(BitStreamReader& bitReader)
{
    const int n = bitReader.getBit();
    if (n == 0)
        return 0;
    return bitReader.getBit() + 1;
}
}  // namespace

// ---------------------------- VC1Unit ------------------------------------------

void VC1Unit::updateBits(const int bitOffset, const int bitLen, const int value) const
{
    uint8_t* ptr = bitReader.getBuffer() + bitOffset / 8;
    BitStreamWriter bitWriter{};
    const int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    const uint8_t* ptr_end = bitReader.getBuffer() + (bitOffset + bitLen) / 8;
    const int endBitsPostfix = 8 - ((bitOffset + bitLen) % 8);

    if (byteOffset > 0)
    {
        const int prefix = *ptr >> (8 - byteOffset);
        bitWriter.putBits(byteOffset, prefix);
    }
    bitWriter.putBits(bitLen, value);

    if (endBitsPostfix < 8)
    {
        const int postfix = *ptr_end & (1 << endBitsPostfix) - 1;
        bitWriter.putBits(endBitsPostfix, postfix);
    }
    bitWriter.flushBits();
}

// ---------------------------- VC1SequenceHeader ------------------------------------------

string VC1SequenceHeader::getStreamDescr() const
{
    std::ostringstream rez;
    rez << "Profile: ";
    switch (profile)
    {
    case Profile::SIMPLE:
        rez << "Simple";
        break;
    case Profile::MAIN:
        rez << "Main";
        break;
    case Profile::COMPLEX:
        rez << "Complex";
        break;
    case Profile::ADVANCED:
        rez << "Advanced@" << level;
        break;
    }

    rez << " Resolution: " << coded_width << ':' << coded_height;
    rez << (interlace ? 'i' : 'p') << "  ";
    rez << "Frame rate: ";
    const double fps = getFPS();
    if (fps != 0.0)
        rez << fps;
    else
        rez << "not found";
    return rez.str();
}

double VC1SequenceHeader::getFPS() const
{
    if (time_base_num == 0 || time_base_den == 0)
        return 0;
    const double fps = time_base_den / static_cast<double>(time_base_num);
    // if (fps > 25.0 && pulldown)
    //	fps /= 1.25;
    return fps;
}

void VC1SequenceHeader::setFPS(const double value)
{
    // if (value < 25.0 && pulldown)
    //	value *= 1.25;

    if (m_fpsFieldBitVal > 0)
    {
        int nr;
        const int time_scale = lround(value) * 1000;
        const int num_units_in_tick = lround(time_scale / value);
        if ((time_scale == 24000 || time_scale == 25000 || time_scale == 30000 || time_scale == 50000 ||
             time_scale == 60000) &&
            (num_units_in_tick == 1000 || num_units_in_tick == 1001))
        {
            time_base_den = time_scale;
            time_base_num = num_units_in_tick;
        }
        else
            THROW(ERR_VC1_ERR_FPS, "Can't overwrite stream fps. Non standard fps values not supported for VC-1 streams")

        switch (time_scale)
        {
        case 24000:
            nr = 1;
            break;
        case 25000:
            nr = 2;
            break;
        case 30000:
            nr = 3;
            break;
        case 50000:
            nr = 4;
            break;
        case 60000:
            nr = 5;
            break;
        default:
            THROW(ERR_VC1_ERR_FPS, "Can't overwrite stream fps. Non standard fps values not supported for VC-1 streams")
        }
        const int dr = (num_units_in_tick == 1000) ? 1 : 2;

        updateBits(m_fpsFieldBitVal, 8, nr);
        updateBits(m_fpsFieldBitVal + 8, 4, dr);
    }
}

int VC1SequenceHeader::decode_sequence_header()
{
    try
    {
        bitReader.setBuffer(m_nalBuffer, m_nalBuffer + m_nalBufferLen);  // skip 00 00 01 xx marker
        profile = static_cast<Profile>(bitReader.getBits(2));
        if (profile == Profile::COMPLEX)
            LTRACE(LT_WARN, 0, "WMV3 Complex Profile is not fully supported");

        else if (profile == Profile::ADVANCED)
            return decode_sequence_header_adv();
        else
        {
            const auto res_sm = bitReader.getBits<uint8_t>(2);  // reserved
            if (res_sm)
            {
                LTRACE(LT_ERROR, 0, "Reserved RES_SM=" << res_sm << " is forbidden");
                return NALUnit::UNSUPPORTED_PARAM;
            }
        }

        bitReader.skipBits(8);                                 // frmrtq_postproc, bitrtq_postproc
        if (bitReader.getBit() && profile == Profile::SIMPLE)  // loop_filter
            LTRACE(LT_WARN, 0, "LOOPFILTER shell not be enabled in simple profile");
        if (bitReader.getBit())  // reserved res_x8
            LTRACE(LT_WARN, 0, "1 for reserved RES_X8 is forbidden");
        bitReader.skipBit();                        // multires
        const int res_fasttx = bitReader.getBit();  // reserved
        if (!res_fasttx)
            LTRACE(LT_WARN, 0, "0 for reserved RES_FASTTX is forbidden");
        if (profile == Profile::SIMPLE && !bitReader.getBit())  // fastuvmc
        {
            LTRACE(LT_ERROR, 0, "FASTUVMC unavailable in Simple Profile");
            return NALUnit::UNSUPPORTED_PARAM;
        }
        if (profile == Profile::SIMPLE && bitReader.getBit())  // extended_mv
        {
            LTRACE(LT_ERROR, 0, "Extended MVs unavailable in Simple Profile");
            return NALUnit::UNSUPPORTED_PARAM;
        }
        bitReader.skipBits(3);  // dquant, vstransform

        if (bitReader.getBit())  // res_transtab
        {
            LTRACE(LT_ERROR, 0, "1 for reserved RES_TRANSTAB is forbidden\n");
            return NALUnit::UNSUPPORTED_PARAM;
        }
        bitReader.skipBits(2);  // overlap, resync_marker

        rangered = bitReader.getBit();
        if (rangered && profile == Profile::SIMPLE)
            LTRACE(LT_WARN, 0, "RANGERED should be set to 0 in simple profile");
        max_b_frames = bitReader.getBits<uint8_t>(3);
        bitReader.skipBits(2);  // quantizer_mode
        finterpflag = bitReader.getBit();

        if (!bitReader.getBit())  // res_rtm_flag
            LTRACE(LT_WARN, 0, "Old WMV3 version detected.");
        // TODO: figure out what they mean (always 0x402F)
        if (!res_fasttx)
            bitReader.skipBits(16);
        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

int VC1SequenceHeader::decode_sequence_header_adv()
{
    level = bitReader.getBits<uint8_t>(3);
    if (level >= 5)
        LTRACE(LT_WARN, 0, "Reserved LEVEL " << level);
    bitReader.skipBits(11);  // chromaformat, frmrtq_postproc, bitrtq_postproc, postprocflag
    coded_width = (bitReader.getBits<uint16_t>(12) + 1) * 2;
    coded_height = (bitReader.getBits<uint16_t>(12) + 1) * 2;
    pulldown = bitReader.getBit();
    interlace = bitReader.getBit();
    tfcntrflag = bitReader.getBit();
    finterpflag = bitReader.getBit();
    bitReader.skipBit();
    psf = bitReader.getBit();
    /*
if(psf) { //PsF, 6.1.13
    LTRACE(LT_ERROR, 0, "Progressive Segmented Frame mode: not supported (yet)");
            return NALUnit::UNSUPPORTED_PARAM;
}
    */
    max_b_frames = 7;
    if (bitReader.getBit())
    {  // Display Info - decoding is not affected by it
        uint16_t w, h;
        uint8_t ar = 0;
        display_width = w = bitReader.getBits<uint16_t>(14) + 1;
        display_height = h = bitReader.getBits<uint16_t>(14) + 1;
        if (bitReader.getBit())
            ar = bitReader.getBits<uint8_t>(4);
        if (ar > 0 && ar < 14)
        {
            sample_aspect_ratio = ff_vc1_pixel_aspect[ar];
        }
        else if (ar == 15)
        {
            w = bitReader.getBits<uint8_t>(8);
            h = bitReader.getBits<uint8_t>(8);
            sample_aspect_ratio = AVRational(w, h);
        }

        if (bitReader.getBit())
        {  // framerate stuff
            if (bitReader.getBit())
            {
                time_base_num = 32;
                time_base_den = bitReader.getBits<uint16_t>(16) + 1;
            }
            else
            {
                m_fpsFieldBitVal = bitReader.getBitsCount();
                const auto nr = bitReader.getBits<uint8_t>(8);
                const auto dr = bitReader.getBits<uint8_t>(4);
                if (nr > 0 && nr < 8 && dr > 0 && dr < 3)
                {
                    time_base_num = ff_vc1_fps_dr[dr - 1];
                    time_base_den = ff_vc1_fps_nr[nr - 1] * 1000;
                }
                else
                    LTRACE(LT_WARN, 0, "Invalid fps value");
            }
        }

        if (bitReader.getBit())
            bitReader.skipBits(24);  // color_prim, transfer_char, matrix_coef
    }

    hrd_param_flag = bitReader.getBit();
    if (hrd_param_flag)
    {
        hrd_num_leaky_buckets = bitReader.getBits<uint8_t>(5);
        bitReader.skipBits(8);  // bitrate exponent, buffer size exponent
        for (int i = 0; i < hrd_num_leaky_buckets; i++) bitReader.skipBits(32);  // hrd_rate[n], hrd_buffer[n]
    }
    return 0;
}

int VC1SequenceHeader::decode_entry_point()
{
    try
    {
        bitReader.setBuffer(m_nalBuffer, m_nalBuffer + m_nalBufferLen);  // skip 00 00 01 xx marker
        bitReader.skipBit();                                             // blink = broken link
        bitReader.skipBit();                                             // clentry = closed entry
        bitReader.skipBit();                                             // panscanflag
        bitReader.skipBit();                                             // refdist flag
        bitReader.skipBit();                                             // loop_filter
        bitReader.skipBit();                                             // fastuvmc
        const int extended_mv = bitReader.getBit();
        bitReader.skipBits(6);  // dquant, vstransform, overlap, quantizer_mode

        if (hrd_param_flag)
        {
            for (int i = 0; i < hrd_num_leaky_buckets; i++) bitReader.skipBits(8);  // hrd_full[n]
        }

        if (bitReader.getBit())
        {
            coded_width = (bitReader.getBits<uint16_t>(12) + 1) * 2;
            coded_height = (bitReader.getBits<uint16_t>(12) + 1) * 2;
        }
        if (extended_mv)
            bitReader.skipBit();  // extended_dmv
        if (bitReader.getBit())
        {
            // av_log(avctx, AV_LOG_ERROR, "Luma scaling is not supported, expect wrong picture\n");
            bitReader.skipBits(3);  // Y range, ignored for now
        }
        if (bitReader.getBit())
        {
            // av_log(avctx, AV_LOG_ERROR, "Chroma scaling is not supported, expect wrong picture\n");
            bitReader.skipBits(3);  // UV range, ignored for now
        }
        return 0;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

// -------------------------- VC1Frame ---------------------------

int VC1Frame::decode_frame_direct(const VC1SequenceHeader& sequenceHdr, uint8_t* buffer, const uint8_t* end)
{
    try
    {
        bitReader.setBuffer(buffer, end);  // skip 00 00 01 xx marker
        if (sequenceHdr.profile < Profile::ADVANCED)
            return vc1_parse_frame_header(sequenceHdr);
        return vc1_parse_frame_header_adv(sequenceHdr);
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

int VC1Frame::vc1_parse_frame_header(const VC1SequenceHeader& sequenceHdr)
{
    if (sequenceHdr.finterpflag)
        bitReader.skipBit();  // interpfrm
    bitReader.skipBits(2);    // framecnt
    if (sequenceHdr.rangered)
        bitReader.skipBit();  // rangeredfrm
    pict_type = static_cast<VC1PictType>(bitReader.getBit());
    if (sequenceHdr.max_b_frames > 0)
    {
        if (pict_type == VC1PictType::I_TYPE)
        {
            if (bitReader.getBit())
                pict_type = VC1PictType::I_TYPE;
            else
                pict_type = VC1PictType::B_TYPE;
        }
        else
            pict_type = VC1PictType::P_TYPE;
    }
    else
        pict_type = (pict_type != VC1PictType::I_TYPE) ? VC1PictType::P_TYPE : VC1PictType::I_TYPE;
    return 0;
}

int VC1Frame::vc1_parse_frame_header_adv(const VC1SequenceHeader& sequenceHdr)
{
    fcm = sequenceHdr.interlace ? decode012(bitReader) : 0;

    if (fcm == 2)  // is coded field
    {
        switch (bitReader.getBits(3))
        {
        case 0:
        case 1:
            pict_type = VC1PictType::I_TYPE;
            break;
        case 2:
        case 3:
            pict_type = VC1PictType::P_TYPE;
            break;
        case 4:
        case 5:
            pict_type = VC1PictType::B_TYPE;
            break;
        case 6:
        case 7:
            pict_type = VC1PictType::BI_TYPE;
            break;
        default:;
        }
    }
    else
    {
        switch (get_unary(bitReader, false, 4))
        {
        case 0:
            pict_type = VC1PictType::P_TYPE;
            break;
        case 1:
            pict_type = VC1PictType::B_TYPE;
            break;
        case 2:
            pict_type = VC1PictType::I_TYPE;
            break;
        case 3:
            pict_type = VC1PictType::BI_TYPE;
            break;
        case 4:
            pict_type = VC1PictType::P_TYPE;  // skipped pic
            break;
        default:;
        }
    }

    if (sequenceHdr.tfcntrflag)
        bitReader.skipBits(8);  // TFCNTR
    if (sequenceHdr.pulldown)
    {
        rptfrmBitPos = bitReader.getBitsCount();
        if (!sequenceHdr.interlace || sequenceHdr.psf)
        {
            rptfrm = bitReader.getBits<uint8_t>(2);  // Repeat Frame Count (0 .. 3)
        }
        else
        {
            tff = bitReader.getBit();  // TFF (top field first)
            rff = bitReader.getBit();  // RFF (repeat first field)
        }
    }
    return 0;
}
