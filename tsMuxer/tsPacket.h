#ifndef TS_PACKET_H_
#define TS_PACKET_H_

#include <memory.h>
#include <types/types.h>

#include <map>

#include "bitStream.h"
#include "vod_common.h"

// H.222 Table 2-45 - Program and program element descriptor
enum class TSDescriptorTag
{
    // Bu-ray specifications
    HDMV = 0x05,          // HDMV registration descriptor
    COPY_CONTROL = 0x88,  // ATSC_CA_descriptor

    // IUT Rec. H.222
    REGISTRATION = 0x05,  // registration_descriptor
    CAS = 0x09,           // CA_descriptor
    LANG = 0x0A,          // ISO_639_language_descriptor
    AAC4 = 0x1C,          // MPEG-4_AAC_audio_descriptor
    AVC = 0x28,           //
    AAC2 = 0x2B,          // MPEG-2_AAC_audio_descriptor
    HEVC = 0x38,
    VVC = 0x39,
    DTS = 0x73,
    LPCM = 0x80,
    AC3 = 0x81,
    EAC3 = 0xCC
};

static constexpr int SYSTEM_START_CODE = 0xb9;

static constexpr int DEFAULT_PCR_PID = 4097;
static constexpr int DEFAULT_PMT_PID = 256;

// IUT Rec. H.222 Table 2-22 - Stream_id assignments
enum PesStreamId
{
    PES_INT_SUB_ID = 0x20,
    PES_INT_AC3_ID = 0x80,
    PES_INT_DTS_ID = 0x8a,
    PES_PROGRAM_STREAM_MAP = 0xbc,
    PES_PRIVATE_DATA1 = 0xbd,
    PADDING_STREAM = 0xbe,
    PES_PRIVATE_DATA2 = 0xbf,
    LPCM_ID = 0xa0,
    PES_AUDIO_ID = 0xc0,
    PES_VIDEO_ID = 0xe0,
    PES_HEVC_ID = 0xe1,
    PES_VVC_ID = 0xe1,
    PES_VC1_ID = 0xfd,
    PROGRAM_STREAM_DIRECTORY = 0xff
};

enum class StreamType
{
    // Blu-ray Disk Specifiction Table 5-22 - stream_coding_type
    VIDEO_MPEG2 = 0x02,             // MPEG-2 video stream for Primary / Secondary video
    VIDEO_H264 = 0x1b,              // MPEG-4 AVC video stream for Primary / Secondary video
    VIDEO_MVC = 0x20,               // forbidden (Used for ProgramInfo_SS() in 5.4.7.6)
    VIDEO_H265 = 0x24,              // MPEG-H HEVC video stream for Primary / Secondary video
    VIDEO_VC1 = 0xea,               // SMPTE VC-1 video stream for Primary / Secondary video
    AUDIO_LPCM = 0x80,              // HDMV LPCM audio stream for Primary audio
    AUDIO_AC3 = 0x81,               // Dolby Digital (AC-3) audio stream for Primary audio
    AUDIO_DTS = 0x82,               // DTS audio stream for Primary audio
    AUDIO_TRUE_HD = 0x83,           // Dolby Lossless audio stream for Primary audio
    AUDIO_EAC3 = 0x84,              // Dolby Digital Plus audio stream for Primary audio
    AUDIO_DTS_HD = 0x85,            // DTS-HD audio stream except XLL for Primary audio
    AUDIO_DTS_HD_MA = 0x86,         // DTS-HD audio stream XLL for Primary audio
    AUDIO_DRA = 0x87,               // DRA audio stream for Primary audio
    AUDIO_DRA_EXT = 0x88,           // DRA Extension audio stream for Primary audio
    AUDIO_EAC3_SECONDARY = 0xA1,    // Dolby Digital Plus audio stream for Secondary audio
    AUDIO_DTS_HD_SECONDARY = 0xA2,  // DTS-HD audio stream for Secondary audio stream
    SUB_PGS = 0x90,                 // Presentation Graphics stream
    SUB_IGS = 0x91,                 // Interactive Graphics stream
    SUB_TGS = 0x92,                 // Text subtitle stream

    // IUT-T REC. H.222 | ISO/IEC 13818-1 Table 2-34 - Stream type assignments
    RESERVED = 0x00,         // ITU-T | ISO/IEC Reserved
    VIDEO_MPEG1 = 0x01,      // ISO/IEC 11172-2 Video
    AUDIO_MPEG1 = 0x03,      // ISO/IEC 11172-3 Audio
    AUDIO_MPEG2 = 0x04,      // ISO/IEC 13818-3 Audio
    PRIVATE_SECTION = 0x05,  // private_sections
    PRIVATE_DATA = 0x06,     // PES packets containing private data
    AUDIO_AAC = 0x0f,        // ISO/IEC 13818-7 Audio with ADTS transport syntax
    VIDEO_MPEG4 = 0x10,      // ISO/IEC 14496-2 Visual
    AUDIO_AAC_RAW = 0x11,    // ISO/IEC 14496-3 Audio with the LATM transport syntax
    VIDEO_SUB_H265 = 0x25,   // HEVC temporal video subset, profile as per of H265 Annex A
    AUDIO_MHAS_MAIN = 0x2d,  // ISO/IEC 23008-3 Audio with MHAS transport syntax _ main stream
    AUDIO_MHAS_AUX = 0x2e,   // ISO/IEC 23008-3 Audio with MHAS transport syntax - auxiliary stream
    VIDEO_H266 = 0x33,       // VVC video stream, profile as per H266 Annex A

    // ATSC
    VIDEO_DCII_ATSC = 0x80,  // DigiCipher II video, identical to ITU-T Rec. H.262
    AUDIO_AC3_ATSC = 0x81,   // E-AC-3 A/52:2018
    SUB_SCTE27_ATSC = 0x82,  // SCTE-27 Subtitling
    DATA_ISOCH_ATSC = 0x83,  // SCTE-19 Isochronous data | Reserved
    AUDIO_EAC3_ATSC = 0x87,  // E-AC-3 A/52:2018
    AUDIO_DTS_ATSC = 0x88,   // E-AC-3 A / 107(ATSC 2.0)

    // DVB
    SUB_DVB = 0x06,  // DVB Subtitles
};

struct AdaptiveField
{
    static constexpr unsigned ADAPTIVE_FIELD_LEN = 2;

    unsigned int length : 8;

    unsigned int afExtension : 1;  // Adaptation field extension flag
    unsigned int transportPrivateData : 1;
    unsigned int splicingPoint : 1;
    unsigned int opcrExist : 1;
    unsigned int pcrExist : 1;
    unsigned int priorityIndicator : 1;
    unsigned int randomAccessIndicator : 1;
    unsigned int discontinuityIndicator : 1;

    unsigned getPCR32()
    {
        const auto pcr = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + ADAPTIVE_FIELD_LEN);
        return my_ntohl(*pcr);
        // return my_ntohl(*pcr) * 0.95;
    }

    uint64_t getPCR33()
    {
        const auto pcr = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + ADAPTIVE_FIELD_LEN);
        const auto pcrLo = reinterpret_cast<uint8_t*>(this) + ADAPTIVE_FIELD_LEN + sizeof(uint32_t);
        return (static_cast<uint64_t>(my_ntohl(*pcr)) << 1) + (*pcrLo >> 7);
    }

    void setPCR33(const uint64_t value)
    {
        const auto pcr = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + ADAPTIVE_FIELD_LEN);
        *pcr = my_htonl(static_cast<uint32_t>(value >> 1));
        const auto pcrLo = reinterpret_cast<uint8_t*>(this) + ADAPTIVE_FIELD_LEN + sizeof(uint32_t);
        pcrLo[0] = ((static_cast<uint8_t>(value) & 1) << 7) + 0x7e;
        pcrLo[1] = 0;  // 41-42 bits of pcr
    }
};

struct TSPacket
{
    // static const unsigned TS_FRAME_SIZE = 188;
    static constexpr unsigned TS_FRAME_SYNC_BYTE = 0x47;
    static constexpr unsigned TS_HEADER_SIZE = 4;

    static constexpr unsigned DATA_EXIST_BIT_VAL = 0x10000000;
    static constexpr unsigned PCR_BIT_VAL = 0x1000;

    unsigned int syncByte : 8;
    unsigned int PIDHi : 5;
    unsigned int priority : 1;
    unsigned int payloadStart : 1;
    unsigned int tei : 1;  // Transport Error Indicator
    unsigned int PIDLow : 8;

    /*
    unsigned int sc: 2;         // scrambling control
    unsigned int afExists: 1;   // adaptive field exist
    unsigned int dataExists: 1;
    unsigned int counter: 4;    //Continuity counter
    */

    unsigned int counter : 4;  // Continuity counter
    unsigned int dataExists : 1;
    unsigned int afExists : 1;  // adaptive field exist
    unsigned int sc : 2;        // scrambling control

    uint16_t getPID() const { return static_cast<uint16_t>(PIDHi << 8) + PIDLow; }

    void setPID(const uint16_t pid)
    {
        PIDHi = pid >> 8;
        PIDLow = pid & 0xff;
    }

    unsigned getHeaderSize() const { return TS_HEADER_SIZE + (afExists ? adaptiveField.length + 1 : 0); }

    static uint32_t getPCRDif32(const uint32_t nextPCR, const uint32_t curPCR)
    {
        if (nextPCR >= curPCR)
            return nextPCR - curPCR;
        return nextPCR + (UINT_MAX - curPCR);
    }

    static int64_t getPCRDif33(const int64_t nextPCR, const int64_t curPCR)
    {
        if (nextPCR >= curPCR)
            return nextPCR - curPCR;
        if (nextPCR < 0x40000000LL && curPCR > 0x1c0000000LL)
            return nextPCR + (0x1ffffffffLL - curPCR) + 1;

        return nextPCR - curPCR;
    }

    AdaptiveField adaptiveField;
};

class AbstractStreamReader;

struct BluRayCoarseInfo
{
    uint32_t m_coarsePts;
    uint32_t m_fineRefID;
    uint32_t m_pktCnt;
    BluRayCoarseInfo(const uint32_t coarsePts, const uint32_t fineRefID, const uint32_t pktCnt)
        : m_coarsePts(coarsePts), m_fineRefID(fineRefID), m_pktCnt(pktCnt)
    {
    }
};

struct PMTIndexData
{
    uint32_t m_pktCnt;
    int64_t m_frameLen;
    PMTIndexData(const uint32_t pktCnt, const int64_t frameLen) : m_pktCnt(pktCnt), m_frameLen(frameLen) {}
};

typedef std::map<uint64_t, PMTIndexData> PMTIndex;

struct PMTStreamInfo
{
    PMTStreamInfo()
        : m_streamType(),
          m_pid(0),
          m_esInfoLen(0),
          m_pmtPID(-1),
          m_esInfoData(),
          m_lang(),
          isSecondary(false),
          m_codecReader()
    {
    }

    PMTStreamInfo(const StreamType streamType, const int pid, const uint8_t* esInfoData, const int esInfoLen,
                  AbstractStreamReader* codecReader, const std::string& lang, const bool secondary)
    {
        m_streamType = streamType;
        m_pid = pid;
        memcpy(m_esInfoData, esInfoData, esInfoLen);
        m_esInfoLen = esInfoLen;
        m_codecReader = codecReader;
        memset(m_lang, 0, sizeof(m_lang));
        memcpy(m_lang, lang.c_str(), lang.size() < 3 ? lang.size() : 3);
        m_pmtPID = -1;
        isSecondary = secondary;
    }
    virtual ~PMTStreamInfo() = default;

    StreamType m_streamType;
    int m_pid;
    int m_esInfoLen;
    int m_pmtPID;
    uint8_t m_esInfoData[128];
    char m_lang[4];
    bool isSecondary;

    // ---------------------
    std::vector<PMTIndex> m_index;  // blu-ray seek index. key=number of tsFrame. value=information about key frame
    AbstractStreamReader* m_codecReader;
};

typedef std::map<int, PMTStreamInfo> PIDListMap;

struct TS_program_map_section
{
    int video_pid;
    int video_type;
    int audio_pid;
    int audio_type;
    int sub_pid;
    int pcr_pid;

    int casPID;
    int casID;

    int program_number;
    // std::vector<PMTStreamInfo> pidList;
    PIDListMap pidList;
    TS_program_map_section();
    bool deserialize(uint8_t* buffer, int buf_size);
    static bool isFullBuff(uint8_t* buffer, int buf_size);
    uint32_t serialize(uint8_t* buffer, int max_buf_size, bool blurayMode, bool hdmvDescriptors);

   private:
    static void extractDescriptors(uint8_t* curPos, int es_info_len, PMTStreamInfo& pmtInfo);
    void extractPMTDescriptors(uint8_t* curPos, int es_info_len);
    // uint32_t tmpAvCrc[257];
};

struct TS_program_association_section
{
    int transport_stream_id;
    int m_nitPID;
    std::map<int, int> pmtPids;  // program pid, program number

    TS_program_association_section();
    bool deserialize(uint8_t* buffer, int buf_size);
    uint32_t serialize(uint8_t* buffer, int buf_size);

   private:
    // uint32_t tmpAvCrc[257];
};

struct PS_stream_pack
{
    bool deserialize(uint8_t* buffer, int buf_size);
    uint64_t m_pts;
    uint32_t m_pts_ext;
    uint32_t m_program_mux_rate;
    uint32_t m_pack_stuffing_length;
};

struct M2TSStreamInfo
{
    M2TSStreamInfo()
        : streamPID(0),
          stream_coding_type(),
          video_format(0),
          frame_rate_index(3),
          number_of_offset_sequences(0),
          width(0),
          height(0),
          HDR(0),
          aspect_ratio_index(0),
          audio_presentation_type(0),
          sampling_frequency_index(0),
          character_code(0),
          language_code(),
          isSecondary(false)
    {
    }
    M2TSStreamInfo(const PMTStreamInfo& pmtStreamInfo);
    M2TSStreamInfo(const M2TSStreamInfo& other);

    int streamPID;
    StreamType stream_coding_type;  // ts type
    int video_format;
    int frame_rate_index;
    int number_of_offset_sequences;
    int width;
    int height;
    int HDR;
    int aspect_ratio_index;
    int audio_presentation_type;
    int sampling_frequency_index;
    int8_t character_code;
    char language_code[4];
    bool isSecondary;
    std::vector<PMTIndex> m_index;

    static void blurayStreamParams(double fps, bool interlaced, int width, int height, int ar, int* video_format,
                                   int* frame_rate_index, int* aspect_ratio_index);
};

struct CLPIStreamInfo : public M2TSStreamInfo
{
    CLPIStreamInfo(const PMTStreamInfo& pmtStreamInfo) : M2TSStreamInfo(pmtStreamInfo)
    {
        memset(country_code, 0, sizeof(country_code));
        memset(copyright_holder, 0, sizeof(copyright_holder));
        memset(recording_year, 0, sizeof(recording_year));
        memset(recording_number, 0, sizeof(recording_number));
    }
    // int stream_coding_type;
    // int video_format;
    // int frame_rate_index;
    // int aspect_ratio_index;
    // bool cc_flag;
    // int audio_presentation_type;
    // int sampling_frequency;

    // char language_code[4];
    char country_code[3];
    char copyright_holder[4];
    char recording_year[3];
    char recording_number[6];

    void ISRC(BitStreamReader& reader);
    void parseStreamCodingInfo(BitStreamReader& reader);
    static void readString(char* dest, BitStreamReader& reader, const int size)
    {
        for (int i = 0; i < size; i++) dest[i] = reader.getBits(8);
        dest[size] = 0;
    }
    static void writeString(const char* dest, BitStreamWriter& writer, const int size)
    {
        for (int i = 0; i < size; i++) writer.putBits(8, dest[i]);
    }
    CLPIStreamInfo()
    {
        memset(language_code, 0, sizeof(language_code));
        memset(country_code, 0, sizeof(country_code));
        memset(copyright_holder, 0, sizeof(copyright_holder));
        memset(recording_year, 0, sizeof(recording_year));
        memset(recording_number, 0, sizeof(recording_number));
    }
    static void composeISRC(BitStreamWriter& writer);
    void composeStreamCodingInfo(BitStreamWriter& writer) const;

   private:
};

struct CLPIProgramInfo
{
    uint32_t SPN_program_sequence_start;
    uint16_t program_map_PID;
    uint8_t number_of_streams_in_ps;
};

class CLPIParser
{
   public:
    CLPIParser()
        : type_indicator(),
          version_number(),
          clip_stream_type(0),
          application_type(0),
          is_ATC_delta(false),
          TS_recording_rate(0),
          number_of_source_packets(0),
          format_identifier(),
          presentation_start_time(0),
          presentation_end_time(0),
          m_clpiNum(0),
          isDependStream(false)
    {
    }

    void parse(uint8_t* buffer, int64_t len);
    bool parse(const char* fileName);
    int compose(uint8_t* buffer, int bufferSize);

    std::vector<CLPIProgramInfo> m_programInfo;
    std::vector<CLPIProgramInfo> m_programInfoMVC;

    char type_indicator[5];
    char version_number[5];
    std::map<int, CLPIStreamInfo> m_streamInfo;
    std::map<int, CLPIStreamInfo> m_streamInfoMVC;

    uint8_t clip_stream_type;
    uint8_t application_type;
    bool is_ATC_delta;
    uint32_t TS_recording_rate;
    uint32_t number_of_source_packets;
    char format_identifier[5];
    uint32_t presentation_start_time;
    uint32_t presentation_end_time;
    size_t m_clpiNum;
    std::vector<uint32_t> SPN_extent_start;
    std::vector<int32_t> interleaveInfo;
    bool isDependStream;

   private:
    static void HDMV_LPCM_down_mix_coefficient(uint8_t* buffer, int dataLength);
    void Extent_Start_Point(uint8_t* buffer, int dataLength);
    void ProgramInfo_SS(uint8_t* buffer, int dataLength);
    static void CPI_SS(uint8_t* buffer, int dataLength);

   private:
    static void parseProgramInfo(uint8_t* buffer, const uint8_t* end, std::vector<CLPIProgramInfo>& programInfoMap,
                                 std::map<int, CLPIStreamInfo>& streamInfoMap);
    void parseSequenceInfo(uint8_t* buffer, const uint8_t* end);
    static void parseCPI(uint8_t* buffer, const uint8_t* end);
    static void EP_map(BitStreamReader& reader);
    static void parseClipMark(uint8_t* buffer, const uint8_t* end);
    void parseClipInfo(BitStreamReader& reader);
    void parseExtensionData(uint8_t* buffer, const uint8_t* end);
    void TS_type_info_block(BitStreamReader& reader);
    void composeProgramInfo(BitStreamWriter& writer, bool isSsExt);
    static void composeTS_type_info_block(BitStreamWriter& writer);
    void composeClipInfo(BitStreamWriter& writer) const;
    void composeSequenceInfo(BitStreamWriter& writer) const;
    void composeCPI(BitStreamWriter& writer, bool isCPIExt);
    static void composeClipMark(BitStreamWriter& writer);
    void composeExtentInfo(BitStreamWriter& writer);
    void composeExtentStartPoint(BitStreamWriter& writer) const;
    void composeEP_map(BitStreamWriter& writer, bool isSSExt);
    std::vector<BluRayCoarseInfo> buildCoarseInfo(M2TSStreamInfo& streamInfo) const;
    void composeEP_map_for_one_stream_PID(BitStreamWriter& writer, M2TSStreamInfo& streamInfo) const;
};

struct MPLSStreamInfo : public M2TSStreamInfo
{
    MPLSStreamInfo();
    MPLSStreamInfo(const PMTStreamInfo& pmtStreamInfo);
    MPLSStreamInfo(const MPLSStreamInfo& other);
    ~MPLSStreamInfo();

    void parseStreamAttributes(BitStreamReader& reader);
    void parseStreamEntry(BitStreamReader& reader);
    void composeStreamAttributes(BitStreamWriter& writer);
    void composeStreamEntry(BitStreamWriter& writer, size_t entryNum, int subPathID = 0) const;
    void composePGS_SS_StreamEntry(BitStreamWriter& writer, size_t entryNum) const;

   public:
    int type;
    uint8_t offsetId;
    bool isSSPG;
    int SS_PG_offset_sequence_id;
    MPLSStreamInfo* leftEye;  // used for stereo PG
    MPLSStreamInfo* rightEye;
    PIPParams pipParams;
};

struct MPLSPlayItem
{
    uint32_t IN_time = 0;
    uint32_t OUT_time = 0;
    std::string fileName;
    int connection_condition = 0;
};

struct PlayListMark
{
    int m_playItemID;
    uint32_t m_markTime;
    PlayListMark(const int playItemID, const uint32_t markTime) : m_playItemID(playItemID), m_markTime(markTime) {}
};

struct ExtDataBlockInfo
{
    ExtDataBlockInfo(const uint8_t* a_data, const int a_dataLen, const int a_id1, const int a_id2) : id1(a_id1), id2(a_id2)
    {
        data.resize(a_dataLen);
        if (!data.empty())
            memcpy(&data[0], a_data, data.size());
    }
    std::vector<uint8_t> data;
    int id1;
    int id2;
};

struct MPLSParser
{
    MPLSParser();

    bool parse(const char* fileName);
    void parse(uint8_t* buffer, int len);
    int compose(uint8_t* buffer, int bufferSize, DiskType dt);

    MPLSStreamInfo getStreamByPID(int pid) const;
    std::vector<MPLSStreamInfo> getPgStreams() const;

    int PlayList_playback_type;
    int playback_count;
    int number_of_SubPaths;
    bool is_multi_angle;
    int ref_to_STC_id;
    bool PlayItem_random_access_flag;
    int number_of_angles;
    bool is_different_audios;
    bool is_seamless_angle_change;

    int m_chapterLen;

    uint32_t IN_time;
    uint32_t OUT_time;
    std::vector<MPLSPlayItem> m_playItems;

    std::vector<MPLSStreamInfo> m_streamInfo;
    std::vector<MPLSStreamInfo> m_streamInfoMVC;
    std::vector<PlayListMark> m_marks;
    int m_m2tsOffset;
    bool isDependStreamExist;
    bool mvc_base_view_r;
    int subPath_type;

    int number_of_primary_video_stream_entries;
    int number_of_primary_audio_stream_entries;
    int number_of_PG_textST_stream_entries;
    int number_of_IG_stream_entries;
    int number_of_secondary_audio_stream_entries;
    int number_of_secondary_video_stream_entries;
    int number_of_PiP_PG_textST_stream_entries_plus;
    int number_of_DolbyVision_video_stream_entries;

    std::vector<std::string> m_mvcFiles;

   private:
    void composeSubPlayItem(BitStreamWriter& writer, size_t playItemNum, size_t subPathNum,
                            const std::vector<PMTIndex>& pmtIndexList) const;
    void composeSubPath(BitStreamWriter& writer, size_t subPathNum, const std::vector<PMTIndex>& pmtIndexList,
                        int type) const;
    int composePip_metadata(uint8_t* buffer, int bufferSize, const std::vector<PMTIndex>& pmtIndexList) const;
    static void composeExtensionData(BitStreamWriter& writer, const std::vector<ExtDataBlockInfo>& extDataBlockInfo);
    void parseExtensionData(uint8_t* data, const uint8_t* dataEnd);
    void SubPath_extension(BitStreamWriter& writer);
    void parseStnTableSS(uint8_t* data, int dataLength);

    void AppInfoPlayList(BitStreamReader& reader);
    void composeAppInfoPlayList(BitStreamWriter& writer) const;
    static void UO_mask_table(BitStreamReader& reader);
    void parsePlayList(uint8_t* buffer, int len);
    void parsePlayItem(BitStreamReader& reader, int PlayItem_id);
    void parsePlayListMark(uint8_t* buffer, int len);
    void STN_table(BitStreamReader& reader, int PlayItem_id);

    void composePlayList(BitStreamWriter& writer);
    // void composePlayItem(BitStreamWriter& writer);
    void composePlayItem(BitStreamWriter& writer, size_t playItemNum, const std::vector<PMTIndex>& pmtIndexList);
    void composePlayListMark(BitStreamWriter& writer);
    void composeSTN_table(BitStreamWriter& writer, size_t PlayItem_id, bool isSSEx);
    int composeSTN_tableSS(uint8_t* buffer, int bufferSize);
    int composeSubPathEntryExtension(uint8_t* buffer, int bufferSize);
    static int composeUHD_metadata(uint8_t* buffer, int bufferSize);
    MPLSStreamInfo& getMainStream();
    MPLSStreamInfo& getMVCDependStream();
    static int calcPlayItemID(const MPLSStreamInfo& streamInfo, uint32_t pts);
    int pgIndexToFullIndex(int value) const;
    void parseSubPathEntryExtension(uint8_t* data, int dataLen);
};

#endif
