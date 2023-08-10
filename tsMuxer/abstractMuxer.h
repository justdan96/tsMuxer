#ifndef ABSTRACT_MUXER_H_
#define ABSTRACT_MUXER_H_

#include <fs/file.h>

#include <map>
#include <string>

#include "metaDemuxer.h"

class MuxerManager;
class BlurayHelper;

class AbstractMuxer
{
   public:
    AbstractMuxer(MuxerManager* owner);
    virtual ~AbstractMuxer() = default;

    virtual void openDstFile() = 0;
    virtual bool doFlush() = 0;
    virtual bool close() = 0;
    virtual void parseMuxOpt(const std::string& opts) = 0;

    virtual void intAddStream(const std::string& streamName, const std::string& codecName, int streamIndex,
                              const std::map<std::string, std::string>& params, AbstractStreamReader* codecReader) = 0;

    virtual bool muxPacket(AVPacket& avPacket) = 0;
    virtual void setFileName(const std::string& fileName, FileFactory* fileFactory);

    virtual void joinToMasterFile() {}
    virtual void setSubMode(AbstractMuxer* mainMuxer, bool flushInterleavedBlock) {}
    virtual void setMasterMode(AbstractMuxer* subMuxer, bool flushInterleavedBlock) {}

    virtual std::string getNextName(std::string curName) { return curName; }

    /*
     * inform writer if new block started, round data by blockRound
     */
    void setBlockMuxMode(int blockSize, int sectorSize);

   protected:
    MuxerManager* m_owner;
    std::string m_origFileName;
    int m_interliaveBlockSize;  // used for SSIF interliaved mode
    int m_sectorSize;
    FileFactory* m_fileFactory;
};

class AbstractMuxerFactory
{
   public:
    AbstractMuxerFactory() = default;
    virtual ~AbstractMuxerFactory() = default;

    virtual AbstractMuxer* newInstance(MuxerManager* owner) const = 0;
};

#endif
