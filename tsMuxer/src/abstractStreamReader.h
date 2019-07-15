#ifndef __ABSTRACT_STREAM_READER_H
#define __ABSTRACT_STREAM_READER_H

#include <types/types.h>
#include "avPacket.h"
#include "avCodecs.h"
#include "pesPacket.h"
#include <fs/file.h>

// Абстрактный класс для чтения данных кодека из файла
// Исользуется для синхронизации и мьюксинга потоков данных
// PTS и DTS возвращаемого AV пакета измеряется в наносекундах.

const static int PTS_CONST_OFFSET = 0;

//class AbstractStreamReader;

class AbstractStreamReader: public BaseAbstractStreamReader {
public:
	enum ContainerType {ctNone, ctEVOB, ctVOB, ctM2TS, ctTS, ctMKV, ctMOV, ctSUP, ctLPCM, ctSRT, ctMultiH264, ctExternal}; 
	const static int NEED_MORE_DATA = 1;
	const static int INTERNAL_READ_ERROR = 2;
	AbstractStreamReader():  BaseAbstractStreamReader(), m_flags(0), m_timeOffset(0), m_containerType(ctNone), m_demuxMode(false), m_secondary(false) {
	}
	virtual ~AbstractStreamReader() {}
	virtual uint64_t getProcessedSize() = 0;
	virtual void setBuffer(uint8_t* data, int dataLen, bool lastBlock = false) {
		m_curPos = m_buffer = data;
		m_bufEnd = m_buffer + dataLen;
	}
	virtual int getTmpBufferSize() {return MAX_AV_PACKET_SIZE;}
	virtual int readPacket(AVPacket& avPacket) = 0;
	virtual int flushPacket(AVPacket& avPacket) = 0;
	virtual int getTSDescriptor(uint8_t* dstBuff) {return 0;}
	virtual void writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket) {}
	virtual void setStreamIndex(int index) {m_streamIndex = index;}
	int getStreamIndex() const { return m_streamIndex; }
	virtual void setTimeOffset(int64_t offset) {m_timeOffset = offset;}
	unsigned m_flags;
	virtual const CodecInfo& getCodecInfo() = 0; // get codecInfo struct. (CodecID, codec name)
	void setSrcContainerType(ContainerType containerType) { m_containerType = containerType; }
	virtual bool beforeFileCloseEvent(File& file) {return true;}
	virtual void onSplitEvent() {}
	virtual void setDemuxMode(bool value) {m_demuxMode = value;}
    virtual bool isPriorityData(AVPacket* packet) { return false; }
    virtual bool needSPSForSplit() const { return false; }
    bool isSecondary() const { return m_secondary; }
    void setIsSecondary(bool value) { m_secondary = value; }
    void setPipParams(const PIPParams& params) { m_pipParams = params; }
    PIPParams getPipParams() { return m_pipParams; }
protected:
	ContainerType m_containerType;
	int64_t m_timeOffset;
	uint8_t* m_buffer;
	uint8_t* m_curPos;
	uint8_t* m_bufEnd;
	//int m_dataLen;
	int m_streamIndex;
	int m_tmpBufferLen;
	bool m_demuxMode;
    bool m_secondary;

    PIPParams m_pipParams;
};

#endif
