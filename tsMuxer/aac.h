#ifndef AAC_CODEC_H_
#define AAC_CODEC_H_

#include <types/types.h>

static constexpr int AAC_HEADER_LEN = 7;

class AACCodec
{
   public:
    static const int aac_sample_rates[16];
    static const int aac_channels[8];
    AACCodec()
        : m_id(0),
          m_layer(0),
          m_channels(0),
          m_sample_rate(48000),
          m_samples(0),
          m_bit_rate(0),
          m_sample_rates_index(0),
          m_channels_index(0),
          m_profile(0),
          m_rdb(0)
    {
    }
    static uint8_t* findAacFrame(uint8_t* buffer, const uint8_t* end);
    static int getFrameSize(const uint8_t* buffer);
    bool decodeFrame(uint8_t* buffer, uint8_t* end);
    void buildADTSHeader(uint8_t* buffer, unsigned frameSize);
    void setSampleRate(const int value) { m_sample_rate = value; }
    void readConfig(uint8_t* buff, int size);

   public:
    int m_id;
    int m_layer;
    unsigned m_channels;
    unsigned m_sample_rate;
    int m_samples;
    int m_bit_rate;
    int m_sample_rates_index;
    int m_channels_index;
    int m_profile;
    int m_rdb;  // ch, sr;
};

#endif
