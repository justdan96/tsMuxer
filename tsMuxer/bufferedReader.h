#ifndef BUFFERED_READER_H_
#define BUFFERED_READER_H_

#include <containers/safequeue.h>
#include <system/terminatablethread.h>

#include <map>
#include <string>

#include "abstractDemuxer.h"
#include "abstractreader.h"

struct ReaderData
{
    ReaderData()
        : m_bufferIndex(0),
          m_notified(false),
          m_nextBlockSize(0),
          m_deleted(false),
          m_firstBlock(false),
          m_lastBlock(false),
          m_eof(false),
          m_atQueue(0),
          itr(nullptr),
          m_blockSize(0),
          m_allocSize(0),
          m_readOffset(0)
    {
        m_nextBlock[0] = nullptr;
        m_nextBlock[1] = nullptr;
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
        if (m_nextBlock[0] == nullptr)
            m_nextBlock[0] = new uint8_t[m_allocSize];
        if (m_nextBlock[1] == nullptr)
            m_nextBlock[1] = new uint8_t[m_allocSize];
    }

    virtual bool openStream()
    {
        init();
        return false;
    }

    virtual int readBlock(uint8_t* buffer, uint32_t max_size) = 0;

    virtual bool closeStream() = 0;

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
    static constexpr int UNKNOWN_READERID = 3;
    BufferedReader(uint32_t blockSize, uint32_t allocSize = 0, uint32_t prereadThreshold = 0);
    ~BufferedReader() override;
    int createReader(int readBuffOffset = 0) override;
    void deleteReader(int readerID) override;  // unregister readed
    uint8_t* readBlock(int readerID, uint32_t& readCnt, int& rez, bool* firstBlockVar = nullptr) override;
    void notify(int readerID,
                uint32_t dataReaded) override;  // reader must call notificate when part of data handled
    uint32_t getReaderCount();
    void terminate();
    void setFileIterator(FileNameIterator* itr, int readerID);
    bool incSeek(int readerID, int64_t offset);
    bool gotoByte(int readerID, int64_t seekDist) override { return false; }

    void setId(const uint32_t value) { m_id = value; }

   protected:
    virtual ReaderData* intCreateReader() = 0;
    void thread_main() override;

    bool m_started;
    bool m_terminated;
    WaitableSafeQueue<int> m_readQueue;
    ReaderData* getReader(int readerID);
    std::condition_variable m_readCond;
    std::mutex m_readMtx;

   private:
    uint32_t m_id;
    std::mutex m_readersMtx;
    std::map<int, ReaderData*> m_readers;
    static int m_newReaderID;
    static int createNewReaderID();
    static std::mutex m_genReaderMtx;
};

#endif
