#include "bufferedReaderManager.h"

#include <climits>

using namespace std;

BufferedReaderManager::BufferedReaderManager(const uint32_t readersCnt, const uint32_t blockSize, const uint32_t allocSize,
                                             const uint32_t prereadThreshold)
{
    init(blockSize, allocSize, prereadThreshold);

    for (uint32_t i = 0; i < readersCnt; i++)
    {
        BufferedReader* reader = new BufferedFileReader(m_blockSize, m_allocSize, m_prereadThreshold);
        reader->setId(i);
        m_fileReaders.push_back(reader);
    }

    m_readersCnt = readersCnt;
}

void BufferedReaderManager::init(const uint32_t blockSize, const uint32_t allocSize, const uint32_t prereadThreshold)
{
    m_blockSize = blockSize > 0 ? blockSize : DEFAULT_FILE_BLOCK_SIZE;
    m_allocSize = allocSize > 0 ? allocSize : m_blockSize + MAX_AV_PACKET_SIZE;
    m_prereadThreshold = prereadThreshold > 0 ? prereadThreshold : m_blockSize / 2;
}

BufferedReaderManager::~BufferedReaderManager()
{
    for (const auto& m_fileReader : m_fileReaders)
    {
        delete m_fileReader;  // need to define destruction order first. This object MUST be deleted after
                              // MCVodStreamer
    }
}

AbstractReader* BufferedReaderManager::getReader(const char* streamName) const
{
    uint32_t minReaderCnt = UINT_MAX;
    uint32_t minReaderIndex = 0;

    for (uint32_t i = 0; i < m_readersCnt; i++)
    {
        if (m_fileReaders[i]->getReaderCount() < minReaderCnt)
        {
            minReaderCnt = m_fileReaders[i]->getReaderCount();
            minReaderIndex = i;
        }
    }
    return m_fileReaders[minReaderIndex];
}
