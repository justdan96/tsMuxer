#ifndef TS_DEMUXER_H
#define TS_DEMUXER_H

#include "abstractDemuxer.h"
#include "BufferedReader.h"
#include <map>
#include <string>
#include <set>
#include "tsPacket.h"
#include "aac.h"
#include "bufferedReaderManager.h"

//typedef StreamReaderMap std::map<int, AbstractStreamReader*>;
class TSDemuxer: public AbstractDemuxer 
{
public:
	TSDemuxer(const BufferedReaderManager& readManager, const char* streamName);
	~TSDemuxer() override;
	void openFile(const std::string& streamName) override;
	void readClose() override;
	uint64_t getDemuxedSize() override;
	int simpleDemuxBlock(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, int64_t& discardSize) override;
	void getTrackList(std::map<uint32_t, TrackInfo>& trackList) override;
	int getLastReadRez() override {return m_lastReadRez;};
	void setFileIterator(FileNameIterator* itr) override;
	int64_t getTrackDelay(uint32_t pid ) override 
	{
		if (m_firstPtsTime.find(pid) != m_firstPtsTime.end())
			return (int64_t)((m_firstPtsTime[pid] - (m_firstVideoPTS != -1 ? m_firstVideoPTS : m_firstPTS)) /90.0 + 0.5); // convert to ms
		else
			return 0;
	}
	void setMPLSInfo(const std::vector<MPLSPlayItem>& mplsInfo) { m_mplsInfo = mplsInfo; }
    int64_t getFileDurationNano() const override;
private:
    bool mvcContinueExpected() const;
private:
	int64_t m_firstPCRTime;
	bool m_m2tsHdrDiscarded;
	int m_lastReadRez;
	bool m_m2tsMode;
	int m_scale;
	int m_nptPos;
	const BufferedReaderManager& m_readManager;
	std::string m_streamName;
    std::string m_streamNameLow;
	std::map<int, int64_t> m_lastPesPts;
	std::map<int, int64_t> m_lastPesDts;
	std::map<int, int64_t> m_firstPtsTime;
	std::map<int, int64_t> m_firstDtsTime;
	std::map<int, int64_t> m_fullPtsTime;
	std::map<int, int64_t> m_fullDtsTime;
	AbstractReader* m_bufferedReader;
	int m_readerID;
	uint8_t* m_curPos;
	int m_pmtPid;
	bool m_codecReady;
	bool m_firstCall;
	uint64_t m_readCnt;
	uint64_t m_dataProcessed;
	bool m_notificated;
	TS_program_map_section m_pmt;
	uint8_t m_tmpBuffer[TS_FRAME_SIZE+4];
	int m_tmpBufferLen;
	//int64_t m_firstDTS;
	int64_t m_firstPTS;
	int64_t m_lastPTS;
	int64_t m_prevFileLen;
	int m_curFileNum;
	int64_t m_firstVideoPTS;
	std::vector<MPLSPlayItem> m_mplsInfo;
	int64_t m_lastPCRVal;
    bool m_nonMVCVideoFound;

    // cache to improve speed
    uint8_t m_acceptedPidCache[8192];
    bool m_firstDemuxCall;

	bool isVideoPID(uint32_t streamType);
	bool checkForRealM2ts(uint8_t* buffer, uint8_t* end);
};

#endif
