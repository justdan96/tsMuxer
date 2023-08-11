#ifndef BUFFERED_FILE_READER_H_
#define BUFFERED_FILE_READER_H_

#include <fs/file.h>
#include <fs/systemlog.h>
#include <types/types.h>

#include "BufferedReader.h"
#include "tsPacket.h"
#include "vodCoreException.h"

// Used to automatically switch to reading the next file in a given list
class FileListIterator : public FileNameIterator
{
   public:
    FileListIterator() : m_index(0) {}
    ~FileListIterator() override = default;

    std::string getNextName() override
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
    size_t m_index;
};

struct FileReaderData : public ReaderData
{
    typedef ReaderData base_class;

   public:
    FileReaderData(uint32_t blockSize, uint32_t allocSize) : m_fileHeaderSize(0) {}

    ~FileReaderData() override = default;

    uint32_t readBlock(uint8_t* buffer, const int max_size) override { return m_file.read(buffer, max_size); }

    bool openStream() override;
    bool closeStream() override { return m_file.close(); }
    bool incSeek(const int64_t offset) override
    {
        return m_file.seek(offset, File::SeekMethod::smCurrent) != static_cast<uint64_t>(-1);
    }

   public:
    File m_file;
    uint32_t m_fileHeaderSize;
};

class BufferedFileReader : public BufferedReader
{
   public:
    BufferedFileReader(uint32_t blockSize, uint32_t allocSize = 0, uint32_t prereadThreshold = 0);

    bool openStream(uint32_t readerID, const char* streamName, int pid = 0,
                    const CodecInfo* codecInfo = nullptr) override;
    bool gotoByte(uint32_t readerID, uint64_t seekDist) override;

   protected:
    ReaderData* intCreateReader() override { return new FileReaderData(m_blockSize, m_allocSize); }
};

#endif
