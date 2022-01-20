#include "dtsStreamReader.h"

#include <sstream>

static const int DCA_EXT_CORE = 0x001;       ///< core in core substream
static const int DCA_EXT_XXCH = 0x002;       ///< XXCh channels extension in core substream
static const int DCA_EXT_X96 = 0x004;        ///< 96/24 extension in core substream
static const int DCA_EXT_XCH = 0x008;        ///< XCh channel extension in core substream
static const int DCA_EXT_EXSS_CORE = 0x010;  ///< core in ExSS (extension substream)
static const int DCA_EXT_EXSS_XBR = 0x020;   ///< extended bitrate extension in ExSS
static const int DCA_EXT_EXSS_XXCH = 0x040;  ///< XXCh channels extension in ExSS
static const int DCA_EXT_EXSS_X96 = 0x080;   ///< 96/24 extension in ExSS
static const int DCA_EXT_EXSS_LBR = 0x100;   ///< low bitrate component in ExSS
static const int DCA_EXT_EXSS_XLL = 0x200;   ///< lossless extension in ExSS

static const int dca_ext_audio_descr_mask[] = {DCA_EXT_XCH, -1, DCA_EXT_X96,  DCA_EXT_XCH | DCA_EXT_X96,
                                               -1,          -1, DCA_EXT_XXCH, -1};

static const unsigned int ppi_dts_samplerate[] = {0,     8000, 16000, 32000, 0,     0,     11025, 22050,
                                                  44100, 0,    0,     12000, 24000, 48000, 96000, 192000};

static const unsigned int dtshd_samplerate[] = {0x1F40,  0x3E80,  0x7D00, 0x0FA00, 0x1F400, 0x5622,  0x0AC44, 0x15888,
                                                0x2B110, 0x56220, 0x2EE0, 0x5DC0,  0x0BB80, 0x17700, 0x2EE00, 0x5DC00};

static const unsigned int ppi_dts_bitrate[] = {
    32000,   56000,   64000,   96000,   112000,  128000,  192000,  224000,     256000,         320000,  384000,
    448000,  512000,  576000,  640000,  768000,  896000,  1024000, 1152000,    1280000,        1344000, 1408000,
    1411200, 1472000, 1536000, 1920000, 2048000, 3072000, 3840000, 1 /*open*/, 2 /*variable*/, 3 /*lossless*/
};

static const int64_t AUPR_HDR = 0x415550522D484452ll;
static const int64_t AUPRINFO = 0x41555052494E464Fll;
static const int64_t BITSHVTB = 0x4249545348565442ll;
static const int64_t BLACKOUT = 0x424C41434B4F5554ll;
static const int64_t BRANCHPT = 0x4252414E43485054ll;
static const int64_t BUILDVER = 0x4255494C44564552ll;
static const int64_t CORESSMD = 0x434F524553534D44ll;
static const int64_t DTSHDHDR = 0x4454534844484452ll;
static const int64_t EXTSS_MD = 0x45585453535f4d44ll;
static const int64_t FILEINFO = 0x46494C45494E464Fll;
static const int64_t NAVI_TBL = 0x4E4156492D54424Cll;
static const int64_t STRMDATA = 0x5354524D44415441ll;
static const int64_t TIMECODE = 0x54494D45434F4445ll;

static const int AOUT_CHAN_CENTER = 0x1;
static const int AOUT_CHAN_LEFT = 0x2;
static const int AOUT_CHAN_RIGHT = 0x4;
static const int AOUT_CHAN_REARCENTER = 0x10;
static const int AOUT_CHAN_REARLEFT = 0x20;
static const int AOUT_CHAN_REARRIGHT = 0x40;
static const int AOUT_CHAN_MIDDLELEFT = 0x100;
static const int AOUT_CHAN_MIDDLERIGHT = 0x200;
static const int AOUT_CHAN_LFE = 0x1000;

static const int AOUT_CHAN_DOLBYSTEREO = 0x10000;
static const int AOUT_CHAN_DUALMONO = 0x20000;
static const int AOUT_CHAN_REVERSESTEREO = 0x40000;

using namespace std;

int DTSStreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    uint8_t* frame = findFrame(m_buffer, m_bufEnd);
    if (frame == 0)
        return 0;
    int skipBytes = 0;
    int skipBeforeBytes = 0;
    int len = decodeFrame(frame, m_bufEnd, skipBytes, skipBeforeBytes);
    if (len < 1)
        return 0;

    m_state = DTSDecodeState::stDecodeDTS;

    // no DTS descriptor in Blu-ray
    if (hdmvDescriptors)
        return 0;

    // ETSI TS 101 154 F.4.2 DTS registration descriptor
    *dstBuff++ = (int)TSDescriptorTag::REGISTRATION;  // descriptor tag
    *dstBuff++ = 4;                                   // descriptor length
    *dstBuff++ = 'D';
    *dstBuff++ = 'T';
    *dstBuff++ = 'S';

    switch (pi_frame_length)
    {
    case 512:
        *dstBuff++ = '1';
        break;
    case 1024:
        *dstBuff++ = '2';
        break;
    case 2048:
        *dstBuff++ = '3';
        break;
    default:
        *dstBuff++ = ' ';
    }

    // ETSI TS 101 154 F.4.3 DTS audio descriptor
    BitStreamWriter bitWriter{};

    *dstBuff++ = (uint8_t)TSDescriptorTag::DTS;  // descriptor tag;
    *dstBuff++ = 5;                              // descriptor length
    bitWriter.setBuffer(dstBuff, dstBuff + 5);
    bitWriter.putBits(4, pi_sample_rate_index);
    bitWriter.putBits(6, pi_bit_rate_index);
    bitWriter.putBits(7, nblks);
    bitWriter.putBits(14, len);
    bitWriter.putBits(6, getSurroundModeCode());
    bitWriter.putBits(1, (pi_channels_conf & AOUT_CHAN_LFE) ? 1 : 0);
    bitWriter.putBits(2, m_dtsEsChannels ? 1 : 0);  // DTS-EX flag
    bitWriter.flushBits();

    return 13;  // descriptor length
}

void DTSStreamReader::writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket)
{
    // 0f 81 71 - from blu ray DTS-HD  ( can use 0x01 instead 0x0f. bits 1-3 are reserved.)
    // 0x01 0x81 0x71 - ordinal DTS == 0x71?
    if (m_useNewStyleAudioPES)
    {
        pesPacket->flagsLo |= 1;  // enable PES extension for DTS stream
        uint8_t* data = (uint8_t*)(pesPacket) + pesPacket->getHeaderLength();
        *data++ = 1;     // PES_extension_flag_2
        *data++ = 0x81;  // marker bit + extension2 len
        if (m_state == DTSDecodeState::stDecodeHD2 || !m_isCoreExists)
            *data++ = 0x72;  // stream id extension. 71 = DTS frame, 72 HD frame
        else
            *data++ = 0x71;  // stream id extension
        pesPacket->m_pesHeaderLen += 3;
    }
}

int DTSStreamReader::getSurroundModeCode()
{
    int rez = 0;
    if ((pi_channels_conf & AOUT_CHAN_LEFT) && (pi_channels_conf & AOUT_CHAN_RIGHT) &&
        (pi_channels_conf & AOUT_CHAN_CENTER) && (pi_channels_conf & AOUT_CHAN_REARLEFT) &&
        (pi_channels_conf & AOUT_CHAN_REARRIGHT))
        rez = 0x09;
    else if ((pi_channels_conf & AOUT_CHAN_LEFT) && (pi_channels_conf & AOUT_CHAN_RIGHT) &&
             (pi_channels_conf & AOUT_CHAN_REARLEFT) && (pi_channels_conf & AOUT_CHAN_REARRIGHT))
        rez = 0x08;
    else if ((pi_channels_conf & AOUT_CHAN_LEFT) && (pi_channels_conf & AOUT_CHAN_RIGHT) &&
             (pi_channels_conf & AOUT_CHAN_CENTER) &&
             ((pi_channels_conf & AOUT_CHAN_REARLEFT) || (pi_channels_conf & AOUT_CHAN_REARRIGHT) ||
              (pi_channels_conf & AOUT_CHAN_REARCENTER)))
        rez = 0x07;
    else if ((pi_channels_conf & AOUT_CHAN_LEFT) && (pi_channels_conf & AOUT_CHAN_RIGHT) &&
             ((pi_channels_conf & AOUT_CHAN_REARLEFT) || (pi_channels_conf & AOUT_CHAN_REARRIGHT) ||
              (pi_channels_conf & AOUT_CHAN_REARCENTER)))
        rez = 0x06;
    else if ((pi_channels_conf & AOUT_CHAN_LEFT) && (pi_channels_conf & AOUT_CHAN_RIGHT) &&
             (pi_channels_conf & AOUT_CHAN_CENTER))
        rez = 0x05;
    else if ((pi_channels_conf & AOUT_CHAN_LEFT) && (pi_channels_conf & AOUT_CHAN_RIGHT))
        rez = 0x02;
    return rez;
}

void DTSStreamReader::checkIfOnlyHDDataExists(uint8_t* buff, uint8_t* end)
{
    for (int i = 0; i < 2 && buff < end - 4; ++i)
    {
        bool isHDData = *((uint32_t*)buff) == my_htonl(DTS_HD_PREFIX);
        if (!isHDData)
            return;

        BitStreamReader reader{};
        reader.setBuffer(buff + 5, end);  // skip 4 byte magic and 1 unknown byte
        int headerSize;
        int hdFrameSize;
        int nuSubStreamIndex = reader.getBits(2);
        if (reader.getBit())
        {
            headerSize = reader.getBits(12) + 1;
            hdFrameSize = reader.getBits(20) + 1;
        }
        else
        {
            headerSize = reader.getBits(8) + 1;
            hdFrameSize = reader.getBits(16) + 1;
        }
        buff += hdFrameSize;
    }
    m_isCoreExists = false;
}

uint8_t* DTSStreamReader::findFrame(uint8_t* buff, uint8_t* end)
{
    // check for DTS-HD headers
    while (end - buff >= 16)
    {
        int64_t* ptr = (int64_t*)buff;
        uint64_t hdrType = my_ntohll(ptr[0]);
        uint64_t hdrSize = my_ntohll(ptr[1]) + 16;

        if (hdrSize > (uint64_t)1 << 61)
            break;

        if (hdrType == AUPRINFO || hdrType == BITSHVTB || hdrType == BLACKOUT || hdrType == BRANCHPT ||
            hdrType == BUILDVER || hdrType == CORESSMD || hdrType == EXTSS_MD || hdrType == FILEINFO ||
            hdrType == NAVI_TBL || hdrType == TIMECODE || hdrType == DTSHDHDR)
        {
            if (hdrSize > (size_t)(end - buff))
                return 0;  // need more data
            buff += hdrSize;
        }
        else if (hdrType == AUPR_HDR)
        {
            if (buff + hdrSize > end)
                return 0;  // need more data
            // determine skipping frames amount
            m_skippingSamples = (buff[35] << 8) + buff[36];
            buff += hdrSize;
        }
        else if (hdrType == STRMDATA)
        {
            // m_dataSegmentLen = hdrSize;
            buff += 16;
            break;
        }
        else
        {
            break;  // no more HD headers
        }
    }

    if (m_firstCall)
    {
        m_firstCall = false;
        checkIfOnlyHDDataExists(buff, end);
    }

    if (!m_isCoreExists)
    {
        // if not core exists find HD frames directly
        for (uint8_t* p_buf = buff; p_buf < end - 4; p_buf++)
        {
            if (p_buf[0] == 0x64 && p_buf[1] == 0x58 && p_buf[2] == 0x20 && p_buf[3] == 0x25)
            {
                return p_buf;
            }
        }
        return 0;
    }

    for (uint8_t* p_buf = buff; p_buf < end - 4; p_buf++)
    {
        if (p_buf < end - 6)
        {
            if (p_buf[0] == 0xff && p_buf[1] == 0x1f && p_buf[2] == 0x00 && p_buf[3] == 0xe8 &&
                (p_buf[4] & 0xf0) == 0xf0 && p_buf[5] == 0x07)
            {
                return p_buf;
            }
            // 14 bits, big endian version of the bitstream
            else if (p_buf[0] == 0x1f && p_buf[1] == 0xff && p_buf[2] == 0xe8 && p_buf[3] == 0x00 && p_buf[4] == 0x07 &&
                     (p_buf[5] & 0xf0) == 0xf0)
            {
                return p_buf;
            }
        }
        // 16 bits, big endian version of the bitstream
        if (p_buf[0] == 0x7f && p_buf[1] == 0xfe && p_buf[2] == 0x80 && p_buf[3] == 0x01)
        {
            return p_buf;
        }
        // 16 bits, little endian version of the bitstream
        else if (p_buf[0] == 0xfe && p_buf[1] == 0x7f && p_buf[2] == 0x01 && p_buf[3] == 0x80)
        {
            return p_buf;
        }
    }
    return 0;
}

int DTSStreamReader::decodeHdInfo(uint8_t* buff, uint8_t* end)
{
    try
    {
        BitStreamReader reader{};
        reader.setBuffer(buff + 5, end);  // skip 4 byte magic and 1 unknown byte
        int headerSize;
        int hdFrameSize;
        int nuSubStreamIndex = reader.getBits(2);
        bool isBlownUpHeader = reader.getBit();
        if (isBlownUpHeader)
        {
            headerSize = reader.getBits(12) + 1;
            hdFrameSize = reader.getBits(20) + 1;
        }
        else
        {
            headerSize = reader.getBits(8) + 1;
            hdFrameSize = reader.getBits(16) + 1;
        }
        if (m_hdType == DTSHD_SUBTYPE::DTS_SUBTYPE_UNINITIALIZED)
        {
            if (buff + headerSize + 4 > end)
                return NOT_ENOUGH_BUFFER;
            uint32_t* hdAudioData = (uint32_t*)(buff + headerSize);
            switch (my_ntohl(*hdAudioData))
            {
            case 0x41A29547:  // XLL
                m_hdType = DTSHD_SUBTYPE::DTS_SUBTYPE_MASTER_AUDIO;
                break;
            case 0x655E315E:  // XBR
                m_hdType = DTSHD_SUBTYPE::DTS_SUBTYPE_HIGH_RES;
                break;
            case 0x0A801921:  // LBR
                m_hdType = DTSHD_SUBTYPE::DTS_SUBTYPE_EXPRESS;
                break;
            case 0x47004A03:  // XXCH
            case 0x5A5A5A5A:  // XCH
                m_hdType = DTSHD_SUBTYPE::DTS_SUBTYPE_EX;
                break;
            case 0x1D95F262:  // X96
                m_hdType = DTSHD_SUBTYPE::DTS_SUBTYPE_96;
                break;
            default:  // Core, Substream, Neo, Sensation...
                m_hdType = DTSHD_SUBTYPE::DTS_SUBTYPE_OTHER;
            }
        }
        else
            return hdFrameSize;

        int nuNumAudioPresent = 1;
        int nuNumAssets = 1;
        bool bStaticFieldsPresent = reader.getBit();
        if (bStaticFieldsPresent)
        {
            int nuRefClockCode = reader.getBits(2);
            int nuExSSFrameDurationCode = reader.getBits(3) + 1;
            if (pi_frame_length == 0)
                pi_frame_length = nuExSSFrameDurationCode << 9;

            if (reader.getBit())
            {
                reader.skipBits(18);  // nuTimeStamp, 18 high bits
                reader.skipBits(18);  // nuTimeStamp, 18 low bits
            }
            nuNumAudioPresent = reader.getBits(3) + 1;
            nuNumAssets = reader.getBits(3) + 1;
            for (int i = 0; i < nuNumAudioPresent; i++) reader.skipBits(nuSubStreamIndex + 1);
            for (int i = 0; i < nuNumAudioPresent; i++)
            {
                for (int j = 0; j < nuSubStreamIndex + 1; j++)
                {
                    if ((j + 1) % 2)
                        reader.skipBits(8);
                }
            }
            if (reader.getBit())
            {
                int nuMixMetadataAdjLevel = reader.getBits(2);
                int nuBits4MixOutMask = reader.getBits(2) * 4 + 4;
                int nuNumMixOutConfigs = reader.getBits(2) + 1;
                for (int i = 0; i < nuNumMixOutConfigs; i++) reader.skipBits(nuBits4MixOutMask);
            }
        }
        for (int i = 0; i < nuNumAssets; i++) reader.skipBits(isBlownUpHeader ? 20 : 16);

        for (int i = 0; i < nuNumAssets; i++)
        {
            reader.skipBits(12);  // nuAssetDescriptorFSIZE - 1, DescriptorDataForAssetIndex
            if (bStaticFieldsPresent)
            {
                if (reader.getBit())     // AssetTypeDescrPresent
                    reader.skipBits(4);  // AssetTypeDescriptor

                if (reader.getBit())      // LanguageDescrPresent
                    reader.skipBits(24);  // LanguageDescriptor

                if (reader.getBit())  // bInfoTextPresent
                {
                    int nuInfoTextByteSize = reader.getBits(10) + 1;
                    for (int j = 0; j < nuInfoTextByteSize; j++) reader.skipBits(8);
                }
                int nuBitResolution = reader.getBits(5) + 1;
                int nuMaxSampleRate = reader.getBits(4);
                hd_pi_channels = reader.getBits(8) + 1;
                int nuSpkrActivityMask = 0;
                if (reader.getBit())  // bOne2OneMapChannels2Speakers
                {
                    if (hd_pi_channels > 2)
                        reader.skipBit();  // bEmbeddedStereoFlag

                    if (hd_pi_channels > 6)
                        reader.skipBit();  // bEmbeddedSixChFlag

                    if (reader.getBit())  // bSpkrMaskEnabled
                    {
                        int nuNumBits4SAMask = reader.getBits(2) * 4 + 4;
                        nuSpkrActivityMask = reader.getBits(nuNumBits4SAMask);
                    }
                    // TODO...
                }
                hd_pi_sample_rate = dtshd_samplerate[nuMaxSampleRate];
                hd_bitDepth = nuBitResolution;

                if (!m_isCoreExists)
                    m_frameDuration = pi_frame_length * 1e9 / hd_pi_sample_rate;

                if (m_hdType != DTSHD_SUBTYPE::DTS_SUBTYPE_MASTER_AUDIO)
                    m_hdBitrate = (unsigned)(hd_pi_sample_rate / (double)pi_frame_length * hdFrameSize * 8);

                hd_pi_lfeCnt = 0;

                if ((nuSpkrActivityMask & 0x8) == 0x8)
                    ++hd_pi_lfeCnt;

                if ((nuSpkrActivityMask & 0x1000) == 0x1000)
                    ++hd_pi_lfeCnt;
            }
            break;
        }
        return hdFrameSize;
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NOT_ENOUGH_BUFFER;
    }
}

bool DTSStreamReader::isPriorityData(AVPacket* packet)
{
    return packet->size >= 2 && ((packet->data[0] == 0x7f && packet->data[1] == 0xfe) ||
                                 (packet->data[0] == 0xfe && packet->data[1] == 0x7f));
}

bool DTSStreamReader::isSecondary() { return m_secondary; }

int DTSStreamReader::decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes)
{
    uint8_t* afterFrameData = buff;
    i_frame_size = 0;
    if (m_isCoreExists)
    {
        skipBeforeBytes = skipBytes = 0;
        if (m_state == DTSDecodeState::stDecodeHD && *((uint32_t*)buff) == my_htonl(DTS_HD_PREFIX))
        {
            m_state = DTSDecodeState::stDecodeHD2;
            return m_hdFrameLen;
        }
        m_state = DTSDecodeState::stDecodeDTS;

        // 14 bits, little endian version of the bitstream
        if (buff[0] == 0xff && buff[1] == 0x1f && buff[2] == 0x00 && buff[3] == 0xe8 && (buff[4] & 0xf0) == 0xf0 &&
            buff[5] == 0x07)
        {
            uint8_t conv_buf[DTS_HEADER_SIZE];
            buf14To16(conv_buf, buff, DTS_HEADER_SIZE, 1);
            i_frame_size = syncInfo16be(conv_buf);
            i_frame_size = i_frame_size * 8 / 14 * 2;
        }
        // 14 bits, big endian version of the bitstream
        else if (buff[0] == 0x1f && buff[1] == 0xff && buff[2] == 0xe8 && buff[3] == 0x00 && buff[4] == 0x07 &&
                 (buff[5] & 0xf0) == 0xf0)
        {
            uint8_t conv_buf[DTS_HEADER_SIZE];
            buf14To16(conv_buf, buff, DTS_HEADER_SIZE, 0);
            i_frame_size = syncInfo16be(conv_buf);
            i_frame_size = i_frame_size * 8 / 14 * 2;
        }
        // 16 bits, big endian version of the bitstream
        else if (buff[0] == 0x7f && buff[1] == 0xfe && buff[2] == 0x80 && buff[3] == 0x01)
        {
            i_frame_size = syncInfo16be(buff);
        }
        // 16 bits, little endian version of the bitstream
        else if (buff[0] == 0xfe && buff[1] == 0x7f && buff[2] == 0x01 && buff[3] == 0x80)
        {
            uint8_t conv_buf[DTS_HEADER_SIZE];
            BufLeToBe(conv_buf, buff, DTS_HEADER_SIZE);
            i_frame_size = syncInfo16be(buff);
        }
        else
            return 0;

        switch (pi_audio_mode & 0xFFFF)
        {
        case 0x0:
            // Mono
            pi_channels_conf = AOUT_CHAN_CENTER;
            break;
        case 0x1:
            // Dual-mono = stereo + dual-mono
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_DUALMONO;
            break;
        case 0x2:
        case 0x3:
        case 0x4:
            // Stereo
            pi_channels = 2;
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 0x5:
            // 3F
            pi_channels = 3;
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER;
            break;
        case 0x6:
            // 2F/1R
            pi_channels = 3;
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER;
            break;
        case 0x7:
            // 3F/1R
            pi_channels = 4;
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARCENTER;
            break;
        case 0x8:
            // 2F2R
            pi_channels = 4;
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0x9:
            // 3F2R
            pi_channels = 5;
            pi_channels_conf =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0xA:
        case 0xB:
            // 2F2M2R
            pi_channels = 6;
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
                               AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0xC:
            // 3F2M2R
            pi_channels = 7;
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                               AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0xD:
        case 0xE:
            // 3F2M2R/LFE
            pi_channels = 8;
            pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                               AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
            break;
        default:
            if (pi_audio_mode <= 63)
            {
                // User defined
                pi_channels = 0;
                pi_channels_conf = 0;
            }
            else
                return 0;
        }

        if (pi_audio_mode & 0x10000)
        {
            (pi_channels)++;
            pi_channels_conf |= AOUT_CHAN_LFE;
            pi_lfeCnt = 1;
        }

        if (pi_sample_rate_index >= sizeof(ppi_dts_samplerate) / sizeof(ppi_dts_samplerate[0]))
        {
            return 0;
        }
        pi_sample_rate = ppi_dts_samplerate[pi_sample_rate_index];
        if (!pi_sample_rate)
            return 0;

        if (pi_bit_rate_index >= sizeof(ppi_dts_bitrate) / sizeof(ppi_dts_bitrate[0]))
        {
            return 0;
        }
        pi_bit_rate = ppi_dts_bitrate[pi_bit_rate_index];
        if (!pi_bit_rate)
            return 0;

        pi_frame_length = (nblks + 1) * 32;
        m_frameDuration = pi_frame_length * 1e9 / pi_sample_rate;

        afterFrameData = buff + i_frame_size;
        if (afterFrameData > end - 4)
            return NOT_ENOUGH_BUFFER;

        if (m_testMode && m_dtsEsChannels == 0)
        {
            uint32_t* curPtr32 = (uint32_t*)(buff + 16);
            int findSize = FFMIN((int)(end - buff), i_frame_size) / 4 - 4;
            for (int i = 0; i < findSize; ++i)
            {
                if (*curPtr32++ == 0x5a5a5a5a)
                {
                    uint8_t* exHeader = (uint8_t*)curPtr32;
                    int dataRest = (int)(buff + i_frame_size - exHeader);
                    int frameSize = (int)((exHeader[0] << 2) + (exHeader[1] >> 6) - 4);  // remove 4 bytes of ext world
                    if (dataRest - frameSize == 0 || dataRest - frameSize == 1)
                    {
                        m_dtsEsChannels = (int(exHeader[1]) >> 2) & 0x07;
                    }
                }
            }
        }
    }

    if (*(uint32_t*)afterFrameData == my_htonl(DTS_HD_PREFIX))
    {
        m_dts_hd_mode = true;

        int hdFrameSize = decodeHdInfo(afterFrameData, end);
        if (hdFrameSize == NOT_ENOUGH_BUFFER)
            return NOT_ENOUGH_BUFFER;

        uint8_t* nextFrame = afterFrameData + hdFrameSize;
        if (nextFrame >= end)
            return NOT_ENOUGH_BUFFER;
        if (m_downconvertToDTS)
        {
            skipBytes = (int)(nextFrame - buff - i_frame_size);
            return i_frame_size;
        }
        else
        {
            m_state = DTSDecodeState::stDecodeHD;
            m_hdFrameLen = (int)(nextFrame - afterFrameData);
            return m_isCoreExists ? i_frame_size : m_hdFrameLen;
        }
    }
    else
        return i_frame_size;
}

int DTSStreamReader::syncInfo16be(const uint8_t* p_buf)
{
    unsigned int frame_size;
    unsigned int i_lfe;
    nblks = (p_buf[4] & 0x01) << 6 | (p_buf[5] >> 2);
    frame_size = (p_buf[5] & 0x03) << 12 | (p_buf[6] << 4) | (p_buf[7] >> 4);
    pi_audio_mode = (p_buf[7] & 0x0f) << 2 | (p_buf[8] >> 6);
    pi_sample_rate_index = (p_buf[8] >> 2) & 0x0f;
    pi_bit_rate_index = (p_buf[8] & 0x03) << 3 | ((p_buf[9] >> 5) & 0x07);
    i_lfe = (p_buf[10] >> 1) & 0x03;
    if (i_lfe)
        pi_audio_mode |= 0x10000;

    int pi_ext_coding = (p_buf[10] >> 4) & 0x01;
    if (pi_ext_coding)
    {
        int ext_descr = p_buf[10] >> 5;
        core_ext_mask = dca_ext_audio_descr_mask[ext_descr];
    }
    else
    {
        core_ext_mask = 0;
    }

    return frame_size + 1;
}

int DTSStreamReader::testSyncInfo16be(const uint8_t* p_buf)
{
    unsigned int frame_size;
    unsigned int test_lfe;
    unsigned int test_audio_mode, test_sample_rate_index, test_bit_rate_index;
    nblks = (p_buf[4] & 0x01) << 6 | (p_buf[5] >> 2);
    frame_size = (p_buf[5] & 0x03) << 12 | (p_buf[6] << 4) | (p_buf[7] >> 4);
    test_audio_mode = (p_buf[7] & 0x0f) << 2 | (p_buf[8] >> 6);
    test_sample_rate_index = (p_buf[8] >> 2) & 0x0f;
    test_bit_rate_index = (p_buf[8] & 0x03) << 3 | ((p_buf[9] >> 5) & 0x07);
    test_lfe = (p_buf[10] >> 1) & 0x03;
    if (test_lfe)
        test_audio_mode |= 0x10000;
    if (test_audio_mode == pi_audio_mode && test_sample_rate_index == pi_sample_rate_index &&
        test_bit_rate_index == pi_bit_rate_index)
        return frame_size + 1;
    else
        return 0;
}

int DTSStreamReader::buf14To16(uint8_t* p_out, const uint8_t* p_in, int i_in, int i_le)
{
    unsigned char tmp, cur = 0;
    int bits_in, bits_out = 0;
    int i_out = 0;
    for (int i = 0; i < i_in; i++)
    {
        if (i % 2)
        {
            tmp = p_in[i - i_le];
            bits_in = 8;
        }
        else
        {
            tmp = p_in[i + i_le] & 0x3F;
            bits_in = 8 - 2;
        }

        if (bits_out < 8)
        {
            int need = (std::min)(8 - bits_out, bits_in);
            cur <<= need;
            cur |= (tmp >> (bits_in - need));
            tmp <<= (8 - bits_in + need);
            tmp >>= (8 - bits_in + need);
            bits_in -= need;
            bits_out += need;
        }
        if (bits_out == 8)
        {
            p_out[i_out] = cur;
            cur = 0;
            bits_out = 0;
            i_out++;
        }

        bits_out += bits_in;
        cur <<= bits_in;
        cur |= tmp;
    }
    return i_out;
}

void DTSStreamReader::BufLeToBe(uint8_t* p_out, const uint8_t* p_in, int i_in)
{
    for (int i = 0; i < i_in / 2; i++)
    {
        p_out[i * 2] = p_in[i * 2 + 1];
        p_out[i * 2 + 1] = p_in[i * 2];
    }
}

double DTSStreamReader::getFrameDurationNano()
{
    if (!m_isCoreExists)
        return m_frameDuration;
    else if (m_dts_hd_mode && !m_downconvertToDTS && m_state != DTSDecodeState::stDecodeHD2)
        return 0;
    else
        return m_frameDuration;
}

bool DTSStreamReader::needSkipFrame(const AVPacket& packet)
{
    if (m_skippingSamples == 0)
        return false;

    if (getFrameDurationNano() > 0)
        m_skippingSamples -= pi_frame_length;
    return true;
}

const std::string DTSStreamReader::getStreamInfo()
{
    std::ostringstream str;

    if (core_ext_mask & DCA_EXT_XCH)
        str << "DTS-ES";
    else if (core_ext_mask & DCA_EXT_X96)
        str << "DTS-96/24";
    if (core_ext_mask & (DCA_EXT_XCH | DCA_EXT_X96))
    {
        if (hd_pi_channels)
            str << " core ";
        else
            str << " ";
    }

    str << "Bitrate: ";
    // 1/*open*/, 2/*variable*/, 3/*lossless*/
    switch (pi_bit_rate)
    {
    case 1:
        str << "Open. ";
        break;
    case 2:
        str << "VBR. ";
        break;
    case 3:
        str << "Loseless. ";
        break;
    default:
        str << (pi_bit_rate + m_hdBitrate) / 1000 << "Kbps  ";
    }

    if (m_hdType == DTSHD_SUBTYPE::DTS_SUBTYPE_MASTER_AUDIO)
        str << "core + MLP data.";
    str << "Sample Rate: " << (hd_pi_sample_rate ? hd_pi_sample_rate : pi_sample_rate) / 1000 << "KHz  ";
    str << "Channels: ";
    if (hd_pi_channels)
    {
        if (hd_pi_lfeCnt)
            str << hd_pi_channels - hd_pi_lfeCnt << '.' << hd_pi_lfeCnt;
        else
            str << hd_pi_channels;
    }
    else
    {
        if (pi_lfeCnt)
            str << (int)(pi_channels + m_dtsEsChannels - pi_lfeCnt) << '.' << pi_lfeCnt;
        else
            str << (int)(pi_channels + m_dtsEsChannels);
    }
    if (m_dts_hd_mode)
    {
        switch (m_hdType)
        {
        case DTSHD_SUBTYPE::DTS_SUBTYPE_MASTER_AUDIO:
            str << " (DTS Master Audio";
            break;
        case DTSHD_SUBTYPE::DTS_SUBTYPE_HIGH_RES:
            str << " (DTS High Resolution";
            break;
        case DTSHD_SUBTYPE::DTS_SUBTYPE_EXPRESS:
            str << " (DTS Express";
            break;
        case DTSHD_SUBTYPE::DTS_SUBTYPE_EX:
            str << " (DTS Ex";
            break;
        case DTSHD_SUBTYPE::DTS_SUBTYPE_96:
            str << " (DTS 96";
            break;
        case DTSHD_SUBTYPE::DTS_SUBTYPE_UNINITIALIZED:
        case DTSHD_SUBTYPE::DTS_SUBTYPE_OTHER:
            break;
        }

        if (hd_bitDepth > 16)
            str << " 24bit";

        str << ")";
    }
    return str.str();
}
