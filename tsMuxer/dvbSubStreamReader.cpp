#include "dvbSubStreamReader.h"

static constexpr uint64_t TS_FREQ_TO_INT_FREQ_COEFF = INTERNAL_PTS_FREQ / PCR_FREQUENCY;
static constexpr int BAD_FRAME = -1;

int DVBSubStreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) { return 0; }

uint8_t* DVBSubStreamReader::findFrame(uint8_t* buff, uint8_t* end)
{
    if (m_firstFrame)
        return buff;
    return buff + 1;
}

#define READ_OFFSET(a) (m_big_offsets ? AV_RB32(a) : AV_RB16(a))

int DVBSubStreamReader::decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes)
{
    skipBytes = 0;
    skipBeforeBytes = 0;
    const int rez = intDecodeFrame(buff, end);
    if (rez <= 0)
        return rez;
    const int64_t currentTime = m_start_display_time;
    const int nextRez = intDecodeFrame(buff + rez, end);
    if (nextRez <= 0)
        return rez;
    m_frameDuration = m_start_display_time - currentTime;
    return rez;
}

int DVBSubStreamReader::intDecodeFrame(uint8_t* buff, uint8_t* end)
{
    return 0;  // not working yet

    int pos, cmd, x1, y1, x2, y2, offset1, offset2, next_cmd_pos;
    int cmd_pos, is_8bit = 0;
    const uint8_t* yuv_palette = nullptr;
    uint8_t colormap[4]{0}, alpha[256]{0};
    int date;
    int i;
    int is_menu = 0;
    int buf_size = static_cast<int>(end - buff);

    if (buf_size < 10)
        return NOT_ENOUGH_BUFFER;
    m_start_display_time = 0;
    m_end_display_time = 0;

    if (m_firstFrame)
    {
        if (AV_RB16(buff) == 0)
        {  // HD subpicture with 4-byte offsets
            m_big_offsets = 1;
            m_offset_size = 4;
        }
        else
        {
            m_big_offsets = 0;
            m_offset_size = 2;
        }
        m_firstFrame = false;
    }
    else
    {
        if (m_big_offsets && AV_RB16(buff) != 0)
            return BAD_FRAME;
        if (!m_big_offsets && AV_RB16(buff) == 0)
            return BAD_FRAME;
    }
    cmd_pos = 2 + m_big_offsets * 4;

    // int frameSize = READ_OFFSET(buff);

    cmd_pos = READ_OFFSET(buff + cmd_pos);

    while ((cmd_pos + 2 + m_offset_size) < buf_size)
    {
        date = AV_RB16(buff + cmd_pos);
        next_cmd_pos = READ_OFFSET(buff + cmd_pos + 2);
        pos = cmd_pos + 2 + m_offset_size;
        offset1 = -1;
        offset2 = -1;
        x1 = y1 = x2 = y2 = 0;
        bool breakFlag = false;
        while (pos < buf_size && !breakFlag)
        {
            cmd = buff[pos++];
            switch (cmd)
            {
            case 0x00:
                // menu subpicture
                is_menu = 1;
                break;
            case 0x01:
                // set start date
                m_start_display_time = (static_cast<int64_t>(date) << 10) * TS_FREQ_TO_INT_FREQ_COEFF;
                break;
            case 0x02:
                // set end date
                m_end_display_time = (static_cast<int64_t>(date) << 10) * TS_FREQ_TO_INT_FREQ_COEFF;
                break;
            case 0x03:
                // set colormap
                if ((buf_size - pos) < 2)
                    return NOT_ENOUGH_BUFFER;
                colormap[3] = buff[pos] >> 4;
                colormap[2] = buff[pos] & 0x0f;
                colormap[1] = buff[pos + 1] >> 4;
                colormap[0] = buff[pos + 1] & 0x0f;
                pos += 2;
                break;
            case 0x04:
                // set alpha
                if ((buf_size - pos) < 2)
                    return NOT_ENOUGH_BUFFER;
                alpha[3] = buff[pos] >> 4;
                alpha[2] = buff[pos] & 0x0f;
                alpha[1] = buff[pos + 1] >> 4;
                alpha[0] = buff[pos + 1] & 0x0f;
                pos += 2;
                break;
            case 0x05:
            case 0x85:
                if ((buf_size - pos) < 6)
                    return NOT_ENOUGH_BUFFER;
                x1 = (buff[pos] << 4) | (buff[pos + 1] >> 4);
                x2 = ((buff[pos + 1] & 0x0f) << 8) | buff[pos + 2];
                y1 = (buff[pos + 3] << 4) | (buff[pos + 4] >> 4);
                y2 = ((buff[pos + 4] & 0x0f) << 8) | buff[pos + 5];
                if (cmd & 0x80)
                    is_8bit = 1;
                pos += 6;
                break;
            case 0x06:
                if ((buf_size - pos) < 4)
                    return NOT_ENOUGH_BUFFER;
                offset1 = AV_RB16(buff + pos);
                offset2 = AV_RB16(buff + pos + 2);
                pos += 4;
                break;
            case 0x86:
                if ((buf_size - pos) < 8)
                    return NOT_ENOUGH_BUFFER;
                offset1 = AV_RB32(buff + pos);
                offset2 = AV_RB32(buff + pos + 4);
                pos += 8;
                break;

            case 0x83:
                // HD set palette
                if ((buf_size - pos) < 768)
                    return NOT_ENOUGH_BUFFER;
                yuv_palette = buff + pos;
                pos += 768;
                break;
            case 0x84:
                // HD set contrast (alpha)
                if ((buf_size - pos) < 256)
                    return NOT_ENOUGH_BUFFER;
                for (i = 0; i < 256; i++) alpha[i] = 0xFF - buff[pos + i];
                pos += 256;
                break;
            case 0xff:
            default:
                if (next_cmd_pos == cmd_pos)
                    return (pos + 1) & 0xfffffffe;
                cmd_pos = next_cmd_pos;
                breakFlag = true;
                break;
            }
        }
    }
    return NOT_ENOUGH_BUFFER;
}

double DVBSubStreamReader::getFrameDuration() { return static_cast<double>(m_frameDuration); }

const std::string DVBSubStreamReader::getStreamInfo()
{
    if (m_big_offsets)
        return "HD-DVD subpicture";
    return "Subpicture";
}
