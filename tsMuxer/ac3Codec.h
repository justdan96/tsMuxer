#ifndef __AC3_CODEC_H
#define __AC3_CODEC_H

#include "avPacket.h"

struct CodecInfo;

typedef struct MLPHeaderInfo
{
    MLPHeaderInfo() : subType(stUnknown), channels(0) {}
    enum MlpSubType
    {
        stUnknown,
        stTRUEHD,
        stMLP
    };
    int stream_type;  ///< 0xBB for MLP, 0xBA for TrueHD

    int group1_bits;  ///< The bit depth of the first substream
    int group2_bits;  ///< Bit depth of the second substream (MLP only)

    int group1_samplerate;  ///< Sample rate of first substream
    int group2_samplerate;  ///< Sample rate of second substream (MLP only)

    int channels_mlp;          ///< Channel arrangement for MLP streams
    int channels_thd_stream1;  ///< Channel arrangement for substream 1 of TrueHD streams (5.1)
    int channels_thd_stream2;  ///< Channel arrangement for substream 2 of TrueHD streams (7.1)

    int access_unit_size;       ///< Number of samples per coded frame
    int access_unit_size_pow2;  ///< Next power of two above number of samples per frame

    int is_vbr;        ///< Stream is VBR instead of CBR
    int peak_bitrate;  ///< Peak bitrate for VBR, actual bitrate (==peak) for CBR

    int num_substreams;  ///< Number of substreams within stream

    int channels;
    uint64_t frame_duration_nano;
    MlpSubType subType;
} MLPHeaderInfo;

class AC3Codec
{
   public:
    static const int AC3_HEADER_SIZE = 7;

    enum AC3State
    {
        stateDecodeAC3,
        stateDecodeAC3Plus,
        stateDecodeTrueHDFirst,
        stateDecodeTrueHD
    };
    AC3Codec()
    {
        m_downconvertToAC3 = m_true_hd_mode = false;
        m_state = stateDecodeAC3;
        m_waitMoreData = false;
        setTestMode(false);
        m_frameDuration = false;
        m_bit_rateExt = 0;
        m_bit_rate = 0;
        m_channels = 0;
        m_lfeon = 0;
        m_extChannelsExists = false;
        m_bsid = m_bsidBase = 0;
        m_dsurmod = 0;
        m_mixinfoexists = false;
    };
    int getHeaderLen() { return AC3_HEADER_SIZE; }
    inline bool isEAC3() { return m_bsid > 10; }
    inline bool isAC3() { return m_bsidBase > 0; }
    void setDownconvertToAC3(bool value) { m_downconvertToAC3 = value; }
    bool getDownconvertToAC3() { return m_downconvertToAC3; }
    bool isTrueHD() { return m_true_hd_mode; }
    void setTestMode(bool value) { m_testMode = value; }
    bool getTestMode() { return m_testMode; }

   protected:
    int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes);
    uint8_t* findFrame(uint8_t* buff, uint8_t* end);
    double getFrameDurationNano();
    const CodecInfo& getCodecInfo();
    const std::string getStreamInfo();

   protected:
    AC3State m_state;
    bool m_waitMoreData;
    bool m_downconvertToAC3;
    bool m_true_hd_mode;
    bool m_useNewStyleAudioPES;
    const static CodecInfo m_codecInfo;
    const static unsigned DTS_HEADER_SIZE = 14;
    uint16_t m_sync_word;
    uint16_t m_crc1;
    uint8_t m_fscod;
    int m_frmsizecod;
    uint8_t m_bsid;
    uint8_t m_bsidBase;
    uint8_t m_bsmod;
    uint8_t m_acmod;
    uint8_t m_cmixlev;
    uint8_t m_surmixlev;
    uint8_t m_dsurmod;
    uint8_t m_lfeon;
    uint8_t m_halfratecod;
    int m_sample_rate;
    uint32_t m_bit_rate;
    uint8_t m_channels;
    uint16_t m_frame_size;
    bool m_mixinfoexists;

    MLPHeaderInfo mh;

    int m_fscod2;
    int m_samples;

    uint32_t m_bit_rateExt;
    bool m_extChannelsExists;

    int parseHeader(uint8_t* buf, uint8_t* end);
    int writeAC3Descriptor(uint8_t* dstBuff);
    int writeEAC3Descriptor(uint8_t* dstBuff);

    int testParseHeader(uint8_t* buf, uint8_t* end);
    bool testDecodeTestFrame(uint8_t* buf, uint8_t* end);
    bool findMajorSync(uint8_t* buffer, uint8_t* end);
    bool decodeDtsHdFrame(uint8_t* buffer, uint8_t* end);

   protected:
    bool m_testMode;
    double m_frameDuration;
};

#endif
