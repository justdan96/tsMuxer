#ifndef __AC3_STREAM_READER_H
#define __AC3_STREAM_READER_H

#include "avPacket.h"
#include "simplePacketizerReader.h"
#include "ac3Codec.h"
#include <deque>
#include "abstractDemuxer.h"

class AC3StreamReader: public SimplePacketizerReader, public AC3Codec
{
public:
	AC3StreamReader(): SimplePacketizerReader(),m_useNewStyleAudioPES(false), m_hdFrameOffset(0), m_thdTsLen(0), m_ac3TsLen(0) {
		m_downconvertToAC3 = m_true_hd_mode = false;
		m_state = stateDecodeAC3;
		m_waitMoreData = false;

        m_thdDemuxWaitAc3 = true; // wait ac3 packet
        m_demuxedTHDSamples = 0;
        m_totalTHDSamples = 0;
        m_nextAc3Time = 0;
        m_thdFrameOffset = 0;
	};
	virtual int getTSDescriptor(uint8_t* dstBuff);
	void setNewStyleAudioPES(bool value) {m_useNewStyleAudioPES = value;}
	virtual void setTestMode(bool value) {AC3Codec::setTestMode(value);}
	virtual int getFreq() {return AC3Codec::m_sample_rate;}
	virtual int getAltFreq() {
		if (m_downconvertToAC3)
			return AC3Codec::m_sample_rate;
		else
			return mh.subType == MLPHeaderInfo::stUnknown ? AC3Codec::m_sample_rate : mh.group1_samplerate;
	}
	virtual int getChannels() {return AC3Codec::m_channels;}
    virtual bool isPriorityData(AVPacket* packet) override;
    virtual bool isIFrame(AVPacket* packet) override { return isPriorityData(packet); }
protected:
	virtual unsigned getHeaderLen() {return AC3Codec::getHeaderLen();}
	virtual int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes) {
		skipBeforeBytes = 0;
		return AC3Codec::decodeFrame(buff, end, skipBytes);
	}
	virtual uint8_t* findFrame(uint8_t* buff, uint8_t* end) {
		return AC3Codec::findFrame(buff, end);
	}
	virtual double getFrameDurationNano(){
		return AC3Codec::getFrameDurationNano();
	}
	virtual const CodecInfo& getCodecInfo(){
		return AC3Codec::getCodecInfo();
	}
	virtual const std::string getStreamInfo() {
		return AC3Codec::getStreamInfo();
	}
	virtual void writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket);


    virtual int readPacket(AVPacket& avPacket) override;
    virtual int flushPacket(AVPacket& avPacket) override;
    int readPacketTHD(AVPacket& avPacket);

    virtual bool needMPLSCorrection() const override;

private:
	bool m_useNewStyleAudioPES;
	int m_hdFrameOffset;
	uint64_t m_thdTsLen;
	uint64_t m_ac3TsLen;

    bool m_thdDemuxWaitAc3;
    
    //std::deque<AVPacket> m_delayedAc3Packet;
    MemoryBlock m_delayedAc3Buffer;
    AVPacket m_delayedAc3Packet;
    //std::deque<AVPacket> m_delayedTHDPacket;
    int m_demuxedTHDSamples;
    int64_t m_totalTHDSamples;
    int64_t m_nextAc3Time;
    int64_t m_thdFrameOffset;
};

#endif
