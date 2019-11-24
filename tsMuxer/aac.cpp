#include "aac.h"
#include "bitStream.h"
#include "vod_common.h"




// convert AAC prifle to mpeg 4 object type.

/*
const int AACCodec::object_type[9] = {1, // 'AAC_MAIN' -> AAC Main
								   2, // 'AAC_LC' -> AAC Low complexity
								   3, // 'AAC_SSR' -> AAC SSR
								   4, // 'AAC_LTP' -> AAC Long term prediction
								   5, // HE_AAC
								   17, // ER_LC   
								   19, // ER_LTP
								   23, // LD
								   27 //DRM_ER_LC
                                  };  
*/

const int AACCodec::aac_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

const int AACCodec::aac_channels[8] = {
    0, 1, 2, 3, 4, 5, 6, 8
};

uint8_t* AACCodec::findAacFrame(uint8_t* buffer, uint8_t* end)
{
	uint8_t* curBuf = buffer;
	while (curBuf < end) 
	{
		if (*curBuf < 0xf0)
			curBuf += 2;
		else if (*curBuf == 0xff && curBuf < end-1 && (curBuf[1]&0xf6) == 0xf0)
			return curBuf;
		else if ((*curBuf&0xf6) == 0xf0 && curBuf > buffer && curBuf[-1] == 0xff)
			return curBuf-1;
		else
			curBuf++;
	}
	return 0;
}

int AACCodec::getFrameSize(uint8_t* buffer)
{
	return ((buffer[3] & 0x03) << 11) + (buffer[4] << 3) + (buffer[5] >> 5);
}

bool AACCodec::decodeFrame(uint8_t* buffer, uint8_t* end)
{
	BitStreamReader bits;
	try {
		bits.setBuffer(buffer, end);
		int synh = bits.getBits(12);

		if(synh != 0xfff)
			return false;
		m_id = bits.getBit();          /* 0: MPEG-4, 1: MPEG-2*/
		m_layer = bits.getBits(2);        /* layer */
		int protection_absent = bits.getBit();          /* protection_absent */
		// -- 16 bit
		m_profile = bits.getBits(2);        /* profile_objecttype */
		m_sample_rates_index = bits.getBits(4);    /* sample_frequency_index */
		if(!aac_sample_rates[m_sample_rates_index])
			return false;
		bits.skipBit();          /* private_bit */
		m_channels_index = bits.getBits(3);    /* channel_configuration */
		if(!aac_channels[m_channels_index])
			return false;
		bits.skipBit();          /* original/copy */
		bits.skipBit();          /* home */
		//if (m_id == 0) // this is MPEG4
		//    bits.skipBits(2);   // emphasis      

		/* adts_variable_header */
		bits.skipBit();          /* copyright_identification_bit */
		bits.skipBit();          /* copyright_identification_start */
		// -- 32 bit getted
		int frameSize = bits.getBits(13) >> 2; /* aac_frame_length */
		//LTRACE(LT_DEBUG, 0, "decodec frame size: " << m_size);
		int adts_buffer_fullness = bits.getBits(11);       /* adts_buffer_fullness */
		m_rdb = bits.getBits(2);   /* number_of_raw_data_blocks_in_frame */

		m_channels = aac_channels[m_channels_index];
		m_sample_rate = aac_sample_rates[m_sample_rates_index];
		m_samples = (m_rdb + 1) * 1024;
		m_bit_rate = frameSize * 8 * m_sample_rate / m_samples; 
		return true;
	} catch(BitStreamException) {
		return NOT_ENOUGHT_BUFFER;
	}
}

void AACCodec::buildADTSHeader(uint8_t* buffer, int frameSize)
{
	BitStreamWriter writer;
	writer.setBuffer(buffer, buffer + AAC_HEADER_LEN);
	writer.putBits(12, 0xfff);
	writer.putBit(m_id);
	writer.putBits(2, m_layer);
	writer.putBit(1); // protection_absent
	writer.putBits(2, m_profile);
	m_sample_rates_index = 0;
	for (int i = 0; i < sizeof(aac_sample_rates); i++)
		if (aac_sample_rates[i] == m_sample_rate) {
			m_sample_rates_index = i;
			break;
		}
	writer.putBits(4, m_sample_rates_index);
	writer.putBit(0); /* private_bit */
	m_channels_index = 0;
	for (int i = 0; i < sizeof(aac_channels); i++)
		if (aac_channels[i] == m_channels) {
			m_channels_index = i;
			break;
		}
	writer.putBits(3, m_channels_index);

	writer.putBit(0); /* original/copy */
	writer.putBit(0); /* home */

	//if (m_id == 0) // this is MPEG4
	//    writer.putBits(2,0);   // emphasis      

    /* adts_variable_header */
    writer.putBit(0);          /* copyright_identification_bit */
    writer.putBit(0);          /* copyright_identification_start */

	writer.putBits(13,frameSize); // /* aac_frame_length */
	writer.putBits(11, 2047); // /* adts_buffer_fullness */
	writer.putBits(2, m_rdb); /* number_of_raw_data_blocks_in_frame */
	writer.flushBits();
}

void AACCodec::readConfig(uint8_t* buff, int size)
{
	BitStreamReader reader;
	reader.setBuffer(buff, buff + size);
	int object_type = reader.getBits(5);
	if (object_type == 31)
		object_type = 32 + reader.getBits(6);
	m_profile = (object_type & 0x3)- 1;
    m_sample_rates_index = reader.getBits(4);
	m_sample_rate = m_sample_rates_index == 0x0f ? reader.getBits(24) : aac_sample_rates[m_sample_rates_index];
	m_channels_index = reader.getBits(4);
	m_channels = aac_channels[m_channels_index];
    //return specific_config_bitindex; 
}
