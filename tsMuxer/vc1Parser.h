#ifndef VC1_PARSER_H
#define VC1_PARSER_H

#include <types/types.h>

#include <string>

#include "bitStream.h"
#include "memory.h"
#include "vod_common.h"

enum class VC1Code
{
    ENDOFSEQ = 0x0A,
    SLICE,
    FIELD,
    FRAME,
    ENTRYPOINT,
    SEQHDR,

    USER_SLICE = 0x1B,  // user-defined slice
    USER_FIELD,
    USER_FRAME,
    USER_ENTRYPOINT,
    USER_SEQHDR
};

enum class Profile
{
    SIMPLE,
    MAIN,
    COMPLEX,  ///< TODO: WMV9 specific
    ADVANCED
};

const int ff_vc1_fps_nr[7] = {24, 25, 30, 50, 60, 48, 72};
constexpr int ff_vc1_fps_dr[2] = {1000, 1001};

/*
struct AVRational {
        int w, h;
        AVRational() {w = h =0;}
        AVRational(int _w, int _h) {w = _w; h = _h;}
};
*/

const AVRational ff_vc1_pixel_aspect[16] = {
    AVRational(0, 1),   AVRational(1, 1),    AVRational(12, 11), AVRational(10, 11),
    AVRational(16, 11), AVRational(40, 33),  AVRational(24, 11), AVRational(20, 11),
    AVRational(32, 11), AVRational(80, 33),  AVRational(18, 11), AVRational(15, 11),
    AVRational(64, 33), AVRational(160, 99), AVRational(0, 1),   AVRational(0, 1)};

enum class VC1PictType
{
    I_TYPE,
    P_TYPE,
    B_TYPE,
    BI_TYPE
};
extern const char* pict_type_str[4];

class VC1Unit
{
   public:
    VC1Unit() : bitReader(), m_nalBuffer(nullptr), m_nalBufferLen(0) {}
    ~VC1Unit() { delete[] m_nalBuffer; }

    static bool isMarker(const uint8_t* ptr) { return ptr[0] == ptr[1] == 0 && ptr[2] == 1; }

    static uint8_t* findNextMarker(uint8_t* buffer, uint8_t* end)
    {
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

    int64_t vc1_unescape_buffer(uint8_t* src, const int64_t size)
    {
        delete[] m_nalBuffer;
        m_nalBuffer = new uint8_t[size];
        if (size < 4)
        {
            std::copy(src, src + size, m_nalBuffer);
            m_nalBufferLen = size;
            return size;
        }
        int64_t dsize = 0;
        for (int64_t i = 0; i < size; i++, src++)
        {
            if (src[0] == 3 && i >= 2 && !src[-1] && !src[-2] && i < size - 1 && src[1] < 4)
            {
                m_nalBuffer[dsize++] = src[1];
                src++;
                i++;
            }
            else
                m_nalBuffer[dsize++] = *src;
        }
        m_nalBufferLen = dsize;
        return dsize;
    }

    int64_t vc1_escape_buffer(uint8_t* dst) const
    {
        const uint8_t* srcStart = m_nalBuffer;
        const uint8_t* initDstBuffer = dst;
        const uint8_t* srcBuffer = m_nalBuffer;
        const uint8_t* srcEnd = m_nalBuffer + m_nalBufferLen;
        for (srcBuffer += 2; srcBuffer < srcEnd;)
        {
            if (*srcBuffer > 3)
                srcBuffer += 3;
            else if (srcBuffer[-2] == 0 && srcBuffer[-1] == 0)
            {
                memcpy(dst, srcStart, srcBuffer - srcStart);
                dst += srcBuffer - srcStart;
                *dst++ = 3;
                *dst++ = *srcBuffer++;
                for (int k = 0; k < 1; k++)
                    if (srcBuffer < srcEnd)
                    {
                        *dst++ = *srcBuffer++;
                    }
                srcStart = srcBuffer;
            }
            else
                srcBuffer++;
        }
        memcpy(dst, srcStart, srcEnd - srcStart);
        dst += srcEnd - srcStart;
        return dst - initDstBuffer;
    }
    const BitStreamReader& getBitReader() const { return bitReader; }

   protected:
    void updateBits(int bitOffset, int bitLen, int value) const;
    BitStreamReader bitReader;
    uint8_t* m_nalBuffer;
    size_t m_nalBufferLen;
};

class VC1SequenceHeader : public VC1Unit
{
   public:
    VC1SequenceHeader()
        : VC1Unit(),
          profile(Profile::SIMPLE),
          rangered(0),
          max_b_frames(0),
          finterpflag(false),
          level(0),
          coded_width(0),
          coded_height(0),
          display_width(0),
          display_height(0),
          pulldown(0),
          interlace(0),
          tfcntrflag(false),
          psf(0),
          time_base_num(0),
          time_base_den(0),
          hrd_param_flag(false),
          hrd_num_leaky_buckets(0),
          sample_aspect_ratio(1, 1),
          m_fpsFieldBitVal(0)
    {
    }
    Profile profile;
    int rangered;
    int max_b_frames;
    int finterpflag;  ///< INTERPFRM present
    int level;
    int coded_width;
    int coded_height;
    int display_width;
    int display_height;
    int pulldown;     ///< TFF/RFF present
    bool interlace;   ///< Progressive/interlaced (RPTFTM syntax element)
    bool tfcntrflag;  ///< TFCNTR present
    int psf;          ///< Progressive Segmented Frame
    int time_base_num;
    int time_base_den;
    int hrd_param_flag;
    int hrd_num_leaky_buckets;

    /* for decoding entry point */
    int decode_entry_point();
    /* ------------------------*/

    AVRational sample_aspect_ratio;  // w, h
    int decode_sequence_header();
    int decode_sequence_header_adv();
    std::string getStreamDescr() const;
    double getFPS() const;
    void setFPS(double value);

   private:
    int m_fpsFieldBitVal;
};

class VC1Frame : public VC1Unit
{
   public:
    VC1Frame() : VC1Unit(), fcm(0), pict_type(VC1PictType::I_TYPE), rptfrm(0), tff(0), rff(0), rptfrmBitPos(0) {}
    int fcm;
    VC1PictType pict_type;
    int rptfrm;
    int tff;
    int rff;
    int rptfrmBitPos;
    int decode_frame_direct(const VC1SequenceHeader& sequenceHdr, uint8_t* buffer, const uint8_t* end);

   private:
    int vc1_parse_frame_header(const VC1SequenceHeader& sequenceHdr);
    int vc1_parse_frame_header_adv(const VC1SequenceHeader& sequenceHdr);
};

#endif
