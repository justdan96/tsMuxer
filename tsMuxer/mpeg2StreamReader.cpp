
#include "mpeg2StreamReader.h"
#include <fs/systemlog.h>
#include "vod_common.h"
#include "vodCoreException.h"
#include "nalUnits.h"
#include "avPacket.h"



int MPEG2StreamReader::getTSDescriptor(uint8_t* dstBuff)
{
	try 
	{
		for (uint8_t* nal = MPEGHeader::findNextMarker(m_buffer, m_bufEnd); nal <= m_bufEnd - 32; 
 			nal = MPEGHeader::findNextMarker(nal+4, m_bufEnd))
		{
			if (nal[3] == SEQ_START_SHORT_CODE) {
				uint8_t* nextNal = MPEGHeader::findNextMarker(nal+4, m_bufEnd);
				m_sequence.deserialize(nal+4, nextNal - nal - 4);
				m_streamAR  = (VideoAspectRatio) m_sequence.aspect_ratio_info;
				//break;
			}
			else if (nal[3] == EXT_START_SHORT_CODE) {
				BitStreamReader bitReader;
				bitReader.setBuffer(nal+4, m_bufEnd);
				int extType = bitReader.getBits(4);
				if (extType == SEQUENCE_EXT) {
					m_sequence.deserializeExtension(bitReader);
				}
			}
		}
	} catch (BitStreamException) {
	}
	return 0;
}

CheckStreamRez MPEG2StreamReader::checkStream(uint8_t* buffer, int len)
{
	CheckStreamRez rez;
	uint8_t* end = buffer + len;
	BitStreamReader bitReader;
	uint8_t* nextNal = 0;
	bool spsFound = false;
	bool iFrameFound = false;
	bool gopFound = false;
	bool sliceFound = false;
	bool seqExtFound = false;
	bool pictureFound = false;
	bool pulldownFound = false;
	int extType = 0;
	MPEGPictureHeader frame(0);
		for (uint8_t* nal = MPEGHeader::findNextMarker(buffer, end); nal <= end - 32; 
			 nal = MPEGHeader::findNextMarker(nal+4, end))
		{
			uint8_t unitType = nal[3];
			try {
				if (unitType >= SLICE_MIN_START_SHORT_CODE && unitType <= SLICE_MAX_START_SHORT_CODE)
					sliceFound = true;
				else
					switch(unitType) {
						case EXT_START_SHORT_CODE:
							bitReader.setBuffer(nal+4, end);
							extType = bitReader.getBits(4);
							if (extType == SEQUENCE_EXT) {
								m_sequence.deserializeExtension(bitReader);
								seqExtFound = true;
							}
							else if (extType == PICTURE_CODING_EXT) {
								frame.deserializeCodingExtension(bitReader);
								pulldownFound |= frame.repeat_first_field > 0;
							}
							break;
						case SEQ_END_SHORT_CODE:
							break;
						case GOP_START_SHORT_CODE:
							gopFound = true;
							break;
						case USER_START_SHORT_CODE:
							break;
						case PICTURE_START_SHORT_CODE:
							if (frame.deserialize(nal+4, end-nal-4) == 0)
								return rez;
							//if (frame.pict_type == PCT_I_FRAME)
							//	iFrameFound = true;
							pictureFound = true;
							break;
						case SEQ_START_SHORT_CODE:
							nextNal = MPEGHeader::findNextMarker(nal+4, end);
							if (m_sequence.deserialize(nal+4, nextNal - nal - 4) == 0)
								return rez;
							m_streamAR  = (VideoAspectRatio) m_sequence.aspect_ratio_info;
							spsFound = true;
							break;
						default:
							return rez;
					}
		} catch(BitStreamException) {
			//return rez;
		}
	}
	if (spsFound && pictureFound /*&& gopFound*/ && sliceFound /*&& seqExtFound*/) {
		rez.codecInfo = mpeg2CodecInfo;
		rez.streamDescr = m_sequence.getStreamDescr();
		if (pulldownFound)
			rez.streamDescr += " (pulldown)";
	}
	return rez;
}

int MPEG2StreamReader::intDecodeNAL(uint8_t* buff)
{
	try {
		int rez = 0;
		uint8_t* nextNal = 0;
		switch(*buff)  
		{
			case SEQ_START_SHORT_CODE:
				rez = processSeqStartCode(buff);
				if (rez != 0)
					return rez;
				nextNal = MPEGHeader::findNextMarker(buff, m_bufEnd)+3;
				while (1) {
					if (nextNal >= m_bufEnd)
						return NOT_ENOUGH_BUFFER;
					switch(*nextNal) 
					{
						case EXT_START_SHORT_CODE:
							rez = processExtStartCode(nextNal);
							if (rez != 0)
								return rez;
							break;
						case GOP_START_SHORT_CODE:
							m_framesAtGop = -1;
							m_lastRef = -1;
							break;
						case PICTURE_START_SHORT_CODE:
							rez = decodePicture(nextNal);
							if (rez == 0) {
								m_lastDecodedPos = nextNal;
							}
							return rez;

					}
					nextNal = MPEGHeader::findNextMarker(nextNal, m_bufEnd)+3;
				}
				break;
			case EXT_START_SHORT_CODE:
				return processExtStartCode(buff);
				break;
			case GOP_START_SHORT_CODE:
				m_framesAtGop = -1;
				m_lastRef = -1;
				break;
			case PICTURE_START_SHORT_CODE:
				rez = decodePicture(buff);
				return rez;
				break;
		}
		return 0;
	} catch(BitStreamException& e) {
		return NOT_ENOUGH_BUFFER;
	}
}

int MPEG2StreamReader::processSeqStartCode(uint8_t* buff)
{
	uint8_t* nextNal = MPEGHeader::findNextMarker(buff, m_bufEnd);
	if (nextNal == m_bufEnd) {
		return NOT_ENOUGH_BUFFER;
	}
	try {
		if (m_sequence.deserialize(buff+1, nextNal - buff - 1) == 0)
			return NALUnit::UNSUPPORTED_PARAM;
		m_streamAR  = (VideoAspectRatio) m_sequence.aspect_ratio_info;
	} catch(BitStreamException) {
		return NOT_ENOUGH_BUFFER;
	}
	int oldSpsLen = 0;
	updateFPS(0, buff, nextNal, oldSpsLen);
	spsFound = true;
	m_lastIFrame = true;
	return 0;
}

int MPEG2StreamReader::processExtStartCode(uint8_t* buff)
{
	BitStreamReader bitReader;
	try {
		bitReader.setBuffer(buff+1, m_bufEnd);
		int extType = bitReader.getBits(4);
		if (extType == SEQUENCE_EXT) {
			m_sequence.deserializeExtension(bitReader);
			m_seqExtFound = true;
		}
		return 0;
	} catch(BitStreamException) {
		return NOT_ENOUGH_BUFFER;
	}
}

int MPEG2StreamReader::decodePicture(uint8_t* buff)
{
	if (!spsFound)
		return NALUnit::SPS_OR_PPS_NOT_READY;

	if (!m_streamMsgPrinted) 
	{
		LTRACE(LT_INFO, 2, "Decoding MPEG2 stream (track " << m_streamIndex << "): " << m_sequence.getStreamDescr());
		m_streamMsgPrinted = true;
	}

	try {
		if (m_frame.deserialize(buff+1, m_bufEnd-buff-1) == 0)
			return NALUnit::UNSUPPORTED_PARAM;
	} catch(BitStreamException) {
		return NOT_ENOUGH_BUFFER;
	}

	m_frame.picture_structure = 0;
	int rez = findFrameExt(buff+1);
	if (rez == NOT_ENOUGH_BUFFER)
		return rez;

	if (m_frame.pict_type == PCT_I_FRAME) {
		m_framesAtGop = -1;
		m_lastRef = -1;
	}

	if (m_frame.ref != m_lastRef) {
		m_framesAtGop++;
		m_lastRef = m_frame.ref;
		m_totalFrameNum++;

		//if (!m_isFirstFrame) 
			//m_curDts += m_pcrIncPerFrame; // not right!
		// more careful handling of top_field_first, repeat_first_field and progressive_frame
		m_curDts += m_prevFrameDelay; 
	
		if (m_frame.repeat_first_field) 
		{
			if (!m_sequence.progressive_sequence)
				m_prevFrameDelay = m_pcrIncPerFrame + m_pcrIncPerField;
			else if (m_frame.top_field_first)
				m_prevFrameDelay = m_pcrIncPerFrame*3;
			else
				m_prevFrameDelay = m_pcrIncPerFrame*2;
		}
		else
			m_prevFrameDelay = m_pcrIncPerFrame;
	
		if (m_removePulldown) 
		{
			checkPulldownSync();
			m_testPulldownDts += m_prevFrameDelay;

			m_prevFrameDelay = m_pcrIncPerFrame;
		}
	}
	m_isFirstFrame = false;
	int refDif = m_frame.ref - m_framesAtGop;
	m_curPts = m_curDts + refDif * m_pcrIncPerFrame;
	m_lastIFrame = m_frame.pict_type == PCT_I_FRAME;
	/*
	LTRACE(LT_INFO, 0, "frame num: " << m_totalFrameNum << " type=" << frameTypeDescr[m_frame.pict_type] << 
		" ps=" << m_frame.progressive_frame << " rf=" << m_frame.repeat_first_field);
	*/
	return 0;
}

int MPEG2StreamReader::findFrameExt(uint8_t* buffer)
{
	
	for(uint8_t* nal = MPEGHeader::findNextMarker(buffer, m_bufEnd); nal < m_bufEnd - 4; 
		         nal = MPEGHeader::findNextMarker(nal+4, m_bufEnd)) 
	{
		if (nal[3] == EXT_START_SHORT_CODE) 
		{
			BitStreamReader bitReader;
			try {
				bitReader.setBuffer(nal+4, m_bufEnd);
				int extType = bitReader.getBits(4);
				if (extType == PICTURE_CODING_EXT) {
					m_frame.deserializeCodingExtension(bitReader);

					if (m_removePulldown) {
						updateBits(bitReader, m_frame.repeat_first_field_bitpos, 1, 0);
						if (m_sequence.progressive_sequence) 
							updateBits(bitReader, m_frame.top_field_first_bitpos, 1, 0);
					}
					return 0;
				}
			} catch(BitStreamException) {
				return NOT_ENOUGH_BUFFER;
			}
		}
		else
			return NALUnit::NOT_FOUND;
	}
	return NOT_ENOUGH_BUFFER;
}


void MPEG2StreamReader::updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen)
{
	m_sequence.setFrameRate(buff + 1, m_fps);
}

void MPEG2StreamReader::updateStreamAR(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen)
{
	if (m_ar != AR_KEEP_DEFAULT)
		m_sequence.setAspectRation(buff + 1, m_ar); // todo: delete this line! debug only
}
