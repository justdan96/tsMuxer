#ifndef __MP3_CODEC_H
#define __MP3_CODEC_H

#include <types/types.h>

class MP3Codec
{
   public:
    uint8_t* mp3FindFrame(uint8_t* buff, uint8_t* end);
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
