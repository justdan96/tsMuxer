#ifndef __SINGLE_FILE_MUXER_H
#define __SINGLE_FILE_MUXER_H

#include <types/types.h>

#include "abstractMuxer.h"
#include "avPacket.h"

class SingleFileMuxer : public AbstractMuxer
{
   public:
    SingleFileMuxer(MuxerManager* owner);
    ~SingleFileMuxer() override;
    bool muxPacket(AVPacket& avPacket) override;
    void intAddStream(const std::string& streamName, const std::string& codecName, int streamIndex,
                      const std::map<std::string, std::string>& params, AbstractStreamReader* codecReader) override;
    bool doFlush() override;
    bool close() override;
    void openDstFile() override;

   protected:
    void parseMuxOpt(const std::string& opts) override;

   private:
    const static int ADD_DATA_SIZE = 2048;
    struct StreamInfo
    {
        File m_file;
        std::string m_fileName;
        int64_t m_dts;
        int64_t m_pts;
        uint8_t* m_buffer;
        int m_part;
        int m_bufLen;
        uint64_t m_totalWrited;
        AbstractStreamReader* m_codecReader;
        StreamInfo(int blockSize)
        {
            m_buffer = new uint8_t[blockSize + MAX_AV_PACKET_SIZE +
                                   ADD_DATA_SIZE];  // reserv extra ADD_DATA_SIZE bytes for stream additional data
            m_bufLen = 0;
            m_dts = -1;
            m_pts = -1;
            m_codecReader = 0;
            m_totalWrited = 0;
            m_part = 1;
        }
        ~StreamInfo() { delete[] m_buffer; }
    };
    int m_lastIndex;
    std::map<std::string, int> m_trackNameTmp;
    // std::map<int, std::string> m_fileNames;
    // std::map<int, File> m_file;
    std::map<int, StreamInfo*> m_streamInfo;
    void writeOutBuffer(StreamInfo* streamInfo);
};

class SingleFileMuxerFactory : public AbstractMuxerFactory
{
   public:
    AbstractMuxer* newInstance(MuxerManager* owner) const override { return new SingleFileMuxer(owner); }
};

#endif
