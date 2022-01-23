#include "ac3Codec.h"

#include <sstream>

#include "avCodecs.h"
#include "bitStream.h"
#include "vodCoreException.h"
#include "vod_common.h"

#define max(a, b) (a > b ? a : b)
static const int NB_BLOCKS = 6;  // number of PCM blocks inside an AC3 frame
static const int AC3_FRAME_SIZE = NB_BLOCKS * 256;

bool isSyncWord(uint8_t* buff) { return buff[0] == 0x0B && buff[1] == 0x77; }

bool isHDSyncWord(uint8_t* buff) { return buff[0] == 0xf8 && buff[1] == 0x72 && buff[2] == 0x6f; }

static const uint8_t eac3_blocks[4] = {1, 2, 3, 6};

const uint8_t ff_ac3_channels[8] = {2, 1, 2, 3, 3, 4, 4, 5};

// possible frequencies
const uint16_t ff_ac3_freqs[3] = {48000, 44100, 32000};

// possible bitrates
const uint16_t ff_ac3_bitratetab[19] = {32,  40,  48,  56,  64,  80,  96,  112, 128, 160,
                                        192, 224, 256, 320, 384, 448, 512, 576, 640};

const uint16_t ff_ac3_frame_sizes[38][3] = {
    {64, 69, 96},       {64, 70, 96},       {80, 87, 120},      {80, 88, 120},      {96, 104, 144},
    {96, 105, 144},     {112, 121, 168},    {112, 122, 168},    {128, 139, 192},    {128, 140, 192},
    {160, 174, 240},    {160, 175, 240},    {192, 208, 288},    {192, 209, 288},    {224, 243, 336},
    {224, 244, 336},    {256, 278, 384},    {256, 279, 384},    {320, 348, 480},    {320, 349, 480},
    {384, 417, 576},    {384, 418, 576},    {448, 487, 672},    {448, 488, 672},    {512, 557, 768},
    {512, 558, 768},    {640, 696, 960},    {640, 697, 960},    {768, 835, 1152},   {768, 836, 1152},
    {896, 975, 1344},   {896, 976, 1344},   {1024, 1114, 1536}, {1024, 1115, 1536}, {1152, 1253, 1728},
    {1152, 1254, 1728}, {1280, 1393, 1920}, {1280, 1394, 1920}};

static const int AC3_ACMOD_MONO = 1;
static const int AC3_ACMOD_STEREO = 2;

const CodecInfo& AC3Codec::getCodecInfo()
{
    if (m_true_hd_mode)
        return trueHDCodecInfo;
    if (isEAC3())
        return eac3CodecInfo;
    else
        return ac3CodecInfo;
}

// returns NO_ERROR, or type of error
AC3Codec::AC3ParseError AC3Codec::parseHeader(uint8_t* buf, uint8_t* end)
{
    BitStreamReader gbc{};
    gbc.setBuffer(buf, end);

    if (gbc.getBits(16) != 0x0B77)  // sync_word
        return AC3ParseError::SYNC;

    // read ahead to bsid to make sure this is AC-3, not E-AC-3
    int id = gbc.showBits(29) & 0x1F;
    if (id > 16)
        return AC3ParseError::BSID;

    m_bsid = id;
    if (m_bsid > 10)  // bsid = 16 => EAC3
    {
        int numblkscod, strmtyp, substreamid, number_of_blocks_per_syncframe;
        int acmod, lfeon, bsmod, fscod, dsurmod, pgmscle, extpgmscle, mixdef, paninfoe;
        bsmod = fscod = dsurmod = pgmscle = extpgmscle = mixdef = paninfoe = 0;

        strmtyp = gbc.getBits(2);
        if (strmtyp == 3)
            return AC3ParseError::SYNC;  // invalid stream type

        substreamid = gbc.getBits(3);

        m_frame_size = (gbc.getBits(11) + 1) * 2;
        if (m_frame_size < AC3_HEADER_SIZE)
            return AC3ParseError::FRAME_SIZE;  // invalid header size

        fscod = gbc.getBits(2);

        if (fscod == 3)
        {
            int m_fscod2 = gbc.getBits(2);
            if (m_fscod2 == 3)
                return AC3ParseError::SYNC;

            numblkscod = 3;
            m_sample_rate = ff_ac3_freqs[m_fscod2] / 2;
        }
        else
        {
            numblkscod = gbc.getBits(2);
            m_sample_rate = ff_ac3_freqs[fscod];
        }
        number_of_blocks_per_syncframe = numblkscod == 0 ? 1 : (numblkscod == 1 ? 2 : (numblkscod == 2 ? 3 : 6));
        acmod = gbc.getBits(3);
        lfeon = gbc.getBit();

        m_samples = eac3_blocks[numblkscod] << 8;
        m_bit_rateExt = m_frame_size * m_sample_rate * 8 / m_samples;

        gbc.skipBits(5);  // skip bsid, already got it
        for (int i = 0; i < (acmod ? 1 : 2); i++)
        {
            gbc.skipBits(5);  // skip dialog normalization
            if (gbc.getBit())
                gbc.skipBits(8);  // skip Compression gain word
        }

        if (strmtyp == 1)
        {
            if (gbc.getBit())
            {
                int chanmap = gbc.getBits(16);
                if (chanmap & 0x7fe0)  // mask standard 5.1 channels
                    m_extChannelsExists = true;
            }
        }
        if (gbc.getBit())  // mixing metadata
        {
            if (acmod > 2)
                gbc.skipBits(2);  // dmixmod
            if ((acmod & 1) && (acmod > 0x2))
                gbc.skipBits(6);  // ltrtcmixlev, lorocmixlev
            if (acmod & 4)
                gbc.skipBits(6);  // ltrtsurmixlev, lorosurmixlev
            if (lfeon && gbc.getBit())
                gbc.skipBits(5);  // lfemixlevcod
            if (strmtyp == 0)
            {
                pgmscle = gbc.getBit();
                if (pgmscle)
                    gbc.skipBits(6);  // pgmscl
                if (acmod == 0 && gbc.getBit())
                    gbc.skipBits(6);  // pgmscl2
                extpgmscle = gbc.getBit();
                if (extpgmscle)
                    gbc.skipBits(6);  // extpgmscl
                mixdef = gbc.getBits(2);
                if (mixdef == 1)
                    gbc.skipBits(5);  // premixcmpsel, drcsrc, premixcmpscl
                else if (mixdef == 2)
                    gbc.skipBits(12);  // mixdata
                else if (mixdef == 3)
                {
                    int mixdeflen = gbc.getBits(5);  //
                    if (gbc.getBit())                // mixdata2e
                    {
                        gbc.skipBits(5);  // premixcmpsel, drcsrc, premixcmpscl
                        if (gbc.getBit())
                            gbc.skipBits(4);  // extpgmlscl
                        if (gbc.getBit())
                            gbc.skipBits(4);  // extpgmcscl
                        if (gbc.getBit())
                            gbc.skipBits(4);  // extpgmrscl
                        if (gbc.getBit())
                            gbc.skipBits(4);  // extpgmlscl
                        if (gbc.getBit())
                            gbc.skipBits(4);  // extpgmrsscl
                        if (gbc.getBit())
                            gbc.skipBits(4);  // extpgmlfescl
                        if (gbc.getBit())
                            gbc.skipBits(4);  // dmixscl
                        if (gbc.getBit())
                        {
                            if (gbc.getBit())
                                gbc.skipBits(4);  // extpgmaux1scl
                            if (gbc.getBit())
                                gbc.skipBits(4);  // extpgmaux2scl
                        }
                    }
                    if (gbc.getBit())  // mixdata3e
                    {
                        gbc.skipBits(5);  // spchdat
                        if (gbc.getBit())
                        {
                            gbc.skipBits(7);  // spchdat1, spchan1att
                            if (gbc.getBit())
                                gbc.skipBits(7);  // spchdat2, spchan2att
                        }
                    }
                    for (int i = 0; i < mixdeflen; i++) gbc.skipBits(8);  // mixdata
                    for (int i = 0; i < 7; i++)                           // mixdatafill
                        if (!gbc.showBits(1))
                            gbc.skipBit();
                        else
                            break;
                }
                if (acmod < 2)
                {
                    paninfoe = gbc.getBit();
                    if (paninfoe)
                        gbc.skipBits(14);  // panmean, paninfo
                    if (acmod == 0x0 && gbc.getBit())
                        gbc.skipBits(14);  // panmean2, paninfo2
                }
                if (gbc.getBit())
                {
                    if (numblkscod == 0)
                        gbc.skipBits(5);  // blkmixcfginfo[0]
                    else
                        for (int blk = 0; blk < number_of_blocks_per_syncframe; blk++)
                            if (gbc.getBit())
                                gbc.skipBits(5);  // blkmixcfginfo[blk]
                }
            }
        }
        if (gbc.getBit())
        {
            bsmod = gbc.getBits(3);
            gbc.skipBits(2);  // copyrightb, origbs
            if (acmod == 2)
                dsurmod = gbc.getBits(2);
        }
        m_mixinfoexists = pgmscle || extpgmscle || mixdef > 0 || paninfoe;

        if (m_channels == 0)  // no AC3 interleave
        {
            m_acmod = acmod;
            m_lfeon = lfeon;
            m_bsmod = bsmod;
            m_fscod = fscod;
            m_dsurmod = dsurmod;
            m_channels = ff_ac3_channels[acmod] + lfeon;
        }
    }
    else  // AC-3
    {
        m_bsidBase = m_bsid;  // id except AC3+ frames
        m_samples = AC3_FRAME_SIZE;
        gbc.skipBits(16);  // m_crc1
        m_fscod = gbc.getBits(2);
        if (m_fscod == 3)
            return AC3ParseError::SAMPLE_RATE;

        m_frmsizecod = gbc.getBits(6);
        if (m_frmsizecod > 37)
            return AC3ParseError::FRAME_SIZE;

        gbc.skipBits(5);  // skip bsid, already got it

        m_bsmod = gbc.getBits(3);
        m_acmod = gbc.getBits(3);
        if ((m_acmod & 1) && m_acmod != AC3_ACMOD_MONO)  // 3 front channels
            gbc.skipBits(2);                             // m_cmixlev
        if (m_acmod & 4)                                 // surround channel exists
            gbc.skipBits(2);                             // m_surmixlev
        if (m_acmod == AC3_ACMOD_STEREO)
            m_dsurmod = gbc.getBits(2);
        m_lfeon = gbc.getBit();

        m_halfratecod = max(m_bsid, 8) - 8;
        m_sample_rate = ff_ac3_freqs[m_fscod] >> m_halfratecod;
        m_bit_rate = (ff_ac3_bitratetab[m_frmsizecod >> 1] * 1000) >> m_halfratecod;
        m_channels = ff_ac3_channels[m_acmod] + m_lfeon;
        m_frame_size = ff_ac3_frame_sizes[m_frmsizecod][m_fscod] * 2;
    }
    return AC3ParseError::NO_ERROR;
}

// returns frame length, or zero (error), or NOT_ENOUGH_BUFFER
int AC3Codec::decodeFrame(uint8_t* buf, uint8_t* end, int& skipBytes)
{
    try
    {
        int rez = 0;
        AC3ParseError err;

        if (end - buf < 2)
            return NOT_ENOUGH_BUFFER;
        if (m_state != AC3State::stateDecodeAC3 && buf[0] == 0x0B && buf[1] == 0x77)
        {
            if (testDecodeTestFrame(buf, end))
                m_state = AC3State::stateDecodeAC3;
        }

        if (m_state == AC3State::stateDecodeAC3)
        {
            skipBytes = 0;
            err = parseHeader(buf, end);

            if (err != AC3ParseError::NO_ERROR)
                return 0;  // parse error

            m_frameDurationNano = (1000000000ull * m_samples) / m_sample_rate;
            rez = m_frame_size;
        }

        if (getTestMode() && !m_true_hd_mode)
        {
            uint8_t* trueHDData = buf + rez;
            if (end - trueHDData < 8)
                return NOT_ENOUGH_BUFFER;
            if (!isSyncWord(trueHDData) && isHDSyncWord(trueHDData + 4))
            {
                if (end - trueHDData < 21)
                    return NOT_ENOUGH_BUFFER;
                m_true_hd_mode = mlp.decodeFrame(trueHDData, trueHDData + 21);
            }
        }

        if ((m_true_hd_mode))  // ommit AC3+
        {
            uint8_t* trueHDData = buf + rez;
            if (end - trueHDData < 7)
                return NOT_ENOUGH_BUFFER;
            if (m_state == AC3State::stateDecodeAC3)
            {
                // check if it is a real HD frame

                if (trueHDData[0] != 0x0B || trueHDData[1] != 0x77 || !testDecodeTestFrame(trueHDData, end))
                {
                    m_waitMoreData = true;
                    m_state = AC3State::stateDecodeTrueHDFirst;
                }
                return rez;
            }
            m_state = AC3State::stateDecodeTrueHD;
            int trueHDFrameLen = (trueHDData[0] & 0x0f) << 8;
            trueHDFrameLen += trueHDData[1];
            trueHDFrameLen *= 2;
            if (end - trueHDData < (int64_t)trueHDFrameLen + 7)
                return NOT_ENOUGH_BUFFER;
            if (!m_true_hd_mode)
            {
                m_true_hd_mode = mlp.decodeFrame(trueHDData, trueHDData + trueHDFrameLen);
            }
            uint8_t* nextFrame = trueHDData + trueHDFrameLen;

            if (nextFrame[0] == 0x0B && nextFrame[1] == 0x77 && testDecodeTestFrame(nextFrame, end))
                m_waitMoreData = false;

            if (m_downconvertToAC3)
            {
                skipBytes = trueHDFrameLen;
                return 0;
            }
            else
            {
                return trueHDFrameLen;
            }
        }
        else
        {
            if (m_downconvertToAC3 && m_bsid > 10)
            {
                skipBytes = rez;  // skip E-AC3 frame
                return 0;
            }
            else
                return rez;
        }
    }
    catch (BitStreamException&)
    {
        return NOT_ENOUGH_BUFFER;
    }
}

AC3Codec::AC3ParseError AC3Codec::testParseHeader(uint8_t* buf, uint8_t* end)
{
    BitStreamReader gbc{};
    gbc.setBuffer(buf, buf + 7);

    int test_sync_word = gbc.getBits(16);
    if (test_sync_word != 0x0B77)
        return AC3ParseError::SYNC;

    // read ahead to bsid to make sure this is AC-3, not E-AC-3
    int test_bsid = gbc.showBits(29) & 0x1F;
    /*
    if (test_bsid != m_bsid)
    return AC3_PARSE_ERROR_SYNC;
    */

    if (test_bsid > 16)
    {
        return AC3ParseError::SYNC;  // invalid stream type
    }
    else if (m_bsid > 10)
    {
        return AC3ParseError::SYNC;  // doesn't used for EAC3
    }
    else
    {
        gbc.skipBits(16);  // test_crc1
        int test_fscod = gbc.getBits(2);
        if (test_fscod == 3)
            return AC3ParseError::SAMPLE_RATE;

        int test_frmsizecod = gbc.getBits(6);
        if (test_frmsizecod > 37)
            return AC3ParseError::FRAME_SIZE;

        gbc.skipBits(5);  // skip bsid, already got it

        int test_bsmod = gbc.getBits(3);
        int test_acmod = gbc.getBits(3);

        if (test_fscod != m_fscod || /*(test_frmsizecod>>1) != (m_frmsizecod>>1) ||*/
            test_bsmod != m_bsmod /*|| test_acmod != m_acmod*/)
            return AC3ParseError::SYNC;

        if ((test_acmod & 1) && test_acmod != AC3_ACMOD_MONO)
            gbc.skipBits(2);  // test_cmixlev

        if (m_acmod & 4)
            gbc.skipBits(2);  // test_surmixlev

        if (m_acmod == AC3_ACMOD_STEREO)
        {
            int test_dsurmod = gbc.getBits(2);
            if (test_dsurmod != m_dsurmod)
                return AC3ParseError::SYNC;
        }
        int test_lfeon = gbc.getBit();

        if (test_lfeon != m_lfeon)
            return AC3ParseError::SYNC;

        int test_halfratecod = max(test_bsid, 8) - 8;
        int test_sample_rate = ff_ac3_freqs[test_fscod] >> test_halfratecod;
        int test_bit_rate = (ff_ac3_bitratetab[test_frmsizecod >> 1] * 1000) >> test_halfratecod;
        int test_channels = ff_ac3_channels[test_acmod] + test_lfeon;
        int test_frame_size = ff_ac3_frame_sizes[test_frmsizecod][test_fscod] * 2;
        if (test_halfratecod != m_halfratecod || test_sample_rate != m_sample_rate || test_bit_rate != m_bit_rate ||
            test_channels != m_channels || test_frame_size != m_frame_size)
            return AC3ParseError::SYNC;
    }
    return AC3ParseError::NO_ERROR;
}

bool AC3Codec::testDecodeTestFrame(uint8_t* buf, uint8_t* end)
{
    return testParseHeader(buf, end) == AC3ParseError::NO_ERROR;
}

uint64_t AC3Codec::getFrameDurationNano()
{
    if (m_bit_rateExt)
        return m_bsid > 10 ? m_frameDurationNano : 0;  // E-AC3. finish frame after AC3 frame
    if (m_waitMoreData)
        return 0;  // AC3 HD

    return m_frameDurationNano;
}

const std::string AC3Codec::getStreamInfo()
{
    std::ostringstream str;
    std::string hd_type;
    if (mlp.m_subType == MlpSubType::stTRUEHD)
        hd_type = "TRUE-HD";
    else if (mlp.m_subType == MlpSubType::stMLP)
        hd_type = "MLP";
    else
        hd_type = "UNKNOWN";

    if (m_true_hd_mode)
    {
        if (isEAC3())
            str << "E-";
        str << "AC3 core+";
        str << hd_type;
        str << ". ";

        str << "Peak bitrate: " << mlp.m_bitrate / 1000 << "Kbps (core " << m_bit_rate / 1000 << "Kbps) ";
        str << "Sample Rate: " << mlp.m_samplerate / 1000 << "KHz ";
        if (m_sample_rate != mlp.m_samplerate)
            str << " (core " << m_sample_rate / 1000 << "Khz) ";
    }
    else
    {
        str << "Bitrate: " << (m_bit_rate + m_bit_rateExt) / 1000 << "Kbps ";
        if (m_bit_rateExt)
            str << "(core " << m_bit_rate / 1000 << "Kbps) ";
        str << "Sample Rate: " << m_sample_rate / 1000 << "KHz ";
    }

    str << "Channels: ";
    int channels = m_channels;
    if (m_extChannelsExists)
        channels += 2;
    if (mlp.m_channels)
        channels = mlp.m_channels;

    if (m_lfeon)
        str << (int)(channels - 1) << ".1";
    else
        str << (int)channels;
    return str.str();
}

uint8_t* AC3Codec::findFrame(uint8_t* buffer, uint8_t* end)
{
    if (buffer == 0)
        return 0;
    uint8_t* curBuf = buffer;
    while (curBuf < end - 1)
    {
        if (*curBuf == 0x0B && curBuf[1] == 0x77)
            return curBuf;
        else
            curBuf++;
    }
    return 0;
}
