#ifndef __BUFFERED_FILE_READER_H
#define __BUFFERED_FILE_READER_H

#include <memory.h>
#include <types/types.h>
#include <fs/file.h>
#include "tsPacket.h"
#include "BufferedReader.h"
#include <fs/systemlog.h>
#include "vodCoreException.h"

// »спользуетс€ дл€ автоматического перехода на чтение следующего файла по заданному списку
class FileListIterator: public FileNameIterator
{
public:
    FileListIterator(): m_index(0)
    {}
	virtual ~FileListIterator() {}

	virtual std::string getNextName() override
    {
        if (++m_index < m_files.size())
            return m_files[m_index];
        /*
		for (unsigned i = 0; i < m_files.size()-1; i++)
			if (m_files[i] == fileName)
				return m_files[i+1];
        */
        return "";
	}

    void addFile(const std::string& fileName)
    {
        /*
		for(unsigned i = 0; i < m_files.size(); i++)
			if (m_files[i] == fileName)
				THROW(ERR_FILE_EXISTS, "File name " << fileName << " already exists.");
        */
		m_files.push_back(fileName);
	}

private:
	std::vector<std::string> m_files;
    int m_index;
};

struct FileReaderData: public ReaderData 
{
    typedef ReaderData base_class;

public:
    FileReaderData(uint32_t blockSize, uint32_t allocSize):
        m_fileHeaderSize(0)
    {}

    virtual ~FileReaderData() 
    {}

    virtual uint32_t readBlock(uint8_t* buffer, int max_size) 
    {
        int rez = 0;
        rez = m_file.read(buffer, max_size);

        return rez;
    }
	

    virtual bool openStream();
	virtual bool closeStream() { return m_file.close(); }
	virtual bool incSeek ( int64_t offset ) { return m_file.seek(offset, File::smCurrent) != uint64_t(-1); }

public:
	File     m_file;
	uint32_t m_fileHeaderSize;
};

class BufferedFileReader: public BufferedReader
{                                              
public:
	BufferedFileReader (uint32_t blockSize, uint32_t allocSize = 0, uint32_t prereadThreshold = 0);

    virtual bool openStream(uint32_t readerID, const char* streamName, int pid = 0, const CodecInfo* codecInfo = 0);
	virtual bool gotoByte(uint32_t readerID, uint64_t seekDist);
protected:
	ReaderData* intCreateReader() { return new FileReaderData(m_blockSize, m_allocSize); }
};

#endif
