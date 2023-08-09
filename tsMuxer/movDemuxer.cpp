#include "movDemuxer.h"

#include <math.h>

#include <algorithm>

#include "aac.h"
#include "abstractStreamReader.h"
#include "avPacket.h"
#include "bitStream.h"
#include "hevc.h"
#include "limits.h"
#include "math.h"
#include "subTrackFilter.h"
#include "vodCoreException.h"
#include "vvc.h"

using namespace std;

struct MovDemuxer::MOVParseTableEntry
{
    uint32_t type;
    int (MovDemuxer::*parse)(MOVAtom atom);
};

static const int MP4ESDescrTag = 0x03;
static const int MP4DecConfigDescrTag = 0x04;
static const int MP4DecSpecificDescrTag = 0x05;

#define MKTAG(a, b, c, d) (a | (b << 8) | (c << 16) | (d << 24))

static const char* const mov_mdhd_language_map[] = {
    // see https :  // developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFChap4/qtff4.html
    "eng", "fra", "deu", "ita", "dut", "sve", "spa", "dan", "por", "nor", "heb", "jpn", "ara", "fin", "ell", "isl",
    "mlt", "tur", "hrv", "zho", "urd", "hin", "tha", "kor", "lit", "pol", "hun", "est", "lav", "smi", "fao", "fas",
    "rus", "zho", "nld", "gle", "alb", "ron", "ces", "slk", "slv", "yid", "srp", "mkd", "bul", "ukr", "bel", "uzb",
    "kaz", "aze", "aze", "arm", "geo", "ron", "kir", "tgk", "tuk", "mon", "mon", "pus", "kur", "kas", "snd", "tib",
    "nep", "san", "mar", "ben", "asm", "guj", "pa ", "ori", "mal", "kan", "tam", "tel", "sin", "bur", "khm", "lao",
    "vie", "ind", "tgl", "may", "may", "amh", "tir", "orm", "som", "swa", "kin", "run", "nya", "mlg", "epo", nullptr, nullptr,
    nullptr, nullptr, nullptr,
    /* 100 */
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "cym", "eus", "cat", "lat", "que", "grn", "aym", "crh", "uig",
    "dzo", "jav"};

struct MOVStts
{
    uint32_t count;
    uint64_t duration;
};

struct MOVDref
{
    uint32_t type;
    char* path;
};

struct MOVStsc
{
    int first;
    int count;
    int id;
};

/*
int64_t av_gcd(int64_t a, int64_t b){
    if(b) return av_gcd(b, a%b);
    else  return a;
}
*/

int ff_mov_lang_to_iso639(unsigned code, char* to)
{
    // see http://www.geocities.com/xhelmboyx/quicktime/formats/mp4-layout.txt
    if (code > 138)
    {
        for (int i = 2; i >= 0; i--)
        {
            to[i] = 0x60 + (code & 0x1f);
            code >>= 5;
        }
        return 1;
    }
    // old fashion apple lang code
    if (code >= sizeof(mov_mdhd_language_map) / sizeof(char*))
        return 0;
    if (!mov_mdhd_language_map[code])
        return 0;
    memcpy(to, mov_mdhd_language_map[code], 4);
    return 1;
}

struct MOVStreamContext : public Track
{
    MOVStreamContext()
        : Track(),
          m_indexCur(0),
          ffindex(0),
          next_chunk(0),
          ctts_count(0),
          fps(0),
          ctts_index(0),
          ctts_sample(0),
          sample_size(0),
          sample_count(0),
          keyframe_count(0),
          time_scale(0),
          current_sample(0),
          bytes_per_frame(0),
          samples_per_frame(0),
          pseudo_stream_id(0),
          audio_cid(0),
          width(0),
          height(0),
          bits_per_coded_sample(0),
          channels(0),
          packet_size(0),
          sample_rate(0)
    {
    }
    virtual ~MOVStreamContext() {}

    vector<int64_t> chunk_offsets;
    vector<uint32_t> m_index;
    uint32_t m_indexCur;

    int ffindex;  // the ffmpeg stream id
    int next_chunk;
    unsigned int ctts_count;
    vector<MOVStsc> stsc_data;
    double fps;

    int ctts_index;
    int ctts_sample;
    unsigned int sample_size;
    unsigned int sample_count;
    unsigned int keyframe_count;
    int time_scale;
    // int time_rate;
    int current_sample;
    unsigned int bytes_per_frame;
    unsigned int samples_per_frame;
    int pseudo_stream_id;  ///< -1 means demux all ids
    int16_t audio_cid;     ///< stsd audio compression id
    int width;             ///< tkhd width
    int height;            ///< tkhd height
    int bits_per_coded_sample;
    int channels;
    int packet_size;
    int sample_rate;
    vector<uint32_t> keyframes;
    // vector<MOVDref> drefs;
    vector<MOVStts> stts_data;
    vector<MOVStts> ctts_data;
};

class MovParsedAudioTrackData : public ParsedTrackPrivData
{
   public:
    MovParsedAudioTrackData(MovDemuxer* demuxer, MOVStreamContext* sc)
        : ParsedTrackPrivData(), m_buff(), m_size(0), m_demuxer(demuxer), m_sc(sc), m_aacRaw()
    {
        isAAC = false;
    }
    void setPrivData(uint8_t* buff, int size) override
    {
        m_buff = buff;
        m_size = size;
        m_aacRaw.m_channels = m_sc->channels;
        m_aacRaw.m_sample_rate = m_sc->sample_rate;
        m_aacRaw.m_id = 1;  // MPEG2
        m_aacRaw.m_profile = 0;
        if (size > 0)
            m_aacRaw.m_profile = (buff[0] >> 3) - 1;
        m_aacRaw.m_layer = 0;
        m_aacRaw.m_rdb = 0;
    }
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override
    {
        uint8_t* dst = pkt->data;
        uint8_t* srcEnd = buff + size;
        while (buff < srcEnd - 4)
        {
            int frameSize = m_sc->sample_size;
            if (frameSize == 0)
                frameSize = m_sc->m_index[m_sc->m_indexCur++];
            if (isAAC)
            {
                m_aacRaw.m_channels = m_sc->channels;
                m_aacRaw.buildADTSHeader(dst, frameSize + AAC_HEADER_LEN);
                memcpy(dst + AAC_HEADER_LEN, buff, frameSize);
                dst += frameSize + AAC_HEADER_LEN;
            }
            else
            {
                memcpy(dst, buff, frameSize);
                dst += frameSize;
            }
            buff += frameSize;
        }
    }
    int newBufferSize(uint8_t* buff, int size) override
    {
        int left = size;
        int i = 0;
        for (; left > 4; ++i)
        {
            left -= m_sc->sample_size;
            if (m_sc->sample_size == 0)
            {
                if (m_sc->m_indexCur + i >= (int)m_sc->m_index.size())
                    THROW(ERR_MOV_PARSE, "Out of index for AAC track #" << m_sc->ffindex << " at position "
                                                                        << m_demuxer->getProcessedBytes());
                left -= m_sc->m_index[(int64_t)m_sc->m_indexCur + i];
            }
        }
        if (left < 0 || left > 4)
            THROW(ERR_MOV_PARSE, "Invalid AAC frame for track #" << m_sc->ffindex << " at position "
                                                                 << m_demuxer->getProcessedBytes());
        if (!isAAC)
            i = 0;
        return (size - left) + i * AAC_HEADER_LEN;
    }

    bool isAAC;

   private:
    uint8_t* m_buff;
    int m_size;
    MovDemuxer* m_demuxer;
    MOVStreamContext* m_sc;
    AACCodec m_aacRaw;
};

class MovParsedH264TrackData : public ParsedTrackPrivData
{
   public:
    MovParsedH264TrackData(MovDemuxer* demuxer, MOVStreamContext* sc)
        : ParsedTrackPrivData(), m_sc(sc), m_demuxer(demuxer), nal_length_size(4)
    {
    }
    void setPrivData(uint8_t* buff, int size) override
    {
        spsPpsList.clear();
        if (size < 6)
            THROW(ERR_MOV_PARSE, "Invalid H.264/AVC extra data format");
        nal_length_size = (buff[4] & 0x03) + 1;
        int spsCnt = buff[5] & 0x1f;
        if (spsCnt == 0)
            return;
        uint8_t* src = buff + 6;
        uint8_t* end = buff + size;
        for (; spsCnt > 0; spsCnt--)
        {
            if (src + 2 > end)
                THROW(ERR_MOV_PARSE, "Invalid H.264/AVC extra data format");
            int nalSize = (src[0] << 8) + src[1];
            src += 2;
            if (src + nalSize > end)
                THROW(ERR_MOV_PARSE, "Invalid H.264/AVC extra data format");
            if (nalSize > 0)
            {
                spsPpsList.push_back(vector<uint8_t>());
                for (int i = 0; i < nalSize; ++i, ++src) spsPpsList.rbegin()->push_back(*src);
            }
        }
        int ppsCnt = *src++;
        for (; ppsCnt > 0; ppsCnt--)
        {
            if (src + 2 > end)
                THROW(ERR_MOV_PARSE, "Invalid H.264/AVC extra data format");
            int nalSize = (src[0] << 8) + src[1];
            src += 2;
            if (src + nalSize > end)
                THROW(ERR_MOV_PARSE, "Invalid H.264/AVC extra data format");
            if (nalSize > 0)
            {
                spsPpsList.push_back(vector<uint8_t>());
                for (int i = 0; i < nalSize; ++i, ++src) spsPpsList.rbegin()->push_back(*src);
            }
        }
    }
    int getNalSize(uint8_t* buff)
    {
        if (nal_length_size == 1)
            return buff[0];
        else if (nal_length_size == 2)
            return (buff[0] << 8) + buff[1];
        else if (nal_length_size == 3)
            return (buff[0] << 16) + (buff[1] << 8) + buff[2];
        else if (nal_length_size == 4)
            return (buff[0] << 24) + (buff[1] << 16) + (buff[2] << 8) + buff[3];
        else
            THROW(ERR_MOV_PARSE, "MP4/MOV error: Unsupported H.264/AVC frame length field value " << nal_length_size);
    }

    void extractData(AVPacket* pkt, uint8_t* buff, int size) override
    {
        uint8_t* dst = pkt->data;
        if (!spsPpsList.empty())
        {
            for (auto& i : spsPpsList)
            {
                *dst++ = 0x0;
                *dst++ = 0x0;
                *dst++ = 0x0;
                *dst++ = 0x1;

                memcpy(dst, &i[0], i.size());
                dst += i.size();
            }
            spsPpsList.clear();
        }
        uint8_t* end = buff + size;
        while (buff < end)
        {
            uint32_t nalSize = getNalSize(buff);
            buff += nal_length_size;
            *dst++ = 0x00;
            *dst++ = 0x00;
            *dst++ = 0x00;
            *dst++ = 0x01;
            memcpy(dst, buff, nalSize);
            dst += nalSize;
            buff += nalSize;
        }
    }

    int newBufferSize(uint8_t* buff, int size) override
    {
        uint8_t* end = buff + size;
        size_t nalCnt = 0;
        while (buff < end)
        {
            if (buff + nal_length_size > end)
                THROW(ERR_MOV_PARSE,
                      "MP4/MOV error: Invalid H.264/AVC frame at position " << m_demuxer->getProcessedBytes());
            uint32_t nalSize = getNalSize(buff);
            buff += nal_length_size;
            if (buff + nalSize > end)
                THROW(ERR_MOV_PARSE,
                      "MP4/MOV error: Invalid H.264/AVC frame at position " << m_demuxer->getProcessedBytes());
            buff += nalSize;
            ++nalCnt;
        }
        size_t spsPpsSize = 0;
        for (auto& i : spsPpsList) spsPpsSize += i.size() + 4;

        return (int)(size + spsPpsSize + nalCnt * (4ll - nal_length_size));
    }

   protected:
    MOVStreamContext* m_sc;
    MovDemuxer* m_demuxer;

    vector<vector<uint8_t>> spsPpsList;
    int nal_length_size;
};

class MovParsedH265TrackData : public MovParsedH264TrackData
{
   public:
    MovParsedH265TrackData(MovDemuxer* demuxer, MOVStreamContext* sc) : MovParsedH264TrackData(demuxer, sc) {}

    void setPrivData(uint8_t* buff, int size) override
    {
        spsPpsList = hevc_extract_priv_data(buff, size, &nal_length_size);
    }
};

class MovParsedH266TrackData : public MovParsedH264TrackData
{
   public:
    MovParsedH266TrackData(MovDemuxer* demuxer, MOVStreamContext* sc) : MovParsedH264TrackData(demuxer, sc) {}

    void setPrivData(uint8_t* buff, int size) override
    {
        spsPpsList = vvc_extract_priv_data(buff, size, &nal_length_size);
    }
};

class MovParsedSRTTrackData : public ParsedTrackPrivData
{
   public:
    MovParsedSRTTrackData(MovDemuxer* demuxer, MOVStreamContext* sc)
        : ParsedTrackPrivData(), m_buff(), m_size(0), m_demuxer(demuxer), m_sc(sc), sttsCnt(0)
    {
        m_packetCnt = 0;
        sttsPos = 0;
        m_timeOffset = 0;
    }

    int64_t getSttsVal()
    {
        if (sttsCnt == 0)
        {
            sttsPos++;
            if (sttsPos >= m_sc->stts_data.size())
                THROW(ERR_MOV_PARSE, "MP4/MOV error: invalid stts index for SRT track #"
                                         << m_sc->ffindex << " at position " << m_demuxer->getProcessedBytes());

            sttsCnt = m_sc->stts_data[sttsPos].count;
        }
        sttsCnt--;
        return m_sc->stts_data[sttsPos].duration * 1000 / m_sc->time_scale;
    }

    void setPrivData(uint8_t* buff, int size) override
    {
        m_buff = buff;
        m_size = size;
        sttsCnt = 0;
        sttsPos = -1;
    }
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override
    {
        uint8_t* end = buff + size;
        std::string prefix;
        std::string suffix;
        std::string subtitleText;
        std::vector<pair<int, string>> tags;
        if (m_packetCnt == 0)
            prefix = "\xEF\xBB\xBF";  // UTF-8 header
        int64_t startTime = m_timeOffset;
        int64_t endTime = startTime + getSttsVal();
        prefix += int32ToStr(++m_packetCnt);
        prefix += "\n";
        prefix += floatToTime(startTime / 1e3, ',');
        prefix += " --> ";
        prefix += floatToTime(endTime / 1e3, ',');
        prefix += '\n';
        uint8_t* dst = pkt->data;
        memcpy(dst, prefix.c_str(), prefix.length());
        dst += prefix.length();
        uint32_t unitSize = 0;

        while (unitSize == 0)
        {
            unitSize = (buff[0] << 8) | buff[1];
            buff += 2;
        }
        subtitleText = std::string((char*)buff, unitSize);
        buff += unitSize;

        while (buff < end)
        {
            uint64_t modifierLen = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
            uint32_t modifierType = (buff[4] << 24) | (buff[5] << 16) | (buff[6] << 8) | buff[7];
            buff += 8;
            modifierLen -= 8;
            if (modifierLen == 1)  // 64-bit length
            {
                modifierLen = 0;
                for (int i = 0; i < 8; i++)
                {
                    modifierLen <<= 8;
                    modifierLen |= *buff++;
                }
                modifierLen -= 8;
            }
            if (modifierType == 0x7374796C)  // 'styl' box
            {
                uint16_t entry_count = (buff[0] << 8) | buff[1];
                buff += 2;
                for (size_t i = 0; i < entry_count; i++)
                {
                    prefix = "";
                    suffix = "";
                    uint16_t startChar = (buff[0] << 8) | buff[1];
                    uint16_t endChar = (buff[2] << 8) | buff[3];
                    buff += 6;  // startChar, endChar, font_ID
                    if (startChar < endChar)
                    {
                        if (*buff & 1)
                        {
                            prefix += "<b>";
                            suffix = "</b>" + suffix;
                        }
                        if (*buff & 2)
                        {
                            prefix += "<i>";
                            suffix = "</i>" + suffix;
                        }
                        if (*buff & 4)
                        {
                            prefix += "<u>";
                            suffix = "</u>" + suffix;
                        }
                        tags.insert(tags.begin(), std::make_pair(startChar, prefix));
                        tags.push_back(std::make_pair(endChar, suffix));
                    }
                    buff += 6;  // font-size, text-color-rgba[4]
                }
            }
            else
                buff += modifierLen;
        }
        if (tags.size() > 0)
        {
            sort(tags.begin(), tags.end(), greater<>());
            for (auto i : tags) subtitleText.insert(i.first, i.second);
        }
        memcpy(dst, subtitleText.c_str(), subtitleText.length());
        dst += subtitleText.length();

        memcpy(dst, "\n\n", 2);
        m_timeOffset = endTime;
    }

    int newBufferSize(uint8_t* buff, int size) override
    {
        int64_t stored_sttsCnt = sttsCnt;
        int64_t stored_sttsPos = sttsPos;
        uint8_t* end = buff + size;
        std::string prefix;
        if (m_packetCnt == 0)
            prefix = "\xEF\xBB\xBF";  // UTF-8 header
        int64_t startTime = m_timeOffset;
        int64_t endTime = startTime + getSttsVal();
        if (size <= 2)
        {
            m_timeOffset = endTime;
            return 0;
        }
        prefix += int32ToStr(m_packetCnt + 1);
        prefix += "\n";
        prefix += floatToTime(startTime / 1e3, ',');
        prefix += " --> ";
        prefix += floatToTime(endTime / 1e3, ',');
        prefix += '\n';
        int textLen = 0;
        uint32_t unitSize = 0;

        try
        {
            while (unitSize == 0)
            {
                unitSize = (buff[0] << 8) | buff[1];
                buff += 2;
            }
            textLen = unitSize;
            buff += unitSize;

            while (buff < end)
            {
                uint64_t modifierLen = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
                uint32_t modifierType = (buff[4] << 24) | (buff[5] << 16) | (buff[6] << 8) | buff[7];
                buff += 8;
                modifierLen -= 8;
                if (modifierLen == 1)  // 64-bit length
                {
                    modifierLen = 0;
                    for (int i = 0; i < 8; i++)
                    {
                        modifierLen <<= 8;
                        modifierLen |= *buff++;
                    }
                    modifierLen -= 8;
                }
                if (modifierType == 0x7374796C)  // 'styl' box
                {
                    uint16_t entry_count = (buff[0] << 8) | buff[1];
                    buff += 2;
                    for (size_t i = 0; i < entry_count; i++)
                    {
                        uint16_t startChar = (buff[0] << 8) | buff[1];
                        uint16_t endChar = (buff[2] << 8) | buff[3];
                        buff += 6;                // startChar, endChar, font-ID
                        if (startChar < endChar)  // face style flags
                        {
                            if (*buff & 1)  // bold
                                textLen += 7;
                            if (*buff & 2)  // italics
                                textLen += 7;
                            if (*buff & 4)  // underline
                                textLen += 7;
                        }
                        buff += 6;  // font-size, text-color-rgba[4]
                    }
                }
                else
                    buff += modifierLen;
            }
        }
        catch (BitStreamException& e)
        {
            (void)e;
            LTRACE(LT_ERROR, 2, "MP4/MOV error: Invalid SRT frame at position " << m_demuxer->getProcessedBytes());
        }

        sttsCnt = stored_sttsCnt;
        sttsPos = stored_sttsPos;
        return (int)(prefix.length() + textLen + 2);
    }

   private:
    uint8_t* m_buff;
    int m_size;
    MovDemuxer* m_demuxer;
    MOVStreamContext* m_sc;
    int m_packetCnt;
    size_t sttsPos;
    int64_t sttsCnt;
    int64_t m_timeOffset;
};

const MovDemuxer::MOVParseTableEntry MovDemuxer::mov_default_parse_table[] = {
    {MKTAG('a', 'v', 's', 's'), &MovDemuxer::mov_read_extradata},
    {MKTAG('c', 'o', '6', '4'), &MovDemuxer::mov_read_stco},  //
    {MKTAG('c', 't', 't', 's'), &MovDemuxer::mov_read_ctts},  //  // composition time to sample
    {MKTAG('d', 'i', 'n', 'f'), &MovDemuxer::mov_read_default},
    {MKTAG('d', 'r', 'e', 'f'), &MovDemuxer::mov_read_dref},  //
    {MKTAG('e', 'd', 't', 's'), &MovDemuxer::mov_read_default},
    {MKTAG('e', 'l', 's', 't'), &MovDemuxer::mov_read_elst},  //
    //{ MKTAG('e','n','d','a'), &MovDemuxer::mov_read_enda }, //
    {MKTAG('f', 'i', 'e', 'l'), &MovDemuxer::mov_read_extradata},  //
    {MKTAG('f', 't', 'y', 'p'), &MovDemuxer::mov_read_ftyp},       //
    {MKTAG('g', 'l', 'b', 'l'), &MovDemuxer::mov_read_glbl},       //
    {MKTAG('h', 'd', 'l', 'r'), &MovDemuxer::mov_read_hdlr},       //
    //{ MKTAG('i','l','s','t'), &MovDemuxer::mov_read_ilst }, //
    {MKTAG('j', 'p', '2', 'h'), &MovDemuxer::mov_read_extradata},  //
    {MKTAG('m', 'd', 'a', 't'), &MovDemuxer::mov_read_mdat},
    {MKTAG('m', 'd', 'h', 'd'), &MovDemuxer::mov_read_mdhd},  //
    {MKTAG('m', 'd', 'i', 'a'), &MovDemuxer::mov_read_default},
    //{ MKTAG('m','e','t','a'), &MovDemuxer::mov_read_meta }, //
    {MKTAG('m', 'i', 'n', 'f'), &MovDemuxer::mov_read_default},
    {MKTAG('m', 'o', 'o', 'f'), &MovDemuxer::mov_read_moof},  //
    {MKTAG('m', 'o', 'o', 'v'), &MovDemuxer::mov_read_moov},  //
    {MKTAG('m', 'v', 'e', 'x'), &MovDemuxer::mov_read_default},
    {MKTAG('m', 'v', 'h', 'd'), &MovDemuxer::mov_read_mvhd},       //
    {MKTAG('S', 'M', 'I', ' '), &MovDemuxer::mov_read_smi},        //  // Sorenson extension ???
    {MKTAG('a', 'l', 'a', 'c'), &MovDemuxer::mov_read_extradata},  //  // alac specific atom

    {MKTAG('a', 'v', 'c', 'C'), &MovDemuxer::mov_read_glbl},  //
    {MKTAG('m', 'v', 'c', 'C'), &MovDemuxer::mov_read_glbl},  //
    {MKTAG('h', 'v', 'c', 'C'), &MovDemuxer::mov_read_glbl},

    //{ MKTAG('p','a','s','p'), &MovDemuxer::mov_read_pasp }, //
    {MKTAG('s', 't', 'b', 'l'), &MovDemuxer::mov_read_default},
    {MKTAG('s', 't', 'c', 'o'), &MovDemuxer::mov_read_stco},  //
    {MKTAG('s', 't', 's', 'c'), &MovDemuxer::mov_read_stsc},  //
    {MKTAG('s', 't', 's', 'd'), &MovDemuxer::mov_read_stsd},  // sample description
    {MKTAG('s', 't', 's', 's'), &MovDemuxer::mov_read_stss},  // sync sample
    {MKTAG('s', 't', 's', 'z'), &MovDemuxer::mov_read_stsz},  // sample size
    {MKTAG('s', 't', 't', 's'), &MovDemuxer::mov_read_stts},
    {MKTAG('t', 'k', 'h', 'd'), &MovDemuxer::mov_read_tkhd},  // track header
    {MKTAG('t', 'f', 'h', 'd'), &MovDemuxer::mov_read_tfhd},  // track fragment header
    {MKTAG('t', 'r', 'a', 'k'), &MovDemuxer::mov_read_trak},
    {MKTAG('t', 'r', 'a', 'f'), &MovDemuxer::mov_read_default},
    {MKTAG('t', 'r', 'e', 'x'), &MovDemuxer::mov_read_trex},
    {MKTAG('t', 'r', 'k', 'n'), &MovDemuxer::mov_read_trkn},
    {MKTAG('t', 'r', 'u', 'n'), &MovDemuxer::mov_read_trun},
    {MKTAG('u', 'd', 't', 'a'), &MovDemuxer::mov_read_default},
    {MKTAG('w', 'a', 'v', 'e'), &MovDemuxer::mov_read_wave},  //
    {MKTAG('e', 's', 'd', 's'), &MovDemuxer::mov_read_esds},  //
    {MKTAG('w', 'i', 'd', 'e'), &MovDemuxer::mov_read_wide},  // place holder
    {MKTAG('c', 'm', 'o', 'v'), &MovDemuxer::mov_read_cmov},
    {MKTAG(0xa9, 'n', 'a', 'm'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'w', 'r', 't'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'c', 'p', 'y'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'i', 'n', 'f'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'i', 'n', 'f'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'A', 'R', 'T'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'a', 'l', 'b'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'c', 'm', 't'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'a', 'u', 't'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'd', 'a', 'y'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'g', 'e', 'n'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 'e', 'n', 'c'), &MovDemuxer::mov_read_udta_string},
    {MKTAG(0xa9, 't', 'o', 'o'), &MovDemuxer::mov_read_udta_string},

    {0, nullptr}};

MovDemuxer::MovDemuxer(const BufferedReaderManager& readManager)
    : IOContextDemuxer(readManager), m_mdat_size(0), m_fileSize(0), fragment()
{
    found_moov = 0;
    found_moof = false;
    m_mdat_pos = 0;
    itunes_metadata = 0;
    moof_offset = 0;
    fileDuration = 0;
    isom = 0;
    m_curChunk = 0;
    m_firstDemux = true;
    m_fileIterator = nullptr;
    m_firstHeaderSize = 0;
}

void MovDemuxer::readClose() {}

void MovDemuxer::openFile(const std::string& streamName)
{
    m_fileName = streamName;
    found_moov = 0;
    found_moof = false;
    m_mdat_pos = 0;
    itunes_metadata = 0;
    moof_offset = 0;
    fileDuration = 0;
    isom = 0;
    m_curChunk = 0;
    m_firstDemux = true;

    m_curPos = m_bufEnd = nullptr;
    m_processedBytes = 0;
    m_isEOF = false;
    num_tracks = 0;

    readClose();

    if (!m_bufferedReader->openStream(m_readerID, streamName.c_str()))
        THROW(ERR_FILE_NOT_FOUND, "Can't open stream " << streamName);

    File tmpFile;
    tmpFile.open(streamName.c_str(), File::ofRead);
    tmpFile.size(&m_fileSize);
    tmpFile.close();

    m_processedBytes = 0;
    m_isEOF = false;
    readHeaders();
    if (m_mdat_pos && m_processedBytes != m_mdat_pos)
        url_fseek(m_mdat_pos);
    buildIndex();
    m_firstHeaderSize = m_processedBytes;
}

void MovDemuxer::buildIndex()
{
    m_curChunk = 0;
    chunks.clear();

    if (num_tracks == 1 && ((MOVStreamContext*)tracks[0])->chunk_offsets.empty())
    {
        chunks.push_back(make_pair(0, 0));
    }
    else
    {
        for (int i = 0; i < num_tracks; ++i)
        {
            auto st = (MOVStreamContext*)tracks[i];
            for (auto& j : st->chunk_offsets)
            {
                if (!found_moof)
                    if (j < m_mdat_pos || j > m_mdat_pos + m_mdat_size)
                        THROW(ERR_MOV_PARSE, "Invalid chunk offset " << j);
                chunks.push_back(make_pair(j - m_mdat_pos, i));
            }
        }
        sort(chunks.begin(), chunks.end());
    }
}

void MovDemuxer::readHeaders()
{
    // check MOV header
    MOVAtom atom;
    atom.size = LLONG_MAX;
    m_mdat_pos = 0;
    if (mov_read_default(atom) < 0)
        THROW(ERR_MOV_PARSE, "error reading header");
    if (!found_moov)
        THROW(ERR_MOV_PARSE, "moov atom not found");
}

int MovDemuxer::simpleDemuxBlock(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, int64_t& discardSize)
{
    for (unsigned int acceptedPID : acceptedPIDs) demuxedData[acceptedPID];
    discardSize = m_firstHeaderSize;
    m_firstHeaderSize = 0;
    if (m_firstDemux)
    {
        m_firstDemux = false;
        int64_t beforeHeadersPos = m_processedBytes;
        if (m_mdat_pos == 0)
        {
            readHeaders();
            if (m_lastReadRez == BufferedReader::DATA_EOF)
                return m_lastReadRez;
            buildIndex();
            if (m_mdat_pos && m_processedBytes != m_mdat_pos)
                url_fseek(m_mdat_pos);
        }
        discardSize += m_mdat_pos - beforeHeadersPos;
        if (chunks.size() > 0)
        {
            discardSize += chunks[m_curChunk].first;
            skip_bytes(chunks[m_curChunk].first);
        }
    }
    uint64_t startPos = m_processedBytes;
    while (m_processedBytes - startPos < m_fileBlockSize && m_curChunk < chunks.size())
    {
        int64_t offset = chunks[m_curChunk].first;
        int64_t next = LLONG_MAX;
        if (m_curChunk < chunks.size() - 1)
            next = chunks[m_curChunk + 1].first;
        else
        {
            next = m_mdat_size;
            m_firstDemux = true;
            m_mdat_pos = 0;
        }
        int chunkSize = (int)(found_moof ? m_mdat_data[m_curChunk].second : next - offset);
        int trackId = (int)chunks[m_curChunk].second;
        auto filterItr = m_pidFilters.find(trackId + 1ll);
        if (filterItr == m_pidFilters.end() && acceptedPIDs.find(trackId + 1ll) == acceptedPIDs.end())
        {
            discardSize += chunkSize;
            skip_bytes(chunkSize);
        }
        else if (chunkSize)
        {
            MemoryBlock& vect = demuxedData[trackId + 1ll];
            auto st = (MOVStreamContext*)tracks[trackId];
            int64_t oldSize = vect.size();
            if (st->parsed_priv_data)
            {
                if (chunkSize > (int)m_tmpChunkBuffer.size())
                    m_tmpChunkBuffer.resize(chunkSize);
                int64_t readed = get_buffer(&m_tmpChunkBuffer[0], chunkSize);
                if (readed == 0)
                    break;
                m_deliveredPacket.size = st->parsed_priv_data->newBufferSize(&m_tmpChunkBuffer[0], chunkSize);
                if (m_deliveredPacket.size)
                {
                    if (filterItr != m_pidFilters.end())
                    {
                        m_filterBuffer.resize(m_deliveredPacket.size);
                        m_deliveredPacket.data = m_filterBuffer.data();
                        st->parsed_priv_data->extractData(&m_deliveredPacket, &m_tmpChunkBuffer[0], chunkSize);
                        int demuxed = filterItr->second->demuxPacket(demuxedData, acceptedPIDs, m_deliveredPacket);
                        discardSize += (int64_t)chunkSize - demuxed;
                    }
                    else
                    {
                        discardSize += (int64_t)chunkSize - m_deliveredPacket.size;
                        vect.grow(m_deliveredPacket.size);
                        m_deliveredPacket.data = vect.data() + oldSize;
                        st->parsed_priv_data->extractData(&m_deliveredPacket, &m_tmpChunkBuffer[0], chunkSize);
                    }
                }
                else
                {
                    discardSize += chunkSize;
                }
            }
            else
            {
                if (filterItr != m_pidFilters.end())
                {
                    m_filterBuffer.resize(chunkSize);
                    int readed = get_buffer(m_filterBuffer.data(), chunkSize);
                    if (readed < chunkSize)
                        m_filterBuffer.grow(readed - chunkSize);
                    if (readed == 0)
                        break;
                    m_deliveredPacket.data = m_filterBuffer.data();
                    m_deliveredPacket.size = (int)m_filterBuffer.size();
                    int demuxed = filterItr->second->demuxPacket(demuxedData, acceptedPIDs, m_deliveredPacket);
                    discardSize += (int64_t)chunkSize - demuxed;
                }
                else
                {
                    vect.grow(chunkSize);
                    int readed = get_buffer(vect.data() + oldSize, chunkSize);
                    if (readed < chunkSize)
                    {
                        vect.grow(readed - chunkSize);
                    }
                    if (readed == 0)
                        break;
                }
            }
        }
        if (found_moof && m_curChunk < chunks.size() - 1)
            skip_bytes(next - offset - m_mdat_data[m_curChunk].second);
        m_curChunk++;
    }

    if (m_processedBytes > startPos)
        return 0;
    else if (m_fileIterator)
    {
        std::string nextName = m_fileIterator->getNextName();
        if (!nextName.empty())
        {
            openFile(nextName);
            return 0;
        }
    }

    m_lastReadRez = BufferedReader::DATA_EOF;
    return m_lastReadRez;
}

void MovDemuxer::getTrackList(std::map<uint32_t, TrackInfo>& trackList)
{
    for (int i = 0; i < num_tracks; i++)
    {
        if (tracks[i]->type != IOContextTrackType::CONTROL)
            trackList.insert(
                std::make_pair(i + 1, TrackInfo(tracks[i]->type == IOContextTrackType::SUBTITLE ? TRACKTYPE_SRT : 0,
                                                tracks[i]->language, 0)));
    }
}

int MovDemuxer::mov_read_default(MOVAtom atom)
{
    int64_t total_size = 0;
    MOVAtom a;
    int err = 0;

    a.offset = atom.offset;
    if (atom.size < 0)
        atom.size = LLONG_MAX;

    while (((total_size + 8) < atom.size) && !m_isEOF && !err)
    {
        a.size = atom.size;
        a.type = 0;
        if (atom.size >= 8)
        {
            a.size = get_be32();
            a.type = get_le32();
        }
        total_size += 8;
        a.offset += 8;
        if (a.size == 1)
        {  // 64 bit extended size
            a.size = get_be64() - 8;
            a.offset += 8;
            total_size += 8;
        }
        if (a.size == 0)
        {
            a.size = atom.size - total_size;
            if (a.size <= 8)
                break;
        }
        a.size -= 8;
        if (a.size < 0)
            break;
        a.size = FFMIN(a.size, atom.size - total_size);

        int64_t left = a.size;
        for (int i = 0; mov_default_parse_table[i].type; i++)
        {
            if (mov_default_parse_table[i].type == a.type)
            {
                int64_t start_pos = m_processedBytes;
                err = (this->*(mov_default_parse_table[i].parse))(a);
                // if (url_is_streamed(pb) && found_moov && found_mdat) break;
                left = a.size - m_processedBytes + start_pos;
                break;
            }
        }
        if ((!found_moof && m_mdat_pos && found_moov) || (found_moof && m_processedBytes + left >= m_fileSize))
            return 0;

        skip_bytes(left);

        a.offset += a.size;
        total_size += a.size;
    }

    if (!err && total_size < atom.size && atom.size < 0x7ffff)
        skip_bytes(atom.size - total_size);

    return err;
}

int MovDemuxer::mov_read_udta_string(MOVAtom atom)
{
    char str[1024]{}, language[4] = {0};
    const char* key = nullptr;
    int str_size;

    if (itunes_metadata)
    {
        int data_size = get_be32();
        int tag = get_le32();
        if (tag == MKTAG('d', 'a', 't', 'a'))
        {
            get_be32();  // type
            get_be32();  // unknown
            str_size = data_size - 16;
            atom.size -= 16;
        }
        else
            return 0;
    }
    else
    {
        str_size = get_be16();  // string length
        ff_mov_lang_to_iso639(get_be16(), language);
        atom.size -= 4;
    }
    switch (atom.type)
    {
    case MKTAG(0xa9, 'n', 'a', 'm'):
        key = "title";
        break;
    case MKTAG(0xa9, 'a', 'u', 't'):
    case MKTAG(0xa9, 'A', 'R', 'T'):
    case MKTAG(0xa9, 'w', 'r', 't'):
        key = "author";
        break;
    case MKTAG(0xa9, 'c', 'p', 'y'):
        key = "copyright";
        break;
    case MKTAG(0xa9, 'c', 'm', 't'):
    case MKTAG(0xa9, 'i', 'n', 'f'):
        key = "comment";
        break;
    case MKTAG(0xa9, 'a', 'l', 'b'):
        key = "album";
        break;
    case MKTAG(0xa9, 'd', 'a', 'y'):
        key = "year";
        break;
    case MKTAG(0xa9, 'g', 'e', 'n'):
        key = "genre";
        break;
    case MKTAG(0xa9, 't', 'o', 'o'):
    case MKTAG(0xa9, 'e', 'n', 'c'):
        key = "muxer";
        break;
    }
    if (!key)
        return 0;
    if (atom.size < 0)
        return -1;

    str_size = (uint16_t)FFMIN(FFMIN((int)(sizeof(str) - 1), str_size), atom.size);
    get_buffer((uint8_t*)str, str_size);
    str[str_size] = 0;
    metaData[key] = str;
    return 0;
}

int MovDemuxer::mov_read_cmov(MOVAtom atom) { THROW(ERR_MOV_PARSE, "Compressed MOV not supported in current version"); }

int MovDemuxer::mov_read_wide(MOVAtom atom)
{
    if (atom.size < 8)
        return 0;  // continue
    if (get_be32() != 0)
    {  // 0 sized mdat atom... use the 'wide' atom size
        skip_bytes(atom.size - 4);
        return 0;
    }
    atom.type = get_le32();
    atom.offset += 8;
    atom.size -= 8;
    if (atom.type != MKTAG('m', 'd', 'a', 't'))
    {
        skip_bytes(atom.size);
        return 0;
    }
    int err = mov_read_mdat(atom);
    return err;
}

// this atom contains actual media data
int MovDemuxer::mov_read_mdat(MOVAtom atom)
{
    if (atom.size == 0)  // wrong one (MP4)
        return 0;
    if (m_mdat_pos == 0)
    {
        m_mdat_pos = m_processedBytes;
        m_mdat_size = atom.size;
    }
    m_mdat_data.push_back(make_pair(m_processedBytes, atom.size));
    return 0;  // now go for moov
}

int MovDemuxer::mov_read_trun(MOVAtom atom)
{
    MOVFragment* frag = &fragment;
    int data_offset = 0;
    unsigned first_sample_flags = frag->flags;

    if (!frag->track_id || frag->track_id > num_tracks)
        return -1;
    Track* st = tracks[frag->track_id - 1];
    MOVStreamContext* sc = (MOVStreamContext*)st;
    if (sc->pseudo_stream_id + 1 != frag->stsd_id)
        return 0;
    get_byte();  // version
    int flags = get_be24();
    unsigned entries = get_be32();
    if (flags & 0x001)
        data_offset = get_be32();
    if (flags & 0x004)
        first_sample_flags = get_be32();
    uint64_t offset = frag->base_data_offset + data_offset;
    sc->chunk_offsets.push_back(offset);
    for (size_t i = 0; i < entries; i++)
    {
        unsigned sample_size = frag->size;
        int sample_flags = i ? frag->flags : first_sample_flags;
        unsigned sample_duration = frag->duration;

        if (flags & 0x100)
            sample_duration = get_be32();
        if (flags & 0x200)
            sample_size = get_be32();
        if (flags & 0x400)
            sample_flags = get_be32();
        if (flags & 0x800)
        {
            sc->ctts_data.push_back(MOVStts());
            sc->ctts_data[sc->ctts_count].count = 1;
            sc->ctts_data[sc->ctts_count].duration = get_be32();
            sc->ctts_count++;
        }

        // assert(sample_duration % sc->time_rate == 0);
        offset += sample_size;
    }
    frag->moof_offset = offset;
    return 0;
}

int MovDemuxer::mov_read_trkn(MOVAtom atom)
{
    get_be32();  // type
    get_be32();  // unknown
    metaData["track"] = int32ToStr(get_be32());
    return 0;
}

int MovDemuxer::mov_read_trex(MOVAtom atom)
{
    trex_data.push_back(MOVTrackExt());
    MOVTrackExt& trex = trex_data[trex_data.size() - 1];
    get_byte();  // version
    get_be24();  // flags
    trex.track_id = get_be32();
    trex.stsd_id = get_be32();
    trex.duration = get_be32();
    trex.size = get_be32();
    trex.flags = get_be32();
    return 0;
}

int MovDemuxer::mov_read_trak(MOVAtom atom)
{
    auto sc = new MOVStreamContext();
    Track* st = tracks[num_tracks] = sc;
    num_tracks++;
    st->type = IOContextTrackType::DATA;
    sc->ffindex = num_tracks;  // st->index;
    return mov_read_default(atom);
}

int MovDemuxer::mov_read_tfhd(MOVAtom atom)
{
    MOVFragment* frag = &fragment;
    MOVTrackExt* trex = nullptr;

    get_byte();  // version
    int flags = get_be24();

    int track_id = get_be32();
    if (!track_id || track_id > num_tracks)
        return -1;
    frag->track_id = track_id;
    for (auto& i : trex_data)
        if (i.track_id == frag->track_id)
        {
            trex = &i;
            break;
        }
    if (!trex)
        THROW(ERR_COMMON, "could not find corresponding trex");

    if (flags & 0x01)
        frag->base_data_offset = get_be64();
    else
        frag->base_data_offset = frag->moof_offset;
    if (flags & 0x02)
        frag->stsd_id = get_be32();
    else
        frag->stsd_id = trex->stsd_id;

    frag->duration = flags & 0x08 ? get_be32() : trex->duration;
    frag->size = flags & 0x10 ? get_be32() : trex->size;
    frag->flags = flags & 0x20 ? get_be32() : trex->flags;
    return 0;
}

int MovDemuxer::mov_read_tkhd(MOVAtom atom) { return 0; }

int MovDemuxer::mov_read_ctts(MOVAtom atom)
{
    auto st = (MOVStreamContext*)tracks[num_tracks - 1];
    get_byte();  // version
    get_be24();  // flags
    int entries = get_be32();
    st->ctts_data.resize(entries);
    for (int i = 0; i < entries; i++)
    {
        st->ctts_data[i].count = get_be32();
        st->ctts_data[i].duration = get_be32();
        // st->time_rate= av_gcd(st->time_rate, abs(st->ctts_data[i].duration));
    }
    return 0;
}

int MovDemuxer::mov_read_stts(MOVAtom atom)
{
    auto st = (MOVStreamContext*)tracks[num_tracks - 1];
    get_byte();  // version
    get_be24();  // flags
    int entries = get_be32();
    st->stts_data.resize(entries);
    for (int i = 0; i < entries; i++)
    {
        st->stts_data[i].count = get_be32();
        st->stts_data[i].duration = get_be32();
        if (i == 0)
        {
            // int64_t tmp = (int64_t) st->time_scale * (int64_t)1000000ll / st->stts_data[i].duration;
            // st->fps = tmp / 1000000.0;
            st->fps = st->time_scale / (double)st->stts_data[i].duration;
        }
    }
    return 0;
}

int MovDemuxer::mov_read_stsz(MOVAtom atom)
{
    auto st = (MOVStreamContext*)tracks[num_tracks - 1];
    get_byte();  // version
    get_be24();  // flags
    st->sample_size = get_be32();
    unsigned int entries = get_be32();
    if (st->sample_size)
        return 0;
    if (entries >= UINT_MAX / sizeof(int))
        return -1;
    for (size_t i = 0; i < entries; i++) st->m_index.push_back(get_be32());
    return 0;
}

int MovDemuxer::mov_read_stss(MOVAtom atom)
{
    auto st = (MOVStreamContext*)tracks[num_tracks - 1];
    get_byte();  // version
    get_be24();  // flags

    unsigned int entries = get_be32();
    if (st->sample_size)
        return 0;
    if (entries >= UINT_MAX / sizeof(int))
        return -1;
    for (size_t i = 0; i < entries; i++) st->keyframes.push_back(get_be32());
    return 0;
}

int MovDemuxer::mov_read_extradata(MOVAtom atom)
{
    if (num_tracks < 1)  // will happen with jp2 files
        return 0;
    Track* st = tracks[num_tracks - 1];
    int64_t newSize = st->codec_priv_size + atom.size + 8;
    if (newSize > INT_MAX || (uint64_t)atom.size > INT_MAX)
        return -1;

    int64_t oldSize = st->codec_priv_size;
    auto tmp = new uint8_t[oldSize];
    memcpy(tmp, st->codec_priv, oldSize);
    delete[] st->codec_priv;
    st->codec_priv = new uint8_t[newSize];
    memcpy(st->codec_priv, tmp, oldSize);
    delete[] tmp;
    uint8_t* buf = st->codec_priv + oldSize;

    //  !!! PROBLEM WITH MP4 SIZE ABOVE 4GB: TODO...
    AV_WB32(buf, (uint32_t)(atom.size + 8));
    AV_WB32(buf + 4, my_htonl(atom.type));
    get_buffer(buf + 8, (int)atom.size);
    st->codec_priv_size = (int)newSize;
    if (st->parsed_priv_data)
        st->parsed_priv_data->setPrivData(st->codec_priv, st->codec_priv_size);
    return 0;
}

int MovDemuxer::mov_read_moov(MOVAtom atom)
{
    if (mov_read_default(atom) < 0)
        return -1;
    found_moov = 1;
    return 0;
}

int MovDemuxer::mov_read_moof(MOVAtom atom)
{
    MOVFragment* frag = &fragment;
    found_moof = true;
    frag->moof_offset = m_processedBytes - 8;
    return mov_read_default(atom);
}

int MovDemuxer::mov_read_mvhd(MOVAtom atom)
{
    int version = get_byte();  // version
    get_be24();                // flags
    if (version == 1)
    {
        get_be64();
        get_be64();
    }
    else
    {
        get_be32();  // creation time
        get_be32();  // modification time
    }
    m_timescale = get_be32();                                      // time scale
    uint64_t duration = (version == 1) ? get_be64() : get_be32();  // duration
    fileDuration = duration * 1000000000ll / m_timescale;
    get_be32();      // preferred scale
    get_be16();      // preferred volume
    skip_bytes(10);  // reserved
    skip_bytes(36);  // display matrix
    get_be32();      // preview time
    get_be32();      // preview duration
    get_be32();      // poster time
    get_be32();      // selection time
    get_be32();      // selection duration
    get_be32();      // current time
    get_be32();      // next track ID
    return 0;
}

int64_t MovDemuxer::getFileDurationNano() const { return fileDuration; }

int MovDemuxer::mov_read_mdhd(MOVAtom atom)
{
    if (num_tracks == -1)
        return -1;
    auto st = (MOVStreamContext*)tracks[num_tracks - 1];
    int version = get_byte();
    if (version > 1)
        return -1;  // unsupported

    get_be24();  // flags
    if (version == 1)
    {
        get_be64();
        get_be64();
    }
    else
    {
        get_be32();  // creation time
        get_be32();  // modification time
    }
    st->time_scale = get_be32();  // time_scale

    int64_t duration = version == 1 ? get_be64() : get_be32();
    fileDuration = FFMAX(fileDuration, (int64_t)(duration / double(st->time_scale) * 1000000000ll));

    unsigned lang = get_be16();  // language
    ff_mov_lang_to_iso639(lang, st->language);
    get_be16();  // quality

    return 0;
}

int MovDemuxer::mov_read_stsd(MOVAtom atom)
{
    if (num_tracks == -1)
        return -1;
    auto st = (MOVStreamContext*)tracks[num_tracks - 1];

    get_byte();  // version
    get_be24();  // flags

    int entries = get_be32();

    for (int pseudo_stream_id = 0; pseudo_stream_id < entries; pseudo_stream_id++)
    {
        // Parsing Sample description table
        // enum CodecID id;
        MOVAtom a;
        int64_t start_pos = m_processedBytes;
        int size = get_be32();         // size
        uint32_t format = get_le32();  // data format

        get_be32();  // reserved
        get_be16();  // reserved
        get_be16();  // dref_id

        st->pseudo_stream_id = pseudo_stream_id;
        st->type = IOContextTrackType::DATA;
        switch (format)
        {
        case MKTAG('a', 'v', 'c', '1'):
        case MKTAG('a', 'v', 'c', '3'):
        case MKTAG('d', 'v', 'a', 'v'):
        case MKTAG('d', 'v', 'a', '1'):
            st->type = IOContextTrackType::VIDEO;
            st->parsed_priv_data = new MovParsedH264TrackData(this, st);
            break;
        case MKTAG('h', 'v', 'c', '1'):
        case MKTAG('h', 'e', 'v', '1'):
        case MKTAG('d', 'v', 'h', 'e'):
        case MKTAG('d', 'v', 'h', '1'):
            st->parsed_priv_data = new MovParsedH265TrackData(this, st);
            st->type = IOContextTrackType::VIDEO;
            break;
        case MKTAG('m', 'p', '4', 'a'):
        case MKTAG('a', 'c', '-', '3'):
        case MKTAG('e', 'c', '-', '3'):
            st->type = IOContextTrackType::AUDIO;
            st->parsed_priv_data = new MovParsedAudioTrackData(this, st);
            break;
        case MKTAG('t', 'x', '3', 'g'):
            st->type = IOContextTrackType::SUBTITLE;
            st->parsed_priv_data = new MovParsedSRTTrackData(this, st);
            break;
        case MKTAG('t', 'm', 'c', 'd'):
            st->type = IOContextTrackType::CONTROL;
            break;
        }

        if (st->type == IOContextTrackType::VIDEO)
        {
            get_be16();                              // version
            get_be16();                              // revision level
            get_be32();                              // vendor
            get_be32();                              // temporal quality
            get_be32();                              // spatial quality
            get_be16();                              // width
            get_be16();                              // height
            get_be32();                              // horiz resolution
            get_be32();                              // vert resolution
            get_be32();                              // data size, always 0
            get_be16();                              // frames per samples
            skip_bytes(32);                          // codec name, pascal string
            st->bits_per_coded_sample = get_be16();  // depth
            get_be16();                              // colortable id
        }
        else if (st->type == IOContextTrackType::AUDIO)
        {
            uint16_t version = get_be16();
            get_be16();                              // revision level
            get_be32();                              // vendor
            st->channels = get_be16();               // channel count
            st->bits_per_coded_sample = get_be16();  // sample size
            st->audio_cid = get_be16();
            st->packet_size = get_be16();  // packet size = 0
            st->sample_rate = ((get_be32() >> 16));
            // Read QT version 1 fields. In version 0 these do not exist.
            if (!isom)
            {
                if (version == 1)
                {
                    st->samples_per_frame = get_be32();
                    get_be32();  // bytes per packet
                    st->bytes_per_frame = get_be32();
                    get_be32();  // bytes per sample
                }
                else if (version == 2)
                {
                    get_be32();                                     // sizeof struct only
                    st->sample_rate = (int)av_int2dbl(get_be64());  // float 64
                    st->channels = get_be32();
                    get_be32();                              // always 0x7F000000
                    st->bits_per_coded_sample = get_be32();  // bits per channel if sound is uncompressed
                    get_be32();                              // lcpm format specific flag
                    st->bytes_per_frame = get_be32();        // bytes per audio packet if constant
                    st->samples_per_frame = get_be32();      // lpcm frames per audio packet if constant
                    // if (format == MKTAG('l','p','c','m'))
                    //    st->codec->codec_id = mov_get_lpcm_codec_id(st->codec->bits_per_coded_sample, flags);
                }
            }
            /*
switch (st->codec->codec_id) {
case CODEC_ID_PCM_S8:
case CODEC_ID_PCM_U8:
    if (st->codec->bits_per_coded_sample == 16)
        st->codec->codec_id = CODEC_ID_PCM_S16BE;
    break;
case CODEC_ID_PCM_S16LE:
case CODEC_ID_PCM_S16BE:
    if (st->codec->bits_per_coded_sample == 8)
        st->codec->codec_id = CODEC_ID_PCM_S8;
    else if (st->codec->bits_per_coded_sample == 24)
        st->codec->codec_id =
            st->codec->codec_id == CODEC_ID_PCM_S16BE ?
            CODEC_ID_PCM_S24BE : CODEC_ID_PCM_S24LE;
    break;
// set values for old format before stsd version 1 appeared
case CODEC_ID_MACE3:
    sc->samples_per_frame = 6;
    sc->bytes_per_frame = 2*st->codec->channels;
    break;
case CODEC_ID_MACE6:
    sc->samples_per_frame = 6;
    sc->bytes_per_frame = 1*st->codec->channels;
    break;
case CODEC_ID_ADPCM_IMA_QT:
    sc->samples_per_frame = 64;
    sc->bytes_per_frame = 34*st->codec->channels;
    break;
case CODEC_ID_GSM:
    sc->samples_per_frame = 160;
    sc->bytes_per_frame = 33;
    break;
default:
    break;
}

bits_per_sample = av_get_bits_per_sample(st->codec->codec_id);
if (bits_per_sample) {
    st->codec->bits_per_coded_sample = bits_per_sample;
    sc->sample_size = (bits_per_sample >> 3) * st->codec->channels;
}
            */
        }
        else if (st->type == IOContextTrackType::SUBTITLE)
        {
            MOVAtom fake_atom(0, 0, size - (m_processedBytes - start_pos));
            mov_read_glbl(fake_atom);
        }
        else
        {
            // other codec type, just skip (rtp, mp4s, tmcd ...)
            skip_bytes(size - (m_processedBytes - start_pos));
        }

        // this will read extra atoms at the end (wave, alac, damr, avcC, SMI ...)
        a.size = size - (m_processedBytes - start_pos);
        if (a.size > 8)
        {
            if (mov_read_default(a) < 0)
                return -1;
        }
        else if (a.size > 0)
            skip_bytes(a.size);
    }
    return 0;
}

int MovDemuxer::mov_read_stco(MOVAtom atom)
{
    auto sc = (MOVStreamContext*)tracks[num_tracks - 1];

    get_byte();  // version
    get_be24();  // flags

    int entries = get_be32();

    if (entries >= UINT_MAX / sizeof(int64_t))
        return -1;

    // sc->chunk_count = entries;

    if (atom.type == MKTAG('s', 't', 'c', 'o'))
        for (int i = 0; i < entries; i++) sc->chunk_offsets.push_back(get_be32());
    else if (atom.type == MKTAG('c', 'o', '6', '4'))
        for (int i = 0; i < entries; i++) sc->chunk_offsets.push_back(get_be64());
    else
        return -1;

    return 0;
}

int MovDemuxer::mov_read_glbl(MOVAtom atom)
{
    if ((uint64_t)atom.size > (1 << 30))
        return -1;
    Track* st = tracks[num_tracks - 1];
    delete[] st->codec_priv;
    st->codec_priv = new unsigned char[atom.size];
    st->codec_priv_size = (int)atom.size;
    get_buffer(st->codec_priv, (int)atom.size);
    if (st->parsed_priv_data)
        st->parsed_priv_data->setPrivData(st->codec_priv, st->codec_priv_size);
    return 0;
}

int MovDemuxer::mov_read_hdlr(MOVAtom atom)
{
    get_byte();  // version
    get_be24();  // flags

    // component type
    int ctype = get_le32();
    if (!ctype)
        isom = 1;

    get_le32();  // component subtype
    get_be32();  // component  manufacture
    get_be32();  // component flags
    get_be32();  // component flags mask

    skip_bytes(atom.size - (m_processedBytes - atom.offset));
    return 0;
}

int MovDemuxer::mov_read_ftyp(MOVAtom atom)
{
    uint32_t type = get_le32();
    if (type != MKTAG('q', 't', ' ', ' '))
        isom = 1;
    get_be32();  // minor version
    skip_bytes(atom.size - 8);
    return 0;
}

int MovDemuxer::mp4_read_descr(int* tag)
{
    *tag = get_byte();
    int len = 0;
    int count = 4;
    while (count--)
    {
        int c = get_byte();
        len = (len << 7) | (c & 0x7f);
        if (!(c & 0x80))
            break;
    }
    return len;
}

int MovDemuxer::mov_read_esds(MOVAtom atom)
{
    auto st = (MOVStreamContext*)tracks[num_tracks - 1];
    get_be32();  // version + flags
    int tag;
    int len = mp4_read_descr(&tag);
    get_be16();  // ID
    if (tag == MP4ESDescrTag)
        get_byte();  // priority
    len = mp4_read_descr(&tag);
    if (tag == MP4DecConfigDescrTag)
    {
        get_byte();  // object_type_id
        get_byte();  // stream type
        get_be24();  // buffer size db
        get_be32();  // max bitrate
        get_be32();  // avg bitrate
        len = mp4_read_descr(&tag);
        if (tag == MP4DecSpecificDescrTag)
        {
            if ((uint64_t)len > (1 << 30))
                return -1;
            st->codec_priv = new unsigned char[len];
            st->codec_priv_size = len;
            get_buffer(st->codec_priv, len);
            // st->parsed_priv_data = new MovParsedAudioTrackData(this, st);
            if (st->parsed_priv_data)
            {
                ((MovParsedAudioTrackData*)st->parsed_priv_data)->isAAC = true;
                st->parsed_priv_data->setPrivData(st->codec_priv, st->codec_priv_size);
                st->channels = (st->codec_priv[1] >> 3) & 0x0f;
            }
        }
    }
    return 0;
}

int MovDemuxer::mov_read_dref(MOVAtom atom)
{
    /*
    MOVStreamContext* st = (MOVStreamContext*) tracks[num_tracks-1];
get_be32(); // version + flags
int entries = get_be32();
    st->drefs.resize(entries);

for (int i = 0; i < entries; i++) {
    uint32_t size = get_be32();
    int64_t next = m_processedBytes + size - 4;
    MOVDref *dref = &(st->drefs[i]);
    dref->type = get_le32();
            get_be32(); // version + flags
    if (dref->type == MKTAG('a','l','i','s') && size > 150) {
        int volume_len = get_byte();
            }
    }
    */
    return 0;
}
int MovDemuxer::mov_read_stsc(MOVAtom atom)
{
    auto st = (MOVStreamContext*)tracks[num_tracks - 1];
    get_byte();  // version
    get_be24();  // flags

    int entries = get_be32();
    st->stsc_data.resize(entries);

    for (int i = 0; i < entries; i++)
    {
        st->stsc_data[i].first = get_be32();
        st->stsc_data[i].count = get_be32();
        st->stsc_data[i].id = get_be32();
    }
    return 0;
}

int MovDemuxer::mov_read_smi(MOVAtom atom) { return 0; }

int MovDemuxer::mov_read_wave(MOVAtom atom)
{
    if ((uint64_t)atom.size > (1 << 30))
        return -1;
    /*
if (st->codec->codec_id == CODEC_ID_QDM2) {
    // pass all frma atom to codec, needed at least for QDM2
    av_free(st->codec->extradata);
    st->codec->extradata = av_mallocz(atom.size + FF_INPUT_BUFFER_PADDING_SIZE);
    if (!st->codec->extradata)
        return AVERROR(ENOMEM);
    st->codec->extradata_size = atom.size;
    get_buffer(pb, st->codec->extradata, atom.size);
} else
    */
    if (atom.size > 8)
    {  // to read frma, esds atoms
        if (mov_read_default(atom) < 0)
            return -1;
    }
    else
        skip_bytes(atom.size);
    return 0;
}

int MovDemuxer::mov_read_elst(MOVAtom atom)
{
    int version = get_byte();
    get_be24();                   // flags
    int edit_count = get_be32();  // entries

    for (int i = 0; i < edit_count; i++)
    {
        if (version == 1)
        {
            uint64_t duration = get_be64();
            int64_t time = get_be64();
            if (time == -1)
                m_firstTimecode[num_tracks] = duration * 1000 / m_timescale;
        }
        else
        {
            uint64_t duration = get_be32();
            int32_t time = get_be32();
            if (time == -1)
                m_firstTimecode[num_tracks] = duration * 1000 / m_timescale;
        }
    }
    get_be32();  // Media rate
    return 0;
}

double MovDemuxer::getTrackFps(uint32_t trackId)
{
    auto st = (MOVStreamContext*)tracks[trackId - 1];
    return st->fps;
}

void MovDemuxer::setFileIterator(FileNameIterator* itr) { m_fileIterator = itr; }
