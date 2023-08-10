#ifndef MP3_CODEC_H_
#define MP3_CODEC_H_

#include <types/types.h>

class MP3Codec
{
   public:
    MP3Codec()
        : m_frame_size(0),
          m_nb_channels(0),
          m_mode_ext(0),
          m_mode(0),
          m_bitrate_index(0),
          m_sample_rate_index(0),
          m_sample_rate(0),
          m_layer(0),
          m_samples(0),
          m_bit_rate(0)
    {
    }

    static uint8_t* mp3FindFrame(uint8_t* buff, uint8_t* end);
    int mp3DecodeFrame(uint8_t* buff, uint8_t* end);

   protected:
    int m_frame_size;
    int m_nb_channels;
    int m_mode_ext;
    int m_mode;
    int m_bitrate_index;
    int m_sample_rate_index;
    int m_sample_rate;
    int m_layer;
    int m_samples;
    int m_bit_rate;
};

#endif
