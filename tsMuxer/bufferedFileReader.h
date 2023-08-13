#ifndef BUFFERED_FILE_READER_H_
#define BUFFERED_FILE_READER_H_

#include <fs/file.h>
#include <types/types.h>

#include "BufferedReader.h"
#include "tsPacket.h"
#include "vodCoreException.h"

// Used to automatically switch to reading the next file in a given list
class FileListIterator final : public FileNameIterator
{
   public:
    FileListIterator() : m_index(0) {}
    ~FileListIterator() override = default;

    std::string getNextName() override
    {
        return ++m_index < m_files.size() ? m_files[m_index] : "";
    }

    void addFile(const std::string& fileName)
    {
        m_files.push_back(fileName);
    }

   private:
    std::vector<std::string> m_files;
    size_t m_index;
};

struct FileReaderData final : ReaderData
{
    typedef ReaderData base_class;

    FileReaderData(uint32_t blockSize, uint32_t allocSize) : m_fileHeaderSize(0) {}

    ~FileReaderData() override = default;

    uint32_t readBlock(uint8_t* buffer, const uint32_t max_size) override { return m_file.read(buffer, max_size); }

    bool openStream() override;
    bool closeStream() override { return m_file.close(); }
    bool incSeek(const int64_t offset) override
    {
        return m_file.seek(offset, File::SeekMethod::smCurrent) != -1;
    }

    File m_file;
    uint32_t m_fileHeaderSize;
};

class BufferedFileReader final : public BufferedReader
{
   public:
    BufferedFileReader(uint32_t blockSize, uint32_t allocSize = 0, uint32_t prereadThreshold = 0);

    bool openStream(int readerID, const char* streamName, int pid = 0,
                    const CodecInfo* codecInfo = nullptr) override;
    bool gotoByte(int readerID, int64_t seekDist) override;

   protected:
    ReaderData* intCreateReader() override { return new FileReaderData(m_blockSize, m_allocSize); }
};

#endif
