#ifndef __VC1_STREAM_READER_H
#define __VC1_STREAM_READER_H

#include "mpegStreamReader.h"
#include "vc1Parser.h"
#include "avCodecs.h"

class VC1StreamReader: public MPEGStreamReader {
public:
	VC1StreamReader(): MPEGStreamReader() {
		m_isFirstFrame = true;
		m_spsFound = 0;
		m_frame.pict_type = -1;
		m_lastIFrame = false;
		m_decodedAfterSeq = false;
		m_firstFileFrame = false;
		m_prevDtsInc = false;
		m_longCodesAllowed = false;
		m_nextFrameAddr = 0;
	}
	virtual ~VC1StreamReader() {}
	virtual int getTSDescriptor(uint8_t* dstBuff);
	virtual CheckStreamRez checkStream(uint8_t* buffer, int len);
    virtual bool skipNal(uint8_t* nal) override;
    virtual bool needSPSForSplit() const override { return true; }
protected:
	virtual const CodecInfo& getCodecInfo() {return vc1CodecInfo;};
	virtual int intDecodeNAL(uint8_t* buff);
	virtual void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen);
	virtual void writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket);
	virtual int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket, PriorityDataInfo* priorityData);
	virtual double getStreamFPS(void * curNalUnit) { return m_sequence.getFPS();};
	virtual int getStreamWidth() const {return m_sequence.coded_width;}
	virtual int getStreamHeight() const {return m_sequence.coded_height;}
	virtual bool getInterlaced() {return m_sequence.interlace;}
	virtual bool isIFrame() {return m_lastIFrame;}
	virtual void onSplitEvent() {
		m_firstFileFrame = true;
	}
private:
    int decodeFrame(uint8_t* buff);
    int decodeEntryPoint(uint8_t* buff);
    int decodeSeqHeader(uint8_t* buff);
private:
	int64_t m_prevDtsInc;
	bool m_lastIFrame;
	std::vector<uint8_t> m_entryPointBuffer;
	std::vector<uint8_t> m_seqBuffer;
	bool m_isFirstFrame;
	int m_spsFound;
	VC1SequenceHeader m_sequence;
	VC1Frame m_frame;
	int getNextBFrames(uint8_t* buffer, int64_t& bTiming);
	uint8_t* findNextFrame(uint8_t* buffer);
	bool m_firstFileFrame;
	bool m_decodedAfterSeq;
	uint8_t* m_nextFrameAddr;
};

#endif
