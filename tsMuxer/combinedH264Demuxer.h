#ifndef COMBINED_H264_DEMUXER_H
#define COMBINED_H264_DEMUXER_H

#include "abstractDemuxer.h"
#include "BufferedReader.h"
#include <map>
#include <string>
#include <set>
#include "bufferedReaderManager.h"
#include "abstractreader.h"
#include "subTrackFilter.h"

class CombinedH264Reader
{
public:
	CombinedH264Reader();
    virtual ~CombinedH264Reader() {}
protected:
    enum ReadState {ReadState_NeedMoreData, ReadState_Primary, ReadState_Secondary, ReadState_Both};

    int getPrefixLen(const uint8_t* pos, const uint8_t* end);
    void addDataToPrimary(const uint8_t* data, const uint8_t* dataEnd, DemuxedData& demuxedData, int64_t& discardSize);
    void addDataToSecondary(const uint8_t* data, const uint8_t* dataEnd, DemuxedData& demuxedData, int64_t& discardSize);
    ReadState detectStreamByNal(const uint8_t* data, const uint8_t* dataEnd);
    void fillPids(const PIDSet& acceptedPIDs, int pid);
protected:
    bool m_firstDemuxCall;
   
    int m_mvcSPS;
    MemoryBlock m_tmpBuffer;
    ReadState m_state;
    int m_mvcStreamIndex;
    int m_avcStreamIndex;
    int m_demuxedPID;
};

class CombinedH264Demuxer: public AbstractDemuxer, public CombinedH264Reader
{
public:
    CombinedH264Demuxer(const BufferedReaderManager& readManager, const char* streamName);
    ~CombinedH264Demuxer() override;
    void openFile(const std::string& streamName) override;
    void readClose() override;
    uint64_t getDemuxedSize() override;
    int simpleDemuxBlock(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, int64_t& discardSize) override;
    void getTrackList(std::map<uint32_t, TrackInfo>& trackList) override;
    int getLastReadRez() override {return m_lastReadRez; };
    void setFileIterator(FileNameIterator* itr) override;

    bool isPidFilterSupported() const override { return true; }
private:
    const BufferedReaderManager& m_readManager;
    AbstractReader* m_bufferedReader;
    int m_readerID;
    int m_lastReadRez;
    uint64_t m_dataProcessed;
};

class CombinedH264Filter: public SubTrackFilter, public CombinedH264Reader
{
public:
    CombinedH264Filter(int demuxedPID);
    ~CombinedH264Filter() override {}
    int demuxPacket(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, AVPacket& avPacket) override;
};


#endif
