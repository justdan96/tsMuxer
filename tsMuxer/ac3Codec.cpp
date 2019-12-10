#include <sstream>
#include "bitStream.h"
#include "avCodecs.h"
#include "vodCoreException.h"
#include "ac3Codec.h"
#include "vod_common.h"



#define max(a,b) (a>b ? a : b)
static const int  NB_BLOCKS = 6; // number of PCM blocks inside an AC3 frame 
static const int  AC3_FRAME_SIZE = NB_BLOCKS * 256;

const static int TRUE_HD_MAJOR_SYNC = 0xF8726FBA; // 0xF8726FBB for DTS-HD
static const int HD_SYNC_WORLD = 0xf8726f;


bool isSyncWorld(uint8_t* buff)
{
    return buff[0] == 0x0B && buff[1] == 0x77;
}

bool isHDSyncWorld(uint8_t* buff)
{
    return buff[0] == 0xf8 && buff[1] == 0x72 && buff[2] == 0x6f;
}

static const uint8_t eac3_blocks[4] = {
    1, 2, 3, 6
}; 

const uint8_t ff_ac3_channels[8] = {
    2, 1, 2, 3, 3, 4, 4, 5
}; 

// possible frequencies 
const uint16_t ff_ac3_freqs[3] = { 48000, 44100, 32000 };

// possible bitrates 
const uint16_t ff_ac3_bitratetab[19] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128,
    160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

const uint16_t ff_ac3_frame_sizes[38][3] = {
    { 64,   69,   96   },
    { 64,   70,   96   },
    { 80,   87,   120  },
    { 80,   88,   120  },
    { 96,   104,  144  },
    { 96,   105,  144  },
    { 112,  121,  168  },
    { 112,  122,  168  },
    { 128,  139,  192  },
    { 128,  140,  192  },
    { 160,  174,  240  },
    { 160,  175,  240  },
    { 192,  208,  288  },
    { 192,  209,  288  },
    { 224,  243,  336  },
    { 224,  244,  336  },
    { 256,  278,  384  },
    { 256,  279,  384  },
    { 320,  348,  480  },
    { 320,  349,  480  },
    { 384,  417,  576  },
    { 384,  418,  576  },
    { 448,  487,  672  },
    { 448,  488,  672  },
    { 512,  557,  768  },
    { 512,  558,  768  },
    { 640,  696,  960  },
    { 640,  697,  960  },
    { 768,  835,  1152 },
    { 768,  836,  1152 },
    { 896,  975,  1344 },
    { 896,  976,  1344 },
    { 1024, 1114, 1536 },
    { 1024, 1115, 1536 },
    { 1152, 1253, 1728 },
    { 1152, 1254, 1728 },
    { 1280, 1393, 1920 },
    { 1280, 1394, 1920 },
}; 

typedef enum {
    AC3_PARSE_ERROR_SYNC        = -1,
    AC3_PARSE_ERROR_BSID        = -2,
    AC3_PARSE_ERROR_SAMPLE_RATE = -3,
    AC3_PARSE_ERROR_FRAME_SIZE  = -4,
} AC3ParseError;

typedef enum {
    AC3_ACMOD_DUALMONO = 0,
    AC3_ACMOD_MONO,
    AC3_ACMOD_STEREO,
    AC3_ACMOD_3F,
    AC3_ACMOD_2F1R,
    AC3_ACMOD_3F1R,
    AC3_ACMOD_2F2R,
    AC3_ACMOD_3F2R
} AC3ChannelMode;

static const uint8_t mlp_quants[16] = {
    16, 20, 24, 0, 0, 0, 0, 0,
     0,  0,  0, 0, 0, 0, 0, 0,
};

static const uint8_t mlp_channels[32] = {
    1, 2, 3, 4, 3, 4, 5, 3, 4, 5, 4, 5, 6, 4, 5, 4,
    5, 6, 5, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t thd_chancount[13] = {
//  LR    C   LFE  LRs LRvh  LRc LRrs  Cs   Ts  LRsd  LRw  Cvh  LFE2
     2,   1,   1,   2,   2,   2,   2,   1,   1,   2,   2,   1,   1
};

static int mlp_samplerate(int in)
{
    if (in == 0xF)
        return 0;

    return (in & 8 ? 44100 : 48000) << (in & 7) ;
}

static int truehd_channels(int chanmap)
{
    int channels = 0, i;

    for (i = 0; i < 13; i++)
        channels += thd_chancount[i] * ((chanmap >> i) & 1);

    return channels;
} 

const static int AC3_DESCRIPTOR_TAG = 0x6A;
const static int EAC3_DESCRIPTOR_TAG = 0x7A;

const CodecInfo& AC3Codec::getCodecInfo() {
	if (m_true_hd_mode)
		return trueHDCodecInfo;
	if (isEAC3())
		return eac3CodecInfo;
	else
		return ac3CodecInfo;
}


int AC3Codec::parseHeader(uint8_t* buf, uint8_t* end)
{
    BitStreamReader gbc;
	gbc.setBuffer(buf, end);


	m_sync_word = gbc.getBits(16);
    if(m_sync_word != 0x0B77)
        return AC3_PARSE_ERROR_SYNC;

    // read ahead to bsid to make sure this is AC-3, not E-AC-3 
    int id = gbc.showBits(29) & 0x1F;
    if(id > 16)
        return AC3_PARSE_ERROR_BSID;
    
    m_bsid = id;
    if(m_bsid > 10) // bsid = 16 => EAC3
    {
        unsigned int numblkscod, strmtyp, substreamid;

        strmtyp = gbc.getBits( 2);
        if (strmtyp == 3)
            return AC3_PARSE_ERROR_SYNC; // invalid stream type

        substreamid = gbc.getBits(3);

        m_frame_size = (gbc.getBits( 11) + 1) * 2;
        if (m_frame_size < AC3_HEADER_SIZE)
            return 0; // invalid header size

        m_fscod = gbc.getBits( 2);

        if (m_fscod == 3) {
            m_fscod2 = gbc.getBits( 2);
            numblkscod = 3;
            if(m_fscod2 == 3)
                return AC3_PARSE_ERROR_SYNC;

            m_sample_rate = ff_ac3_freqs[m_fscod2] / 2;
        } else {
            numblkscod = gbc.getBits( 2);
            m_sample_rate = ff_ac3_freqs[m_fscod];
        }
        m_bsmod = 0;
        m_acmod = gbc.getBits( 3);
        m_lfeon = gbc.getBit();
        m_channels = ff_ac3_channels[m_acmod] + m_lfeon;
        m_samples = eac3_blocks[numblkscod] * 256;
        m_bit_rateExt = m_frame_size * (m_sample_rate) * 8 / (m_samples);
        m_bit_rate = 0;

        gbc.skipBits(5); // skip bsid, already got it
        m_bsidBase = m_bsid;
        for (int i = 0; i < (m_acmod ? 1 : 2); i++) {
            gbc.skipBits(5); // skip dialog normalization
            if (gbc.getBit()) {
                gbc.skipBits(8); //skip Compression gain word
            }
        }

        if (strmtyp == 1) 
        {
            if (gbc.getBit())
            {
                int chanLayout = gbc.getBits(16);
                if (chanLayout & 0x7fe0) // mask standart 5.1 channels
                    m_extChannelsExists = true;
            }
        }
    }
    else // AC-3
    {
        m_bsidBase = m_bsid; // id except AC3+ frames
        m_samples = AC3_FRAME_SIZE;
        m_crc1 = gbc.getBits(16);
        m_fscod = gbc.getBits( 2);
        if(m_fscod == 3)
            return AC3_PARSE_ERROR_SAMPLE_RATE;

        m_frmsizecod = gbc.getBits( 6);
        if(m_frmsizecod > 37)
            return AC3_PARSE_ERROR_FRAME_SIZE;

	    gbc.skipBits(5); // skip bsid, already got it

        m_bsmod = gbc.getBits( 3);
        m_acmod = gbc.getBits( 3);
        if((m_acmod & 1) && m_acmod != AC3_ACMOD_MONO) {
            m_cmixlev = gbc.getBits( 2);
        }
        if(m_acmod & 4) {
            m_surmixlev = gbc.getBits( 2);
        }
        if(m_acmod == AC3_ACMOD_STEREO) {
            m_dsurmod = gbc.getBits( 2);
        }
        m_lfeon = gbc.getBit();

        m_halfratecod = max(m_bsid, 8) - 8;
        m_sample_rate = ff_ac3_freqs[m_fscod] >> m_halfratecod;
        m_bit_rate = (ff_ac3_bitratetab[m_frmsizecod>>1] * 1000) >> m_halfratecod;
        m_channels = ff_ac3_channels[m_acmod] + m_lfeon;
        m_frame_size = ff_ac3_frame_sizes[m_frmsizecod][m_fscod] * 2;
    }
    return 0;
} 

bool AC3Codec::findMajorSync(uint8_t* buffer, uint8_t* end)
{
	uint32_t majorSyncVal = my_htonl(TRUE_HD_MAJOR_SYNC);
	for (uint8_t* cur = buffer; cur < end - 4; cur++) {
		uint32_t* testVal = (uint32_t*) cur;
		if (*testVal == majorSyncVal)
			return true;
	}
	return false;
}

bool AC3Codec::decodeDtsHdFrame(uint8_t* buffer, uint8_t* end)
{
	if (end - buffer < 21)
		return false;
	int ratebits = 0;
	BitStreamReader reader;
	reader.setBuffer(buffer + 4, end);
	if (reader.getBits(24) != HD_SYNC_WORLD) /* Sync words */
        return false;
    mh.stream_type = reader.getBits(8);

	mh.subType = MLPHeaderInfo::stUnknown;
    if (mh.stream_type == 0xbb) {
		mh.subType = MLPHeaderInfo::stMLP;
        mh.group1_bits = mlp_quants[reader.getBits(4)];
        mh.group2_bits = mlp_quants[reader.getBits(4)];
        ratebits = reader.getBits(4);
        mh.group1_samplerate = mlp_samplerate(ratebits);
        mh.group2_samplerate = mlp_samplerate(reader.getBits(4));
        reader.skipBits(11);
        mh.channels_mlp = reader.getBits(5);
		mh.channels =  mlp_channels[mh.channels_mlp];
    } else if (mh.stream_type == 0xba) {
		mh.subType = MLPHeaderInfo::stTRUEHD;
        mh.group1_bits = 24; // TODO: Is this information actually conveyed anywhere?
        mh.group2_bits = 0;
        ratebits = reader.getBits(4);
        mh.group1_samplerate = mlp_samplerate(ratebits);
        mh.group2_samplerate = 0;
        reader.skipBits(8);
        mh.channels_thd_stream1 = reader.getBits(5);
		reader.skipBits(2);
        mh.channels_thd_stream2 = reader.getBits(13);

		if (mh.channels_thd_stream2)
               mh.channels = truehd_channels(mh.channels_thd_stream2);
         else
               mh.channels = truehd_channels(mh.channels_thd_stream1); 
	} else
        return false;

    mh.access_unit_size = 40 << (ratebits & 7);
    mh.access_unit_size_pow2 = 64 << (ratebits & 7);
	mh.frame_duration_nano = mh.access_unit_size * 1000000000ll / mh.group1_samplerate;

	reader.skipBits(32);
	reader.skipBits(16);

    mh.is_vbr = reader.getBit();
    mh.peak_bitrate = (reader.getBits(15) * mh.group1_samplerate + 8) >> 4;
	mh.num_substreams = reader.getBits(4);
	//for (int i = 0; i < 11; ++i)
	//	reader.skipBits(8);
	//reader.skipBits(4);
    //skip_bits_long(gb, 4 + 11 * 8);
	return true;
}

int AC3Codec::decodeFrame(uint8_t *buf, uint8_t *end, int& skipBytes)
{
	try {
		int rez = 0;
		int err;

		if (end - buf < 2)
			return NOT_ENOUGHT_BUFFER;
		if (m_state != stateDecodeAC3 && buf[0] == 0x0B && buf[1] == 0x77) {
			if (testDecodeTestFrame(buf, end))
				m_state = stateDecodeAC3; 
		}

		if (m_state == stateDecodeAC3)
		{
			skipBytes = 0;
			err = parseHeader(buf, end);

			if(err < 0)
				return 0; // parse error

			m_frameDuration = (1000000000ull * m_samples) / (double) m_sample_rate;
            rez = m_frame_size;
		}

        if (getTestMode() && !m_true_hd_mode)
        {
            uint8_t* trueHDData = buf + rez;
            if (end - trueHDData < 8)
                return NOT_ENOUGHT_BUFFER;
            if (!isSyncWorld(trueHDData) && isHDSyncWorld(trueHDData+4)) 
            {
                if (end - trueHDData < 21)
                    return NOT_ENOUGHT_BUFFER;
                m_true_hd_mode = decodeDtsHdFrame(trueHDData, trueHDData  + 21);
            }
        }

		if ((m_true_hd_mode))  // ommit AC3+
		{
			uint8_t* trueHDData = buf + rez;
			if (end - trueHDData < 7)
				return NOT_ENOUGHT_BUFFER;
			if (m_state == stateDecodeAC3) 
            {
                // check if it is a real HD frame

				if (trueHDData[0] != 0x0B || trueHDData[1] != 0x77 || !testDecodeTestFrame(trueHDData, end)) 
				{
				    uint8_t* tmpNextFrame = findFrame(buf+7, end);
					m_waitMoreData = true;
					m_state = stateDecodeTrueHDFirst; 
				}
				return rez;
			}
			m_state = stateDecodeTrueHD;
			int trueHDFrameLen = (trueHDData[0] & 0x0f) << 8;
			trueHDFrameLen += trueHDData[1];
			trueHDFrameLen *= 2;
			if (end - trueHDData < trueHDFrameLen+7)
				return NOT_ENOUGHT_BUFFER;
			if (!m_true_hd_mode) 
			{
				//m_true_hd_mode |= findMajorSync(trueHDData, trueHDData + trueHDFrameLen);
				m_true_hd_mode = decodeDtsHdFrame(trueHDData, trueHDData  + trueHDFrameLen);
			}
			uint8_t* nextFrame = trueHDData + trueHDFrameLen;

            if (nextFrame[0] == 0x0B && nextFrame[1] == 0x77 && testDecodeTestFrame(nextFrame, end)) 
			//if (isSyncWorld(nextFrame) && !isHDSyncWorld(nextFrame+4)) 
				m_waitMoreData = false;

			if (m_downconvertToAC3) {
				skipBytes = trueHDFrameLen;
				return 0;
			}
			else {
				return trueHDFrameLen;
			}
		}
        else {
            if (m_downconvertToAC3 && m_bsid > 10) {
                skipBytes = rez; // skip E-AC3 frame
                return 0;
            }
            else
			    return rez;
        }
	} catch(BitStreamException&) {
		return NOT_ENOUGHT_BUFFER;
	}
} 

int AC3Codec::testParseHeader(uint8_t* buf, uint8_t* end)
{
    BitStreamReader gbc;
	gbc.setBuffer(buf, buf + 7);

	int test_sync_word = gbc.getBits(16);
    if(test_sync_word != 0x0B77)
        return AC3_PARSE_ERROR_SYNC;

    // read ahead to bsid to make sure this is AC-3, not E-AC-3 
    int test_bsid = gbc.showBits(29) & 0x1F;
	/*
	if (test_bsid != m_bsid)
        return AC3_PARSE_ERROR_SYNC;
	*/

    if (test_bsid > 16) {
        return AC3_PARSE_ERROR_SYNC; // invalid stream type
    }
    else if(m_bsid > 10) 
    {
        return AC3_PARSE_ERROR_SYNC; // doesn't used for EAC3
    }
    else {
        int test_crc1 = gbc.getBits(16);
        int test_fscod = gbc.getBits( 2);
        if(test_fscod == 3)
            return AC3_PARSE_ERROR_SAMPLE_RATE;

        int test_frmsizecod = gbc.getBits( 6);
        if(test_frmsizecod > 37)
            return AC3_PARSE_ERROR_FRAME_SIZE;

	    gbc.skipBits(5); // skip bsid, already got it

        int test_bsmod = gbc.getBits( 3);
        int test_acmod = gbc.getBits( 3);

	    if (test_fscod != m_fscod || /*(test_frmsizecod>>1) != (m_frmsizecod>>1) ||*/
		    test_bsmod != m_bsmod /*|| test_acmod != m_acmod*/)
            return AC3_PARSE_ERROR_SYNC;

        if((test_acmod & 1) && test_acmod != AC3_ACMOD_MONO) {
            int test_cmixlev = gbc.getBits( 2);
        }
        if(m_acmod & 4) {
            int test_surmixlev = gbc.getBits( 2);
        }

        if(m_acmod == AC3_ACMOD_STEREO) {
            int test_dsurmod = gbc.getBits( 2);
		    if (test_dsurmod != m_dsurmod)
	            return AC3_PARSE_ERROR_SYNC;
        }
        int test_lfeon = gbc.getBit();
    	
	    if (test_lfeon != m_lfeon)
		    return AC3_PARSE_ERROR_SYNC;
    	

        int test_halfratecod = max(test_bsid, 8) - 8;
        int test_sample_rate = ff_ac3_freqs[test_fscod] >> test_halfratecod;
        int test_bit_rate = (ff_ac3_bitratetab[test_frmsizecod>>1] * 1000) >> test_halfratecod;
        int test_channels = ff_ac3_channels[test_acmod] + test_lfeon;
        int test_frame_size = ff_ac3_frame_sizes[test_frmsizecod][test_fscod] * 2;
	    if (test_halfratecod != m_halfratecod || test_sample_rate != m_sample_rate ||
		    test_bit_rate != m_bit_rate || test_channels != m_channels /*|| test_frame_size != m_frame_size*/)
		    return AC3_PARSE_ERROR_SYNC;
    }
    return 0;
} 

bool AC3Codec::testDecodeTestFrame(uint8_t *buf, uint8_t *end)
{
    return testParseHeader(buf, end) == 0;
} 

double AC3Codec::getFrameDurationNano() 
{
    if (m_bit_rateExt)
        return m_bsid > 10 ? m_frameDuration : 0; // E-AC3. finish frame after AC3 frame
	else if (m_waitMoreData)
		return 0; // AC3 HD
	else
		return m_frameDuration;
}

const std::string AC3Codec::getStreamInfo()
{
	std::ostringstream str;
	std::string hd_type;
	if (mh.subType == MLPHeaderInfo::stTRUEHD)
		hd_type = "TRUE-HD";
	else if (mh.subType == MLPHeaderInfo::stMLP)
		hd_type = "MLP";
	else
		hd_type = "UNKNOWN";

	if (m_true_hd_mode) {
		if (isEAC3())
			str << "E-";
		str << "AC3 core+";
		str << hd_type;
		str << ". ";
	}
	if (m_true_hd_mode) {
		str << "Peak bitrate: " << mh.peak_bitrate/1000 << "Kbps (core " << m_bit_rate / 1000 << "Kbps) ";
		str << "Sample Rate: " << mh.group1_samplerate/1000 << "KHz ";
		if (m_sample_rate != mh.group1_samplerate)
			str << " (core " << m_sample_rate / 1000 << "Khz) ";
	}
	else {
		str << "Bitrate: " << (m_bit_rate+m_bit_rateExt) / 1000 << "Kbps ";
        if (m_bit_rateExt)
            str << "(core " << m_bit_rate / 1000 << "Kbps) ";
		str << "Sample Rate: " << m_sample_rate/1000 << "KHz ";
	}

	str << "Channels: ";
    int channels = m_channels;
    if (m_extChannelsExists)
        channels += 2;
    if (mh.channels)
        channels = mh.channels;

    if (m_lfeon)
        str << (int) (channels-1) << ".1";
    else
        str << (int) channels;
	return str.str();
}

uint8_t* AC3Codec::findFrame(uint8_t* buffer, uint8_t* end)
{
	if (buffer == 0)
		return 0;
	uint8_t* curBuf = buffer;
	while (curBuf < end-1) 
	{
		if (*curBuf == 0x0B && curBuf[1] == 0x77)
			return curBuf;
		else
			curBuf++;
	}
	return 0;
}

