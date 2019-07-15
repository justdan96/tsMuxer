#include <types/types.h>

#ifndef __AAC_CODEC_H
#define __AAC_CODEC_H

const static int AAC_HEADER_LEN = 7;

class AACCodec {
public:
	// convert AAC prifle to mpeg 4 object type.
	//static const int object_type[9];
	const static int OBJECT_AAC_MAIN = 1;
	const static int OBJECT_AAC_LC = 2;
	const static int OBJECT_AAC_SSR = 3;
	const static int OBJECT_AAC_LTP = 4;

	const static int PROFILE_LC = 0;
	const static int PROFILE_MAIN = 1;
	const static int PROFILE_SSR = 2;
	const static int PROFILE_LTP = 3;

	static const int aac_sample_rates[16];
	static const int aac_channels[8];
	AACCodec() {
		m_sample_rate = 48000;
		m_channels = 0;
	}
	uint8_t* findAacFrame(uint8_t* buffer, uint8_t* end);
	int getFrameSize(uint8_t* buffer);
	bool decodeFrame(uint8_t* buffer, uint8_t* end);
	void buildADTSHeader(uint8_t* buffer, int frameSize);
	inline void setSampleRate(int value) {m_sample_rate = value;}
	void readConfig(uint8_t* buff, int size);
public:
    int m_id;
	int m_layer;
	int m_channels;
    int m_sample_rate;
    int m_samples;
    int m_bit_rate;
	int m_sample_rates_index;
	int m_channels_index;
	int m_profile;
    //int m_frameSize
	int m_rdb; // ch, sr;
};

#endif
