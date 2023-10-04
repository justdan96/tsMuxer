#ifndef ABSTRACT_READER_H_
#define ABSTRACT_READER_H_

#include <string>

struct CodecInfo;

typedef std::string translateStreamName(const std::string& streamName);

class AbstractReader
{
   public:
    static constexpr int DATA_EOF = 1;
    static constexpr int DATA_NOT_READY = 2;
    static constexpr int DATA_EOF2 = 3;
    static constexpr int DATA_DELAYED = 4;
    static constexpr int DATA_NOT_READY2 = 5;

    AbstractReader() : m_blockSize(0), m_allocSize(0), m_prereadThreshold(0) {}
    virtual ~AbstractReader() = default;
    virtual uint8_t* readBlock(int readerID, uint32_t& readCnt, int& rez, bool* firstBlockVar = nullptr) = 0;
    virtual bool seek(int readerID, int64_t offset) = 0;
    virtual bool incSeek(int readerID, int64_t offset) = 0;
    virtual void notify(int readerID, uint32_t dataReaded) = 0;
    virtual int createReader(int readBuffOffset = 0) = 0;
    virtual void deleteReader(int readerID) = 0;
    virtual bool openStream(int readerID, const char* streamName, int pid = 0,
                            const CodecInfo* codecInfo = nullptr) = 0;

    virtual void setBlockSize(const uint32_t nBlockSize) { m_blockSize = nBlockSize; }
    virtual void setAllocSize(const uint32_t nAllocSize) { m_allocSize = nAllocSize; }
    virtual void setPreReadThreshold(const uint32_t nPreReadThreshold) { m_prereadThreshold = nPreReadThreshold; }

    virtual uint32_t getPreReadThreshold() { return m_prereadThreshold; }

    virtual bool gotoByte(int readerID, int64_t seekDist) = 0;

   protected:
    uint32_t m_blockSize;
    uint32_t m_allocSize;
    uint32_t m_prereadThreshold;
};

#endif
