#ifndef AC3_CODEC_H_
#define AC3_CODEC_H_

#include "avPacket.h"
#include "mlpCodec.h"

struct CodecInfo;

class AC3Codec
{
   public:
    static constexpr int AC3_HEADER_SIZE = 7;

    enum class AC3State
    {
        stateDecodeAC3,
        stateDecodeAC3Plus,
        stateDecodeTrueHDFirst,
        stateDecodeTrueHD
    };

    enum class AC3ParseError
    {
        NO_ERROR = 0,
        SYNC = -1,
        BSID = -2,
        SAMPLE_RATE = -3,
        FRAME_SIZE = -4,
        CRC2 = -5,
        NOT_ENOUGH_BUFFER = -10,
    };

    AC3Codec()
        : m_fscod(0),
          m_frmsizecod(0),
          m_bsmod(0),
          m_acmod(0),
          m_halfratecod(0),
          m_sample_rate(0),
          m_frame_size(0),
          m_samples(0)
    {
        m_downconvertToAC3 = m_true_hd_mode = false;
        m_state = AC3State::stateDecodeAC3;
        m_waitMoreData = false;
        AC3Codec::setTestMode(false);
        m_frameDuration = 0;
        m_bit_rateExt = 0;
        m_bit_rate = 0;
        m_channels = 0;
        m_lfeon = 0;
        m_extChannelsExists = false;
        m_bsid = m_bsidBase = 0;
        m_strmtyp = 0;
        m_dsurmod = 0;
        m_mixinfoexists = false;
    }

    virtual ~AC3Codec() {}

    virtual int getHeaderLen() { return AC3_HEADER_SIZE; }
    bool isEAC3() const { return m_bsid > 10; }
    bool isAC3() const { return m_bsidBase > 0; }
    void setDownconvertToAC3(const bool value) { m_downconvertToAC3 = value; }
    bool getDownconvertToAC3() const { return m_downconvertToAC3; }
    bool isTrueHD() const { return m_true_hd_mode; }
    virtual void setTestMode(const bool value) { m_testMode = value; }
    bool getTestMode() const { return m_testMode; }

   protected:
    int decodeFrame(uint8_t* buf, uint8_t* end, int& skipBytes);
    static uint8_t* findFrame(uint8_t* buffer, const uint8_t* end);
    uint64_t getFrameDuration() const;
    virtual const CodecInfo& getCodecInfo();
    virtual const std::string getStreamInfo();

    AC3State m_state;
    bool m_waitMoreData;
    bool m_downconvertToAC3;
    bool m_true_hd_mode;
    int m_fscod;
    int m_frmsizecod;
    uint8_t m_bsid;
    uint8_t m_bsidBase;
    int m_strmtyp;
    int m_bsmod;
    int m_acmod;
    int m_dsurmod;
    int m_lfeon;
    uint8_t m_halfratecod;
    int m_sample_rate;
    int m_bit_rate;
    int m_channels;
    int m_frame_size;
    bool m_mixinfoexists;

    MLPCodec mlp;

    int m_samples;

    uint32_t m_bit_rateExt;
    bool m_extChannelsExists;

    static bool crc32(const uint8_t* buf, int length);
    AC3ParseError parseHeader(uint8_t* buf, uint8_t* end);

    AC3ParseError testParseHeader(uint8_t* buf, uint8_t* end) const;
    bool testDecodeTestFrame(uint8_t* buf, uint8_t* end) const;

    bool m_testMode;
    int64_t m_frameDuration;
};

#endif
