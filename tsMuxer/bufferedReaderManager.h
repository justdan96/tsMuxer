#ifndef BUFFERED_READER_MANAGER_H_
#define BUFFERED_READER_MANAGER_H_

#include <vector>

#include "bufferedFileReader.h"

class BufferedReaderManager
{
   public:
    BufferedReaderManager(uint32_t readersCnt, uint32_t blockSize = 0, uint32_t allocSize = 0,
                          uint32_t prereadThreshold = 0);
    ~BufferedReaderManager();
    AbstractReader* getReader(const char* streamName) const;

    void init(uint32_t blockSize = 0, uint32_t allocSize = 0, uint32_t prereadThreshold = 0);

    uint32_t getBlockSize() const { return m_blockSize; }
    uint32_t getAllocSize() const { return m_allocSize; }
    uint32_t getPreReadThreshold() const { return m_prereadThreshold; }

   private:
    std::vector<BufferedReader*> m_fileReaders;
    uint32_t m_readersCnt;
    uint32_t m_blockSize;
    uint32_t m_allocSize;
    uint32_t m_prereadThreshold;
};

#endif
