#ifndef MATROSKA_PARSER_H
#define MATROSKA_PARSER_H

//#include "avcodecs.h"
#include <vector>

#include "aac.h"
#include "abstractStreamReader.h"
#include "avCodecs.h"
#include "avPacket.h"
#include "ioContextDemuxer.h"
#include "vod_common.h"

typedef IOContextTrackType MatroskaTrackType;
typedef Track MatroskaTrack;

/* EBML version supported */
const static int EBML_VERSION = 1;

/* top-level master-IDs */
const static uint32_t EBML_ID_HEADER = 0x1A45DFA3;

/* IDs in the HEADER master */
const static uint16_t EBML_ID_EBMLVERSION = 0x4286;
const static uint16_t EBML_ID_EBMLREADVERSION = 0x42F7;
const static uint16_t EBML_ID_EBMLMAXIDLENGTH = 0x42F2;
const static uint16_t EBML_ID_EBMLMAXSIZELENGTH = 0x42F3;
const static uint16_t EBML_ID_DOCTYPE = 0x4282;
const static uint16_t EBML_ID_DOCTYPEVERSION = 0x4287;
const static uint16_t EBML_ID_DOCTYPEREADVERSION = 0x4285;

/* IDs in the chapters master */
const static uint16_t MATROSKA_ID_EDITIONENTRY = 0x45B9;
const static uint16_t MATROSKA_ID_CHAPTERATOM = 0xB6;
const static uint16_t MATROSKA_ID_CHAPTERTIMESTART = 0x91;
const static uint16_t MATROSKA_ID_CHAPTERTIMEEND = 0x92;
const static uint16_t MATROSKA_ID_CHAPTERDISPLAY = 0x80;
const static uint16_t MATROSKA_ID_CHAPSTRING = 0x85;
const static uint16_t MATROSKA_ID_EDITIONUID = 0x45BC;
const static uint16_t MATROSKA_ID_EDITIONFLAGHIDDEN = 0x45BD;
const static uint16_t MATROSKA_ID_EDITIONFLAGDEFAULT = 0x45DB;
const static uint16_t MATROSKA_ID_CHAPTERUID = 0x73C4;
const static uint16_t MATROSKA_ID_CHAPTERFLAGHIDDEN = 0x98;

/* general EBML types */
const static uint8_t EBML_ID_VOID = 0xEC;

/*
 * Matroska element IDs. max. 32-bit.
 */

/* toplevel segment */
const static uint32_t MATROSKA_ID_SEGMENT = 0x18538067;

/* matroska top-level master IDs */
const static uint32_t MATROSKA_ID_INFO = 0x1549A966;
const static uint32_t MATROSKA_ID_TRACKS = 0x1654AE6B;
const static uint32_t MATROSKA_ID_CUES = 0x1C53BB6B;
const static uint32_t MATROSKA_ID_TAGS = 0x1254C367;
const static uint32_t MATROSKA_ID_SEEKHEAD = 0x114D9B74;
const static uint32_t MATROSKA_ID_CLUSTER = 0x1F43B675;
const static uint32_t MATROSKA_ID_CHAPTERS = 0x1043A770;

/* IDs in the info master */
const static uint32_t MATROSKA_ID_TIMECODESCALE = 0x2AD7B1;
const static uint16_t MATROSKA_ID_DURATION = 0x4489;
const static uint16_t MATROSKA_ID_TITLE = 0x7BA9;
const static uint16_t MATROSKA_ID_WRITINGAPP = 0x5741;
const static uint16_t MATROSKA_ID_MUXINGAPP = 0x4D80;
const static uint16_t MATROSKA_ID_DATEUTC = 0x4461;
const static uint16_t MATROSKA_ID_SEGMENTUID = 0x73A4;

/* ID in the tracks master */
const static uint8_t MATROSKA_ID_TRACKENTRY = 0xAE;

/* IDs in the trackentry master */
const static uint8_t MATROSKA_ID_TRACKNUMBER = 0xD7;
const static uint16_t MATROSKA_ID_TRACKUID = 0x73C5;
const static uint8_t MATROSKA_ID_TRACKTYPE = 0x83;
const static uint8_t MATROSKA_ID_TRACKAUDIO = 0xE1;
const static uint8_t MATROSKA_ID_TRACKVIDEO = 0xE0;
const static uint8_t MATROSKA_ID_CODECID = 0x86;
const static uint16_t MATROSKA_ID_CODECPRIVATE = 0x63A2;
const static uint32_t MATROSKA_ID_CODECNAME = 0x258688;
const static uint32_t MATROSKA_ID_CODECINFOURL = 0x3B4040;
const static uint32_t MATROSKA_ID_CODECDOWNLOADURL = 0x26B240;
const static uint16_t MATROSKA_ID_TRACKNAME = 0x536E;
const static uint32_t MATROSKA_ID_TRACKLANGUAGE = 0x22B59C;
const static uint8_t MATROSKA_ID_TRACKFLAGENABLED = 0xB9;
const static uint8_t MATROSKA_ID_TRACKFLAGDEFAULT = 0x88;
const static uint8_t MATROSKA_ID_TRACKFLAGLACING = 0x9C;
const static uint16_t MATROSKA_ID_TRACKMINCACHE = 0x6DE7;
const static uint16_t MATROSKA_ID_TRACKMAXCACHE = 0x6DF8;
const static uint32_t MATROSKA_ID_TRACKDEFAULTDURATION = 0x23E383;

const static uint32_t MATROSKA_ID_TRACKCONTENTENCODINGS = 0x6D80;
const static uint32_t MATROSKA_ID_TRACKCONTENTENCODING = 0x6240;
const static uint32_t MATROSKA_ID_ENCODINGCOMPRESSION = 0x5034;
const static uint32_t MATROSKA_ID_ENCODINGCOMPALGO = 0x4254;
const static uint32_t MATROSKA_ID_ENCODINGCOMPSETTINGS = 0x4255;

/* IDs in the trackvideo master */
const static uint32_t MATROSKA_ID_VIDEOFRAMERATE = 0x2383E3;
const static uint16_t MATROSKA_ID_VIDEODISPLAYWIDTH = 0x54B0;
const static uint16_t MATROSKA_ID_VIDEODISPLAYHEIGHT = 0x54BA;
const static uint8_t MATROSKA_ID_VIDEOPIXELWIDTH = 0xB0;
const static uint8_t MATROSKA_ID_VIDEOPIXELHEIGHT = 0xBA;
const static uint8_t MATROSKA_ID_VIDEOFLAGINTERLACED = 0x9A;
const static uint16_t MATROSKA_ID_VIDEOSTEREOMODE = 0x53B9;
const static uint16_t MATROSKA_ID_VIDEOASPECTRATIO = 0x54B3;
const static uint32_t MATROSKA_ID_VIDEOCOLOURSPACE = 0x2EB524;

/* IDs in the trackaudio master */
const static uint8_t MATROSKA_ID_AUDIOSAMPLINGFREQ = 0xB5;
const static uint16_t MATROSKA_ID_AUDIOOUTSAMPLINGFREQ = 0x78B5;

const static uint16_t MATROSKA_ID_AUDIOBITDEPTH = 0x6264;
const static uint8_t MATROSKA_ID_AUDIOCHANNELS = 0x9F;

/* ID in the cues master */
const static uint8_t MATROSKA_ID_POINTENTRY = 0xBB;

/* IDs in the pointentry master */
const static uint8_t MATROSKA_ID_CUETIME = 0xB3;
const static uint8_t MATROSKA_ID_CUETRACKPOSITION = 0xB7;

/* IDs in the cuetrackposition master */
const static uint8_t MATROSKA_ID_CUETRACK = 0xF7;
const static uint8_t MATROSKA_ID_CUECLUSTERPOSITION = 0xF1;

/* IDs in the tags master */
/* TODO */

/* IDs in the seekhead master */
const static uint16_t MATROSKA_ID_SEEKENTRY = 0x4DBB;

/* IDs in the seekpoint master */
const static uint16_t MATROSKA_ID_SEEKID = 0x53AB;
const static uint16_t MATROSKA_ID_SEEKPOSITION = 0x53AC;

/* IDs in the cluster master */
const static uint8_t MATROSKA_ID_CLUSTERTIMECODE = 0xE7;
const static uint8_t MATROSKA_ID_BLOCKGROUP = 0xA0;
const static uint8_t MATROSKA_ID_SIMPLEBLOCK = 0xA3;

/* IDs in the blockgroup master */
const static uint8_t MATROSKA_ID_BLOCK = 0xA1;
const static uint8_t MATROSKA_ID_BLOCKDURATION = 0x9B;
const static uint8_t MATROSKA_ID_BLOCKREFERENCE = 0xFB;

enum class MatroskaEyeMode
{
    MATROSKA_EYE_MODE_MONO = 0x0,
    MATROSKA_EYE_MODE_RIGHT = 0x1,
    MATROSKA_EYE_MODE_LEFT = 0x2,
    MATROSKA_EYE_MODE_BOTH = 0x3
};

enum class MatroskaAspectRatioMode
{
    MATROSKA_ASPECT_RATIO_MODE_FREE = 0x0,
    MATROSKA_ASPECT_RATIO_MODE_KEEP = 0x1,
    MATROSKA_ASPECT_RATIO_MODE_FIXED = 0x2
};

/*
 * These aren't in any way "matroska-form" things,
 * it's just something I use in the muxer/demuxer.
 */

typedef enum
{
    MATROSKA_TRACK_ENABLED = (1 << 0),
    MATROSKA_TRACK_DEFAULT = (1 << 1),
    MATROSKA_TRACK_LACING = (1 << 2),
    MATROSKA_TRACK_REAL_V = (1 << 4),
    MATROSKA_TRACK_SHIFT = (1 << 16)
} MatroskaTrackFlags;

typedef enum
{
    MATROSKA_VIDEOTRACK_INTERLACED = (MATROSKA_TRACK_SHIFT << 0)
} MatroskaVideoTrackFlags;

/*
 * Matroska Codec IDs. Strings.
 */

struct CodecTags
{
    const char* str;
    // enum CodecID id;
    int id;
};

#define MATROSKA_CODEC_ID_HEVC_FOURCC "V_MPEGH/ISO/HEVC"
#define MATROSKA_CODEC_ID_AVC_FOURCC "V_MPEG4/ISO/AVC"
#define MATROSKA_CODEC_ID_VIDEO_VFW_FOURCC "V_MS/VFW/FOURCC"
#define MATROSKA_CODEC_ID_AUDIO_ACM "A_MS/ACM"
#define MATROSKA_CODEC_ID_AUDIO_AC3 "A_AC3"
#define MATROSKA_CODEC_ID_SRT "S_TEXT/UTF8"
#define MATROSKA_CODEC_ID_AUDIO_AAC "A_AAC"
#define MATROSKA_CODEC_ID_AUDIO_PCM_BIG "A_PCM/INT/BIG"
#define MATROSKA_CODEC_ID_AUDIO_PCM_LIT "A_PCM/INT/LIT"
#define MATROSKA_CODEC_ID_AUDIO_ACM "A_MS/ACM"
#define MATROSKA_CODEC_ID_SUBTITLE_PGS "S_HDMV/PGS"

/* max. depth in the EBML tree structure */
#define EBML_MAX_DEPTH 16

// extern CodecTags ff_mkv_codec_tags[];

class ParsedH264TrackData : public ParsedTrackPrivData
{
   public:
    ParsedH264TrackData(uint8_t* buff, int size);
    ~ParsedH264TrackData() override {}
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;

   protected:
    int m_nalSize;
    bool m_firstExtract;

    std::vector<std::vector<uint8_t>> m_spsPpsList;
    void writeNalHeader(uint8_t*& dst);
    size_t getSPSPPSLen();
    int writeSPSPPS(uint8_t* dst);
    virtual bool spsppsExists(uint8_t* buff, int size);
};

class ParsedH265TrackData : public ParsedH264TrackData
{
   public:
    ParsedH265TrackData(uint8_t* buff, int size);
    ~ParsedH265TrackData() override {}

    bool spsppsExists(uint8_t* buff, int size) override;
};

class ParsedAC3TrackData : public ParsedTrackPrivData
{
   public:
    ParsedAC3TrackData(uint8_t* buff, int size);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedAC3TrackData() override {}

   private:
    bool m_firstPacket;
    bool m_shortHeaderMode;
};

class ParsedAACTrackData : public ParsedTrackPrivData
{
   public:
    ParsedAACTrackData(uint8_t* buff, int size);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedAACTrackData() override {}

   private:
    AACCodec m_aacRaw;
};

class ParsedLPCMTrackData : public ParsedTrackPrivData
{
   public:
    ParsedLPCMTrackData(MatroskaTrack* track);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedLPCMTrackData() override {}

   private:
    bool m_convertBytes;
    int m_bitdepth;
    int m_channels;
    MemoryBlock m_waveBuffer;
};

class ParsedSRTTrackData : public ParsedTrackPrivData
{
   public:
    ParsedSRTTrackData(uint8_t* buff, int size);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedSRTTrackData() override {}

   private:
    int m_packetCnt;
};

class ParsedVC1TrackData : public ParsedTrackPrivData
{
   public:
    ParsedVC1TrackData(uint8_t* buff, int size);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedVC1TrackData() override {}

   private:
    std::vector<uint8_t> m_seqHeader;
    bool m_firstPacket;
};

class ParsedPGTrackData : public ParsedTrackPrivData
{
   public:
    ParsedPGTrackData() {}
    ~ParsedPGTrackData() override {}
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;

   private:
};

typedef struct MatroskaVideoTrack
{
    MatroskaTrack track;

    int pixel_width;
    int pixel_height;
    int display_width;
    int display_height;

    uint32_t fourcc;

    MatroskaAspectRatioMode ar_mode;
    MatroskaEyeMode eye_mode;

    //..
} MatroskaVideoTrack;

typedef struct MatroskaAudioTrack
{
    MatroskaTrack track;

    int channels;
    int bitdepth;
    int internal_samplerate;
    int samplerate;
    int block_align;

    /* real audio header */
    int coded_framesize;
    int sub_packet_h;
    int frame_size;
    int sub_packet_size;
    int sub_packet_cnt;
    int pkt_cnt;
    uint8_t* buf;
    //..
} MatroskaAudioTrack;

typedef struct MatroskaSubtitleTrack
{
    MatroskaTrack track;

    int ass;
    //..
} MatroskaSubtitleTrack;

#endif  // MATROSKA_PARSER_H
