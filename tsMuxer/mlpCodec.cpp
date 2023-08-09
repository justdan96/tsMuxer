#include "mlpCodec.h"

#include "bitStream.h"
#include "vod_common.h"

static constexpr int HD_SYNC_WORD = 0xf8726f;

static const uint8_t mlp_channels[32] = {1, 2, 3, 4, 3, 4, 5, 3, 4, 5, 4, 5, 6, 4, 5, 4,
                                         5, 6, 5, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const uint8_t thd_chancount[13] = {
    //  LR    C   LFE  LRs LRvh  LRc LRrs  Cs   Ts  LRsd  LRw  Cvh  LFE2
    2, 1, 1, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1};

// returns 48/96/192/44.1/88.2/176.4 kHz, or 0 on error
int MLPCodec::mlp_samplerate(const int ratebits)
{
    switch (ratebits)
    {
    case 0:
        return 48000;
    case 1:
        return 96000;
    case 2:
        return 192000;
    case 8:
        return 44100;
    case 9:
        return 88200;
    case 10:
        return 176400;
    default:
        return 0;
    }
}

uint64_t MLPCodec::getFrameDuration() const { return INTERNAL_PTS_FREQ * m_samples / m_samplerate; }

static int numChannels6(const int chanmap)
{
    int channels = 0;
    for (int i = 0; i < 5; i++) channels += thd_chancount[i] * ((chanmap >> i) & 1);
    return channels;
}

static int numChannels8(const int chanmap)
{
    int channels = 0;
    for (int i = 0; i < 13; i++) channels += thd_chancount[i] * ((chanmap >> i) & 1);
    return channels;
}

// returns frame position starting with sync_word 0xf8726f
uint8_t* MLPCodec::findFrame(uint8_t* buffer, uint8_t* end)
{
    uint8_t* curBuf = buffer;
    while (curBuf < end)
    {
        if (curBuf[4] == 0xf8 && curBuf[5] == 0x72 && curBuf[6] == 0x6f)
            return curBuf;
        else
            curBuf++;
    }
    return nullptr;
}

int MLPCodec::getFrameSize(uint8_t* buffer) { return ((buffer[0] & 0x0f) << 9) + (buffer[1] << 1); }

bool MLPCodec::isMinorSync(uint8_t* buffer, uint8_t* end) const
{
    /* The first nibble of a frame is a parity check of the 4-byte
       access unit header and all the 2- or 4-byte substream headers. */
    if (m_substreams == 0)
        return false;
    uint8_t parity_bits = 0;
    int p = 0;
    for (int i = -1; i < m_substreams; i++)
    {
        parity_bits ^= buffer[p++];
        parity_bits ^= buffer[p++];
        // dynamic_range_control data if extra_substream_word flag
        if (i < 0 || buffer[p - 2] & 0x80)
        {
            parity_bits ^= buffer[p++];
            parity_bits ^= buffer[p++];
        }
    }
    return ((parity_bits >> 4) ^ (parity_bits & 0xF)) == 0xF;
}

// returns true if TrueHD or MLP frame, false on error
bool MLPCodec::decodeFrame(uint8_t* buffer, uint8_t* end)
{
    if (end - buffer < 21)
        return false;
    BitStreamReader reader{};
    reader.setBuffer(buffer + 4, end);
    if (reader.getBits(24) != HD_SYNC_WORD) /* Sync words */
        return isMinorSync(buffer, end);

    int ratebits = 0;
    const int stream_type = reader.getBits(8);

    if (stream_type == 0xbb)  // MLP
    {
        m_subType = MlpSubType::stMLP;
        reader.skipBits(8);  // group1_bits, group2_bits
        ratebits = reader.getBits(4);
        m_samplerate = mlp_samplerate(ratebits);
        if (m_samplerate == 0)
            return false;
        reader.skipBits(4);  // group2_samplerate
        reader.skipBits(11);
        m_channels = mlp_channels[reader.getBits(5)];
    }
    else if (stream_type == 0xba)  // True-HD
    {
        m_subType = MlpSubType::stTRUEHD;
        ratebits = reader.getBits(4);  // audio_sampling_frequency
        m_samplerate = mlp_samplerate(ratebits);
        if (m_samplerate == 0)
            return false;
        if (reader.getBits(2) > 0)  // 6/8ch_multichannel_type: 0 = standard loudspeaker layout
            return false;
        reader.skipBits(6);  // reserved, 2ch_presentation_channel_modifier, 6ch_presentation_channel_modifier
        const int channel_assign_6 = reader.getBits(5);   // 6ch_presentation_channel_assignment
        reader.skipBits(2);                               // 8ch_presentation_channel_modifier
        const int channel_assign_8 = reader.getBits(13);  // 8ch_presentation_channel_assignment

        if (channel_assign_8 > 0)
        {
            m_channels = numChannels8(channel_assign_8);
            if (m_channels > 8)
                return false;
        }
        else
        {
            m_channels = numChannels6(channel_assign_6);
            if (m_channels > 6)
                return false;
        }
    }
    else
        return false;

    m_samples = 40 << (ratebits & 7);
    if (reader.getBits(16) != 0xB752)  // signature
        return false;
    reader.skipBits(32);                                                            // flags, reserved
    reader.skipBit();                                                               // is_vbr
    m_bitrate = (reader.getBits(15) /* peak_data_rate */ * m_samplerate + 8) >> 4;  // + 8 is for rounding to nearest
    m_substreams = reader.getBits(4);
    return true;
}
