#ifndef __BUFFERED_READER_H
#define __BUFFERED_READER_H

#include <containers/safequeue.h>
#include <system/terminatablethread.h>
#include <types/types.h>

#include <map>
#include <string>

#include "abstractDemuxer.h"
#include "abstractreader.h"

struct ReaderData
{
   public:
    ReaderData()
        : m_bufferIndex(0),
          m_notified(false),
          m_nextBlockSize(0),
          m_deleted(false),
          m_eof(false),
          m_atQueue(0),
          itr(0),
          m_readOffset(0)
    {
        m_nextBlock[0] = NULL;
        m_nextBlock[1] = NULL;
    }

    virtual ~ReaderData()
    {
        // deleteNextBlocks();
        delete[] m_nextBlock[0];
        delete[] m_nextBlock[1];
    }
    /*
virtual void deleteNextBlocks()
{
    delete [] m_nextBlock[0];
    delete [] m_nextBlock[1];
}
    */
    virtual bool incSeek(int64_t offset) { return true; }

    virtual void init()
    {
        // deleteNextBlocks();
        if (m_nextBlock[0] == 0)
            m_nextBlock[0] = new uint8_t[m_allocSize];
        if (m_nextBlock[1] == 0)
            m_nextBlock[1] = new uint8_t[m_allocSize];
    }

    virtual bool openStream()
    {
        init();
        return false;
    };

    virtual uint32_t readBlock(uint8_t* buffer, int max_size) = 0;

    virtual bool closeStream() = 0;

   public:
    uint8_t m_bufferIndex;
    bool m_notified;
    int m_nextBlockSize;
    bool m_deleted;
    bool m_firstBlock;
    bool m_lastBlock;
    bool m_eof;
    int m_atQueue;
    FileNameIterator* itr;
    uint8_t* m_nextBlock[2];
    uint32_t m_blockSize;
    uint32_t m_allocSize;
    std::string m_streamName;
    int m_readOffset;
};

class BufferedReader : public AbstractReader, TerminatableThread
{
   public:
    const static int UNKNOWN_READERID = 3;
    BufferedReader(uint32_t blockSize, uint32_t allocSize = 0, uint32_t prereadThreshold = 0);
    ~BufferedReader() override;
    uint32_t createReader(int readBuffOffset = 0) override;
    void deleteReader(uint32_t readerID) override;  // unregister readed
    uint8_t* readBlock(uint32_t readerID, uint32_t& readCnt, int& rez, bool* firstBlockVar = 0) override;
    void notify(uint32_t readerID,
                uint32_t dataReaded) override;  // reader must call notificate when part of data handled
    uint32_t getReaderCount();
    void terminate();
    void setFileIterator(FileNameIterator* itr, int readerID);
    bool incSeek(uint32_t readerID, int64_t offset);
    bool gotoByte(uint32_t readerID, uint64_t seekDist) override { return false; }

    void setId(int value) { m_id = value; }

   protected:
    virtual ReaderData* intCreateReader() = 0;
    void thread_main() override;

   protected:
    bool m_started;
    bool m_terminated;
    WaitableSafeQueue<uint32_t> m_readQueue;
    ReaderData* getReader(uint32_t readerID);
    std::condition_variable m_readCond;
    std::mutex m_readMtx;

   private:
    int m_id;
    std::mutex m_readersMtx;
    std::map<uint32_t, ReaderData*> m_readers;
    static uint32_t m_newReaderID;
    static uint32_t createNewReaderID();
    static std::mutex m_genReaderMtx;
};

#endif
