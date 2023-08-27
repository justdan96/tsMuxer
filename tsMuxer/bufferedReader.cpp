
#include "bufferedReader.h"

#include <fs/systemlog.h>

#include "abstractreader.h"
#include "vod_common.h"

#ifndef NO_ERROR
#define NO_ERROR 0
#endif

using namespace std;

int BufferedReader::m_newReaderID = 0;
std::mutex BufferedReader::m_genReaderMtx;
static constexpr unsigned QUEUE_MAX_SIZE = 4096;

BufferedReader::BufferedReader(const uint32_t blockSize, const uint32_t allocSize, const uint32_t prereadThreshold)
    : m_started(false), m_terminated(false), m_readQueue(QUEUE_MAX_SIZE), m_id(0)
{
    // size of the blocks being read
    m_blockSize = blockSize;
    // size of the memory reserved per-block (can exceed the block size, in which case the rest of the memory can be
    // used by the user)
    m_allocSize = allocSize ? allocSize : blockSize;
    // data preread threshold
    m_prereadThreshold = prereadThreshold ? prereadThreshold : m_blockSize / 2;
}

ReaderData* BufferedReader::getReader(const int readerID)
{
    std::lock_guard lock(m_readersMtx);
    const auto itr = m_readers.find(readerID);
    return itr != m_readers.end() ? itr->second : nullptr;
}

bool BufferedReader::incSeek(const int readerID, const int64_t offset)
{
    std::lock_guard lock(m_readersMtx);
    const auto itr = m_readers.find(readerID);
    if (itr != m_readers.end())
    {
        ReaderData* data = itr->second;
        const bool rez = data->incSeek(offset);
        if (rez)
        {
            data->m_eof = false;
            data->m_notified = false;
        }
        return rez;
    }
    return false;
}

BufferedReader::~BufferedReader()
{
    terminate();
    m_readQueue.push(0);
    join();
    for (const auto& m_reader : m_readers)
    {
        const ReaderData* pData = m_reader.second;
        delete pData;
    }
}

int BufferedReader::createNewReaderID()
{
    m_genReaderMtx.lock();
    const int rez = ++m_newReaderID;
    m_genReaderMtx.unlock();
    return rez;
}

int BufferedReader::createReader(const int readBuffOffset)
{
    ReaderData* data = intCreateReader();

    data->m_blockSize = m_blockSize;
    data->m_allocSize = m_allocSize;

    data->m_readOffset = readBuffOffset;

    data->m_firstBlock = true;
    data->m_lastBlock = false;

    std::lock_guard lock(m_readersMtx);
    const int newReaderID = createNewReaderID();
    m_readers[newReaderID] = data;
    size_t rSize = m_readers.size();
    if (!m_started)
    {
        run(this);
        m_started = true;
    }
    LTRACE(LT_INFO, 0, "Reader #" << m_id << ". Start new stream " << newReaderID << ". stream(s): " << rSize);

    return newReaderID;
}

void BufferedReader::deleteReader(const int readerID)
{
    {
        std::lock_guard lock(m_readersMtx);
        const auto iterator = m_readers.find(readerID);
        if (iterator == m_readers.end())
            return;
        ReaderData* data = iterator->second;
        if (data->m_atQueue > 0)
            data->m_deleted = true;  // There are requests in the queue for reading into this structure.
        else
        {
            delete iterator->second;  // No outstanding requests for reading in the queue. Delete immediately.
            m_readers.erase(iterator);
        }
    }
}

uint8_t* BufferedReader::readBlock(const int readerID, uint32_t& readCnt, int& rez, bool* firstBlockVar)
{
    ReaderData* data;
    {
        std::lock_guard lock(m_readersMtx);
        const auto itr = m_readers.find(readerID);
        if (itr != m_readers.end())
        {
            data = itr->second;
            if (!data->m_nextBlockSize && !data->m_notified)
            {  // No fragments of this block found, and no requests for reading this block
                data->m_notified = true;
                data->m_atQueue++;
                m_readQueue.push(readerID);
            }
        }
        else
        {
            rez = UNKNOWN_READERID;
            readCnt = 0;
            return nullptr;
        }
    }

    if (!data->m_nextBlockSize)
    {
        std::unique_lock lk(m_readMtx);
        while (data->m_nextBlockSize == 0 && !data->m_eof) m_readCond.wait(lk);
    }
    readCnt = data->m_nextBlockSize >= 0 ? data->m_nextBlockSize : 0;
    rez = data->m_eof ? DATA_EOF : NO_ERROR;
    const uint8_t prevIndex = data->m_bufferIndex;
    data->m_bufferIndex = 1 - data->m_bufferIndex;
    data->m_nextBlockSize = 0;
    data->m_notified = false;
    if (firstBlockVar)
        *firstBlockVar = data->m_firstBlock;
    return data->m_nextBlock[prevIndex];
}

void BufferedReader::terminate()
{
    m_terminated = true;
    // join();
}

void BufferedReader::notify(const int readerID, const uint32_t dataReaded)
{
    ReaderData* data = getReader(readerID);
    if (data == nullptr)
        return;
    if (dataReaded >= m_prereadThreshold && !data->m_notified)
    {
        std::lock_guard lock(m_readersMtx);
        data->m_notified = true;
        data->m_atQueue++;
        m_readQueue.push(readerID);
    }
}

uint32_t BufferedReader::getReaderCount()
{
    std::lock_guard lock(m_readersMtx);
    return static_cast<uint32_t>(m_readers.size());
}

void BufferedReader::thread_main()
{
    try
    {
        while (!m_terminated)
        {
            const int readerID = m_readQueue.pop();
            if (m_terminated)
            {
                break;
            }
            ReaderData* data = getReader(readerID);
            if (data)
            {
                uint8_t* buffer = data->m_nextBlock[data->m_bufferIndex] + data->m_readOffset;
                if (!data->m_deleted)
                {
                    int bytesReaded = data->readBlock(buffer, data->m_blockSize);
                    if (data->m_lastBlock)
                    {
                        data->m_lastBlock = false;
                        data->m_firstBlock = true;
                    }
                    else if (data->m_firstBlock)
                    {
                        data->m_firstBlock = false;
                    }

                    if (bytesReaded <= 0 || (bytesReaded < static_cast<int>(data->m_blockSize) && data->itr))
                    {
                        if (data->itr)
                        {
                            std::string nextFileName = data->itr->getNextName();
                            if (nextFileName != data->m_streamName)
                            {
                                data->closeStream();
                                data->m_streamName = nextFileName;
                                if (!data->m_streamName.empty() && data->openStream())
                                {
                                    if (bytesReaded == 0)
                                    {
                                        // data->m_nextFileInfo = NEXT_FILE_FIRST_BLOCK;
                                        data->m_firstBlock = true;
                                        bytesReaded = data->readBlock(buffer, m_blockSize);
                                        if (bytesReaded < static_cast<int>(m_blockSize))
                                        {
                                            data->m_eof = true;
                                            data->m_lastBlock = true;
                                        }
                                    }
                                    else
                                    {
                                        data->m_lastBlock = true;
                                    }
                                }
                                else
                                    data->m_eof = true;
                            }
                        }
                        else
                        {
                            data->m_eof = true;
                        }
                    }

                    data->m_blockSize = m_blockSize;
                    if (bytesReaded == 0)
                    {
                        data->m_eof = true;
                    }

                    {
                        std::lock_guard lk(m_readMtx);
                        data->m_nextBlockSize = bytesReaded;
                        m_readCond.notify_one();
                    }
                }

                {
                    std::lock_guard lock(m_readersMtx);
                    data->m_atQueue--;
                    if (data->m_deleted && data->m_atQueue == 0)
                    {
                        delete data;
                        m_readers.erase(readerID);
                    }
                }
            }
        }
    }
    catch (std::exception& e)
    {
        LTRACE(LT_ERROR, 0, "BufferedReader::thread_main() throws exception: " << e.what());
    }
    catch (...)
    {
        LTRACE(LT_ERROR, 0, "BufferedReader::thread_main() throws unknown exception");
    }
}

void BufferedReader::setFileIterator(FileNameIterator* itr, const int readerID)
{
    assert(readerID != -1);
    std::lock_guard lock(m_readersMtx);
    const auto reader = m_readers.find(readerID);
    if (reader != m_readers.end())
        reader->second->itr = itr;
}
