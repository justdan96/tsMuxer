#include "stdafx.h"

#include "BufferedReader.h"
#include "vod_common.h"
#include <fs/systemlog.h>
#include "abstractreader.h"

#define NO_ERROR 0


using namespace std;

uint32_t BufferedReader::m_newReaderID = 0;
Mutex BufferedReader::m_genReaderMtx;
const unsigned QUEUE_MAX_SIZE = 4096;

BufferedReader::BufferedReader (uint32_t blockSize, uint32_t allocSize, uint32_t prereadThreshold):
    m_started(false),
    m_terminated(false),
    m_readQueue ( QUEUE_MAX_SIZE ),
	m_id(0)
{
    // размер читаемых блоков
    m_blockSize = blockSize; 
    // размер выдел€емой пам€ти под блок (может превышать размер блока, остаток пам€ти используетс€ дл€ нужд пользовател€
    m_allocSize = allocSize ? allocSize : blockSize;
    // ѕорог предчтени€ данных
    m_prereadThreshold = prereadThreshold ? prereadThreshold : m_blockSize / 2;
}

ReaderData* BufferedReader::getReader(uint32_t readerID)
{
	Mutex::ScopedLock lock(&m_readersMtx);
	map<uint32_t, ReaderData*>::iterator itr = m_readers.find(readerID);
	return itr != m_readers.end() ? itr->second : 0;
}

bool BufferedReader::incSeek(uint32_t readerID, int64_t offset)
{
	Mutex::ScopedLock lock(&m_readersMtx);
	map<uint32_t, ReaderData*>::iterator itr = m_readers.find(readerID);
	if (itr != m_readers.end()) {
		ReaderData* data = itr->second;
		bool rez = data->incSeek(offset);
		if (rez) {
			data->m_eof = false;
			data->m_notified = false;
		}
		return rez;
	}
	else
		return false;
}

BufferedReader::~BufferedReader()
{
	terminate();
	m_readQueue.push(0);
	join();
	for (std::map<uint32_t, ReaderData*>::iterator itr = m_readers.begin(); itr != m_readers.end(); ++itr)
    {
        int i = 0;
        ReaderData* pData = itr->second;
        delete pData;
    }
}

uint32_t BufferedReader::createNewReaderID()
{
	m_genReaderMtx.lock();
	uint32_t rez = ++m_newReaderID;
	m_genReaderMtx.unlock();
	return  rez;
}

uint32_t BufferedReader::createReader(int readBuffOffset)
{
	uint32_t newReaderID = 0;
	ReaderData* data = intCreateReader();
    
    data->m_blockSize = m_blockSize;
    data->m_allocSize = m_allocSize;
	
    data->m_readOffset  = readBuffOffset;

	data->m_firstBlock = true;
	data->m_lastBlock = false;
	size_t rSize;
	{
		Mutex::ScopedLock lock(&m_readersMtx);
		newReaderID = createNewReaderID();
		m_readers.insert(make_pair(newReaderID, data));
		rSize = m_readers.size();
		if (!m_started) {
			TerminatableThread::run(this);
			m_started = true;
		}
	}
	LTRACE(LT_INFO , 0 , "Reader #" << m_id << ". Start new stream " << newReaderID << ". stream(s): " << (uint32_t) rSize);
	
	return newReaderID;
}

void BufferedReader::deleteReader(uint32_t readerID)
{
	size_t rSize;
	{
		Mutex::ScopedLock lock(&m_readersMtx); 
		std::map<uint32_t, ReaderData*>::iterator iterator = m_readers.find(readerID);
		if (iterator == m_readers.end())
			return;
		ReaderData* data = iterator->second;
		if (data->m_atQueue > 0)
			data->m_deleted = true; // ¬ очереди есть запросы на чтение в эту структуру.
		else {
			delete iterator->second; // Ќет сто€щих в очереди запросов на чтение. ”дал€ем сразу
			m_readers.erase(iterator);
		}
		rSize = m_readers.size();
	}
	//LTRACE(LT_INFO, 0, "stop stream " << readerID << ". stream(s): " << (uint32_t) rSize);
}

uint8_t* BufferedReader::readBlock(uint32_t readerID, uint32_t& readCnt, int& rez, bool* firstBlockVar)
{
	ReaderData* data = 0;
	{
		Mutex::ScopedLock lock(&m_readersMtx);
		map<uint32_t, ReaderData*>::iterator itr = m_readers.find(readerID);
		if (itr != m_readers.end()) {
			data = itr->second;
			if (!data->m_nextBlockSize && !data->m_notified) { // Ќет начитанного след блока и нет команды на его чтение
				data->m_notified = true;
				data->m_atQueue++;
				m_readQueue.push(readerID);
			}
		}
		else {
			rez = UNKNOWN_READERID;
			readCnt = 0;
			return 0;					
		}
	}

	if (!data->m_nextBlockSize)
	{
		Mutex::ScopedLock lk(&m_readMtx);
		while(data->m_nextBlockSize == 0 && !data->m_eof) 
			m_readCond.wait(&lk);
	}
	readCnt = data->m_nextBlockSize >= 0 ? data->m_nextBlockSize : 0;
	rez     = data->m_eof ? DATA_EOF : NO_ERROR;
	uint8_t prevIndex = data->m_bufferIndex;
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
	//join();
}

void BufferedReader::notify(uint32_t readerID, uint32_t dataReaded)
{
    ReaderData* data = getReader(readerID);
    if (data == 0)
	return;
    if (dataReaded >= m_prereadThreshold && !data->m_notified) {
	Mutex::ScopedLock lock(&m_readersMtx);
	data->m_notified = true;
	data->m_atQueue++;
	m_readQueue.push(readerID);
    }
}								
								
uint32_t BufferedReader::getReaderCount()
{
	Mutex::ScopedLock lock(&m_readersMtx);
	return (uint32_t) m_readers.size();
}

void BufferedReader::thread_main()
{
	try
	{
		while (!m_terminated)
		{

			uint32_t readerID = m_readQueue.pop();
			if (m_terminated)
			{
				break;
			}
			ReaderData* data = getReader(readerID);
			if (data)
			{
				uint8_t* buffer = data->m_nextBlock[data->m_bufferIndex] + data->m_readOffset;
				if (!data->m_deleted) {
					int bytesReaded = data->readBlock ( buffer, data->m_blockSize );
					if (data->m_lastBlock) {
						data->m_lastBlock = false;
						data->m_firstBlock = true;
					}
					else if (data->m_firstBlock) {
						data->m_firstBlock = false;
					}
					
                    if ((bytesReaded < data->m_blockSize && data->itr) || bytesReaded <= 0) 
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
                                        //data->m_nextFileInfo = NEXT_FILE_FIRST_BLOCK;
                                        data->m_firstBlock = true;
                                        bytesReaded = data->readBlock(buffer, m_blockSize);
                                        if (bytesReaded < m_blockSize) {
                                            data->m_eof = true;
                                            data->m_lastBlock = true;
                                        }
                                    }
                                    else {
                                        data->m_lastBlock = true;
                                    }
                                }
                                else 
                                    data->m_eof = true;
                            }
                        }
                        else {
                            data->m_eof = true;
                        }
                    }

					data->m_blockSize = m_blockSize;
					if (bytesReaded == 0) {
						data->m_eof = true;
					}

					{
						Mutex::ScopedLock lk(&m_readMtx);
						data->m_nextBlockSize = bytesReaded;
						m_readCond.notify_one();
					}
				}

				{
					Mutex::ScopedLock lock(&m_readersMtx);
					data->m_atQueue--;
					if (data->m_deleted && data->m_atQueue == 0) {
						delete data;
						m_readers.erase(readerID);
					}
				}
			}
		}
	} catch(std::exception& e) {
		LTRACE(LT_ERROR, 0, "BufferedReader::thread_main() throws exception: " << e.what());
	} catch(...) {
		LTRACE(LT_ERROR, 0, "BufferedReader::thread_main() throws unknown exception");
	}
}


void BufferedReader::setFileIterator(FileNameIterator* itr, int readerID)
{
    assert(readerID != -1);
    Mutex::ScopedLock lock(&m_readersMtx);
    std::map<uint32_t, ReaderData*>::iterator reader = m_readers.find(readerID);
    if (reader != m_readers.end())
        reader->second->itr = itr;
}
