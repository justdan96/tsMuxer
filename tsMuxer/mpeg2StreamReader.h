#ifndef __MPEG2_STREAM_READER_H
#define __MPEG2_STREAM_READER_H

#include "mpegStreamReader.h"
#include "mpegVideo.h"
#include "avCodecs.h"

class MPEG2StreamReader: public MPEGStreamReader {
public:
	MPEG2StreamReader(): MPEGStreamReader(), m_sequence(0), m_frame(0) {
		m_isFirstFrame = true;
		spsFound = false;
		m_framesAtGop = 0;
		//m_lastFullFrame = true;
		m_lastRef = -1;
		m_seqExtFound = false;
		m_streamMsgPrinted = false;
		m_lastIFrame = false;
		m_longCodesAllowed = false;
		m_prevFrameDelay = 0;
	}
	virtual int getTSDescriptor(uint8_t* dstBuff);
	virtual CheckStreamRez checkStream(uint8_t* buffer, int len);

	virtual int getStreamWidth() const {return m_sequence.width;}
	virtual int getStreamHeight()const {return m_sequence.height;}
	virtual bool getInterlaced() {return !m_sequence.progressive_sequence;}

protected:
	virtual const CodecInfo& getCodecInfo() {return mpeg2CodecInfo;};
	virtual int intDecodeNAL(uint8_t* buff);
	virtual void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen);
	virtual void updateStreamAR(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen);
	virtual double getStreamFPS(void * curNalUnit) { return m_sequence.getFrameRate();};
	virtual bool isIFrame() {return m_lastIFrame;}
private:
	bool m_streamMsgPrinted;
	int m_lastRef;
	//bool m_lastFullFrame;
	int m_framesAtGop;
	bool m_isFirstFrame;
	bool spsFound;
	bool m_seqExtFound;
	bool m_lastIFrame;
	int64_t m_prevFrameDelay;
	MPEGSequenceHeader m_sequence;
	MPEGPictureHeader m_frame;
	int getNextBFrames(uint8_t* buffer);
	int findFrameExt(uint8_t* buffer);
	int decodePicture(uint8_t* buff);
	int processExtStartCode(uint8_t* buff);	
	int processSeqStartCode(uint8_t* buff);
	uint8_t* m_nextFrameAddr;
};

#endif
