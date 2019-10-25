#ifndef __ABSTRACT_VIDEO_CODER_H
#define __ABSTRACT_VIDEO_CODER_H

#include "scrambledInfo.h"

namespace vodcore
{

class AbstractVideoDecoder {
public:
	AbstractVideoDecoder(int rewindCoeff):m_rewindCoeff(rewindCoeff) {}
	virtual ~AbstractVideoDecoder() {}
	virtual bool processNextBlock(ScrambledInfo& inScrambledInfo,
		                  ScrambledInfo& outScrambledInfo,
						  ScrambledInfo& outBackScrambledInfo,
		                  uint8_t* data, uint32_t dataLen, 
						  uint8_t* outBuff, 
						  uint32_t& outBufLen,
					      uint8_t* rbBuff, uint32_t &rbBufLen,
						  uint64_t lastPTS) = 0;
	virtual void setStreamID(uint8_t streamID) = 0;
	//virtual int getBitRate() = 0;
protected:
	int m_rewindCoeff;
};

}
#endif
