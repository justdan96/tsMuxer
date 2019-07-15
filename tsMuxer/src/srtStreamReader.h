#ifndef __SRT_STREAM_READER
#define __SRT_STREAM_READER

#include "avPacket.h"
#include "abstractStreamReader.h"
#include "avCodecs.h"
#include <queue>
#include "textSubtitles.h"
#include "psgStreamReader.h"
#include "utf8Converter.h"


class SRTStreamReader: public AbstractStreamReader
{
public:
	SRTStreamReader();
	~SRTStreamReader();
	virtual int readPacket(AVPacket& avPacket);
	virtual int flushPacket(AVPacket& avPacket) {return m_dstSubCodec->flushPacket(avPacket);}
	virtual void setBuffer(uint8_t* data, int dataLen, bool lastBlock = false);
	virtual uint64_t getProcessedSize() {return m_processedSize;}
	CheckStreamRez checkStream(uint8_t* buffer, int len, ContainerType containerType, int containerDataType, int containerStreamIndex); 
	virtual const CodecInfo& getCodecInfo() {return pgsCodecInfo;}
	virtual void setStreamIndex(int index) {
		m_streamIndex = index;
		m_dstSubCodec->setStreamIndex(index);
	}
	virtual void setDemuxMode(bool value) 
	{
		m_demuxMode = value;
		PGSStreamReader* pgsReader = dynamic_cast<PGSStreamReader*> (m_dstSubCodec);
		if (pgsReader)
			pgsReader->setDemuxMode(value);
	}
	void setVideoInfo(int width, int height, double fps) {
		m_srtRender->setVideoInfo(width,height,fps);
		PGSStreamReader* pgsReader = dynamic_cast<PGSStreamReader*> (m_dstSubCodec);
		if (pgsReader)
			pgsReader->setVideoInfo(0, 0, fps);

	}
	void setFont(const text_subtitles::Font& font) {m_srtRender->m_textRender->setFont(font);}
    void setAnimation(const text_subtitles::TextAnimation& animation);
	void setBottomOffset(int offset) {m_srtRender->setBottomOffset(offset);}

protected:
	virtual int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket, PriorityDataInfo* priorityData) {
		return m_dstSubCodec->writeAdditionData(dstBuffer, dstEnd, avPacket, priorityData);
	}
private:
	UtfConverter::SourceFormat m_srcFormat;
	double m_inTime;
	double m_outTime;
	int m_processedSize;
	long m_charSize;
	AbstractStreamReader* m_dstSubCodec;
	text_subtitles::TextToPGSConverter* m_srtRender;
	bool m_lastBlock;
	int parseText(uint8_t* dataStart, int len);
	std::vector<uint8_t> m_tmpBuffer;
	std::queue<std::wstring> m_sourceText;
	std::queue<uint32_t> m_origSize;
	std::wstring m_renderedText;
	long m_splitterOfs; 
	uint16_t m_short_R;
	uint16_t m_short_N;
	uint32_t m_long_R;
	uint32_t m_long_N;
    text_subtitles::TextAnimation m_animation;

	enum {PARSE_FIRST_LINE, PARSE_TIME, PARSE_TEXT} m_state;
	uint8_t* renderNextMessage(uint32_t& renderedLen);
	bool parseTime(const std::wstring& text);
	std::string detectUTF8Lang(uint8_t* buffer, int len);
	bool detectSrcFormat(uint8_t* dataStart, int len, int& prefixLen);
	bool strOnlySpace(std::wstring& str);
};

#endif
