#ifndef MATROSKA_PARSER_H
#define MATROSKA_PARSER_H

// #include "avcodecs.h"
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
static constexpr int EBML_VERSION = 1;

/* top-level master-IDs */
static constexpr uint32_t EBML_ID_HEADER = 0x1A45DFA3;

/* IDs in the HEADER master */
static constexpr uint16_t EBML_ID_EBMLVERSION = 0x4286;
static constexpr uint16_t EBML_ID_EBMLREADVERSION = 0x42F7;
static constexpr uint16_t EBML_ID_EBMLMAXIDLENGTH = 0x42F2;
static constexpr uint16_t EBML_ID_EBMLMAXSIZELENGTH = 0x42F3;
static constexpr uint16_t EBML_ID_DOCTYPE = 0x4282;
static constexpr uint16_t EBML_ID_DOCTYPEVERSION = 0x4287;
static constexpr uint16_t EBML_ID_DOCTYPEREADVERSION = 0x4285;

/* IDs in the chapters master */
static constexpr uint16_t MATROSKA_ID_EDITIONENTRY = 0x45B9;
static constexpr uint16_t MATROSKA_ID_CHAPTERATOM = 0xB6;
static constexpr uint16_t MATROSKA_ID_CHAPTERTIMESTART = 0x91;
static constexpr uint16_t MATROSKA_ID_CHAPTERTIMEEND = 0x92;
static constexpr uint16_t MATROSKA_ID_CHAPTERDISPLAY = 0x80;
static constexpr uint16_t MATROSKA_ID_CHAPSTRING = 0x85;
static constexpr uint16_t MATROSKA_ID_EDITIONUID = 0x45BC;
static constexpr uint16_t MATROSKA_ID_EDITIONFLAGHIDDEN = 0x45BD;
static constexpr uint16_t MATROSKA_ID_EDITIONFLAGDEFAULT = 0x45DB;
static constexpr uint16_t MATROSKA_ID_CHAPTERUID = 0x73C4;
static constexpr uint16_t MATROSKA_ID_CHAPTERFLAGHIDDEN = 0x98;

/* general EBML types */
static constexpr uint8_t EBML_ID_VOID = 0xEC;
static constexpr uint8_t EBML_ID_CRC32 = 0xBF;

/*
 * Matroska element IDs. max. 32-bit.
 */

/* toplevel segment */
static constexpr uint32_t MATROSKA_ID_SEGMENT = 0x18538067;

/* matroska top-level master IDs */
static constexpr uint32_t MATROSKA_ID_INFO = 0x1549A966;
static constexpr uint32_t MATROSKA_ID_TRACKS = 0x1654AE6B;
static constexpr uint32_t MATROSKA_ID_CUES = 0x1C53BB6B;
static constexpr uint32_t MATROSKA_ID_TAGS = 0x1254C367;
static constexpr uint32_t MATROSKA_ID_SEEKHEAD = 0x114D9B74;
static constexpr uint32_t MATROSKA_ID_CLUSTER = 0x1F43B675;
static constexpr uint32_t MATROSKA_ID_CHAPTERS = 0x1043A770;

/* IDs in the info master */
static constexpr uint32_t MATROSKA_ID_TIMECODESCALE = 0x2AD7B1;
static constexpr uint16_t MATROSKA_ID_DURATION = 0x4489;
static constexpr uint16_t MATROSKA_ID_TITLE = 0x7BA9;
static constexpr uint16_t MATROSKA_ID_WRITINGAPP = 0x5741;
static constexpr uint16_t MATROSKA_ID_MUXINGAPP = 0x4D80;
static constexpr uint16_t MATROSKA_ID_DATEUTC = 0x4461;
static constexpr uint16_t MATROSKA_ID_SEGMENTUID = 0x73A4;

/* ID in the tracks master */
static constexpr uint8_t MATROSKA_ID_TRACKENTRY = 0xAE;

/* IDs in the trackentry master */
static constexpr uint8_t MATROSKA_ID_TRACKNUMBER = 0xD7;
static constexpr uint16_t MATROSKA_ID_TRACKUID = 0x73C5;
static constexpr uint8_t MATROSKA_ID_TRACKTYPE = 0x83;
static constexpr uint8_t MATROSKA_ID_TRACKAUDIO = 0xE1;
static constexpr uint8_t MATROSKA_ID_TRACKVIDEO = 0xE0;
static constexpr uint8_t MATROSKA_ID_CODECID = 0x86;
static constexpr uint16_t MATROSKA_ID_CODECPRIVATE = 0x63A2;
static constexpr uint32_t MATROSKA_ID_CODECNAME = 0x258688;
static constexpr uint32_t MATROSKA_ID_CODECINFOURL = 0x3B4040;
static constexpr uint32_t MATROSKA_ID_CODECDOWNLOADURL = 0x26B240;
static constexpr uint16_t MATROSKA_ID_TRACKNAME = 0x536E;
static constexpr uint32_t MATROSKA_ID_TRACKLANGUAGE = 0x22B59C;
static constexpr uint8_t MATROSKA_ID_TRACKFLAGENABLED = 0xB9;
static constexpr uint8_t MATROSKA_ID_TRACKFLAGDEFAULT = 0x88;
static constexpr uint8_t MATROSKA_ID_TRACKFLAGLACING = 0x9C;
static constexpr uint16_t MATROSKA_ID_TRACKMINCACHE = 0x6DE7;
static constexpr uint16_t MATROSKA_ID_TRACKMAXCACHE = 0x6DF8;
static constexpr uint32_t MATROSKA_ID_TRACKDEFAULTDURATION = 0x23E383;

static constexpr uint32_t MATROSKA_ID_TRACKCONTENTENCODINGS = 0x6D80;
static constexpr uint32_t MATROSKA_ID_TRACKCONTENTENCODING = 0x6240;
static constexpr uint32_t MATROSKA_ID_ENCODINGCOMPRESSION = 0x5034;
static constexpr uint32_t MATROSKA_ID_ENCODINGCOMPALGO = 0x4254;
static constexpr uint32_t MATROSKA_ID_ENCODINGCOMPSETTINGS = 0x4255;

/* IDs in the trackvideo master */
static constexpr uint32_t MATROSKA_ID_VIDEOFRAMERATE = 0x2383E3;
static constexpr uint16_t MATROSKA_ID_VIDEODISPLAYWIDTH = 0x54B0;
static constexpr uint16_t MATROSKA_ID_VIDEODISPLAYHEIGHT = 0x54BA;
static constexpr uint8_t MATROSKA_ID_VIDEOPIXELWIDTH = 0xB0;
static constexpr uint8_t MATROSKA_ID_VIDEOPIXELHEIGHT = 0xBA;
static constexpr uint8_t MATROSKA_ID_VIDEOFLAGINTERLACED = 0x9A;
static constexpr uint16_t MATROSKA_ID_VIDEOSTEREOMODE = 0x53B9;
static constexpr uint16_t MATROSKA_ID_VIDEOASPECTRATIO = 0x54B3;
static constexpr uint32_t MATROSKA_ID_VIDEOCOLOURSPACE = 0x2EB524;

/* IDs in the trackaudio master */
static constexpr uint8_t MATROSKA_ID_AUDIOSAMPLINGFREQ = 0xB5;
static constexpr uint16_t MATROSKA_ID_AUDIOOUTSAMPLINGFREQ = 0x78B5;

static constexpr uint16_t MATROSKA_ID_AUDIOBITDEPTH = 0x6264;
static constexpr uint8_t MATROSKA_ID_AUDIOCHANNELS = 0x9F;

/* ID in the cues master */
static constexpr uint8_t MATROSKA_ID_POINTENTRY = 0xBB;

/* IDs in the pointentry master */
static constexpr uint8_t MATROSKA_ID_CUETIME = 0xB3;
static constexpr uint8_t MATROSKA_ID_CUETRACKPOSITION = 0xB7;

/* IDs in the cuetrackposition master */
static constexpr uint8_t MATROSKA_ID_CUETRACK = 0xF7;
static constexpr uint8_t MATROSKA_ID_CUECLUSTERPOSITION = 0xF1;

/* IDs in the tags master */
/* TODO */

/* IDs in the seekhead master */
static constexpr uint16_t MATROSKA_ID_SEEKENTRY = 0x4DBB;

/* IDs in the seekpoint master */
static constexpr uint16_t MATROSKA_ID_SEEKID = 0x53AB;
static constexpr uint16_t MATROSKA_ID_SEEKPOSITION = 0x53AC;

/* IDs in the cluster master */
static constexpr uint8_t MATROSKA_ID_CLUSTERTIMECODE = 0xE7;
static constexpr uint8_t MATROSKA_ID_BLOCKGROUP = 0xA0;
static constexpr uint8_t MATROSKA_ID_SIMPLEBLOCK = 0xA3;

/* IDs in the blockgroup master */
static constexpr uint8_t MATROSKA_ID_BLOCK = 0xA1;
static constexpr uint8_t MATROSKA_ID_BLOCKDURATION = 0x9B;
static constexpr uint8_t MATROSKA_ID_BLOCKREFERENCE = 0xFB;

enum class MatroskaEyeMode
{
    MONO = 0x0,
    RIGHT = 0x1,
    LEFT = 0x2,
    BOTH = 0x3
};

enum class MatroskaAspectRatioMode
{
    FREE = 0x0,
    KEEP = 0x1,
    FIXED = 0x2
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
    int id;
};

constexpr auto MATROSKA_CODEC_ID_VVC_FOURCC = "V_MPEGI/ISO/VVC";
constexpr auto MATROSKA_CODEC_ID_HEVC_FOURCC = "V_MPEGH/ISO/HEVC";
constexpr auto MATROSKA_CODEC_ID_AVC_FOURCC = "V_MPEG4/ISO/AVC";
constexpr auto MATROSKA_CODEC_ID_VIDEO_VFW_FOURCC = "V_MS/VFW/FOURCC";
constexpr auto MATROSKA_CODEC_ID_AUDIO_ACM = "A_MS/ACM";
constexpr auto MATROSKA_CODEC_ID_AUDIO_AC3 = "A_AC3";
constexpr auto MATROSKA_CODEC_ID_SRT = "S_TEXT/UTF8";
constexpr auto MATROSKA_CODEC_ID_AUDIO_AAC = "A_AAC";
constexpr auto MATROSKA_CODEC_ID_AUDIO_PCM_BIG = "A_PCM/INT/BIG";
constexpr auto MATROSKA_CODEC_ID_AUDIO_PCM_LIT = "A_PCM/INT/LIT";
constexpr auto MATROSKA_CODEC_ID_SUBTITLE_PGS = "S_HDMV/PGS";

/* max. depth in the EBML tree structure */
constexpr auto EBML_MAX_DEPTH = 16;

// extern CodecTags ff_mkv_codec_tags[];

class ParsedH264TrackData : public ParsedTrackPrivData
{
   public:
    ParsedH264TrackData(uint8_t* buff, int size);
    ~ParsedH264TrackData() override = default;
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;

   protected:
    int m_nalSize;
    bool m_firstExtract;

    std::vector<std::vector<uint8_t>> m_spsPpsList;
    static void writeNalHeader(uint8_t*& dst);
    size_t getSPSPPSLen() const;
    int writeSPSPPS(uint8_t* dst) const;
    virtual bool spsppsExists(uint8_t* buff, int size);
};

class ParsedH265TrackData : public ParsedH264TrackData
{
   public:
    ParsedH265TrackData(const uint8_t* buff, int size);
    ~ParsedH265TrackData() override = default;

    bool spsppsExists(uint8_t* buff, int size) override;
};

class ParsedH266TrackData : public ParsedH264TrackData
{
   public:
    ParsedH266TrackData(const uint8_t* buff, int size);
    ~ParsedH266TrackData() override = default;

    bool spsppsExists(uint8_t* buff, int size) override;
};

class ParsedAC3TrackData : public ParsedTrackPrivData
{
   public:
    ParsedAC3TrackData(uint8_t* buff, int size);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedAC3TrackData() override = default;

private:
    bool m_firstPacket;
    bool m_shortHeaderMode;
};

class ParsedAACTrackData : public ParsedTrackPrivData
{
   public:
    ParsedAACTrackData(uint8_t* buff, int size);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedAACTrackData() override = default;

private:
    AACCodec m_aacRaw;
};

class ParsedLPCMTrackData : public ParsedTrackPrivData
{
   public:
    ParsedLPCMTrackData(MatroskaTrack* track);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedLPCMTrackData() override = default;

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
    ~ParsedSRTTrackData() override = default;

private:
    int m_packetCnt;
};

class ParsedVC1TrackData : public ParsedTrackPrivData
{
   public:
    ParsedVC1TrackData(uint8_t* buff, int size);
    void extractData(AVPacket* pkt, uint8_t* buff, int size) override;
    ~ParsedVC1TrackData() override = default;

private:
    std::vector<uint8_t> m_seqHeader;
    bool m_firstPacket;
};

class ParsedPGTrackData : public ParsedTrackPrivData
{
   public:
    ParsedPGTrackData() = default;
    ~ParsedPGTrackData() override = default;
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
