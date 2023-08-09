
#include "mp3Codec.h"

const uint16_t ff_mpa_bitrate_tab[2][3][15] = {{{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
                                                {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
                                                {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}},
                                               {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
                                                {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
                                                {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}}};

const uint16_t ff_mpa_freq_tab[3] = {44100, 48000, 32000};

// const static int MPA_STEREO = 0;
// const static int MPA_JSTEREO = 1;
// const static int MPA_DUAL = 2;
const static int MPA_MONO = 3;

uint8_t* MP3Codec::mp3FindFrame(uint8_t* buff, uint8_t* end)
{
    if (buff == nullptr)
        return nullptr;
    // header
    for (uint8_t* cur = buff; cur < end - 4; cur++)
    {
        uint32_t header = my_ntohl(*((uint32_t*)cur));
        if ((header & 0xffe00000) != 0xffe00000)
            continue;
        // layer check
        if ((header & (3 << 17)) == 0)
            continue;
        // bit rate
        if ((header & (0xf << 12)) == 0xf << 12)
            continue;
        // frequency
        if ((header & (3 << 10)) == 3 << 10)
            continue;
        return cur;
    }
    return nullptr;
}

int MP3Codec::mp3DecodeFrame(uint8_t* buff, uint8_t* end)
{
    // int sample_rate, frame_size, mpeg25, padding;
    // int sample_rate_index, bitrate_index;
    int mpeg25, lsf;
    if (end - buff < 4)
        return 0;
    uint32_t header = my_ntohl(*((uint32_t*)buff));
    if (header & (1 << 20))
    {
        lsf = (header & (1 << 19)) ? 0 : 1;
        mpeg25 = 0;
    }
    else
    {
        lsf = 1;
        mpeg25 = 1;
    }

    m_layer = 4 - ((header >> 17) & 3);
    if (m_layer == 4)
        return 0;
    /* extract frequency */
    m_sample_rate_index = (header >> 10) & 3;
    if (m_sample_rate_index == 3)
        return 0;  // invalid sample rate
    m_sample_rate = ff_mpa_freq_tab[m_sample_rate_index] >> (lsf + mpeg25);
    m_sample_rate_index += 3 * (lsf + mpeg25);
    // error_protection = ((header >> 16) & 1) ^ 1;

    m_bitrate_index = (header >> 12) & 0xf;
    if (m_bitrate_index == 15)
        return 0;  // invalid bitrate index
    int padding = (header >> 9) & 1;
    // extension = (header >> 8) & 1;
    m_mode = (header >> 6) & 3;
    m_mode_ext = (header >> 4) & 3;
    // copyright = (header >> 3) & 1;
    // original = (header >> 2) & 1;
    // emphasis = header & 3;

    if (m_mode == MPA_MONO)
        m_nb_channels = 1;
    else
        m_nb_channels = 2;

    if (m_bitrate_index != 0)
    {
        m_frame_size = ff_mpa_bitrate_tab[lsf][m_layer - 1][m_bitrate_index];
        m_bit_rate = m_frame_size * 1000;
        switch (m_layer)
        {
        case 1:
            m_samples = 384;
            m_frame_size = (m_frame_size * 12000) / m_sample_rate;
            m_frame_size = (m_frame_size + padding) * 4;
            break;
        case 2:
            m_samples = 1152;
            m_frame_size = (m_frame_size * 144000) / m_sample_rate;
            m_frame_size += padding;
            break;
        default:
        case 3:
            m_samples = 1152;
            m_frame_size = (m_frame_size * 144000) / (m_sample_rate << lsf);
            m_frame_size += padding;
            break;
        }
    }
    else
    {
        /* if no frame size computed, signal it */
        return 0;
    }
    return m_frame_size;
}
