#ifndef __BUFFERED_READER_MANAGER_H
#define __BUFFERED_READER_MANAGER_H

#include <types/types.h>

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

    inline uint32_t getBlockSize() { return m_blockSize; }
    inline uint32_t getAllocSize() { return m_allocSize; }
    inline uint32_t getPreReadThreshold() { return m_prereadThreshold; }

   private:
    std::vector<BufferedReader*> m_fileReaders;
    uint32_t m_readersCnt;
    uint32_t m_blockSize;
    uint32_t m_allocSize;
    uint32_t m_prereadThreshold;
};

#endif
