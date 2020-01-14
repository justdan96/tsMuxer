#ifndef __ABSTRACT_MUXER_H
#define __ABSTRACT_MUXER_H

#include <fs/file.h>
#include <fs/systemlog.h>

#include <map>
#include <string>

#include "bufferedFileWriter.h"
#include "metaDemuxer.h"

class MuxerManager;
class BlurayHelper;

class AbstractMuxer
{
   public:
    AbstractMuxer(MuxerManager* owner);
    virtual ~AbstractMuxer() {}

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

    virtual std::string getNextName(const std::string curName) { return curName; }

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
    AbstractMuxerFactory() {}
    virtual ~AbstractMuxerFactory() {}

    virtual AbstractMuxer* newInstance(MuxerManager* owner) const = 0;
};

#endif
