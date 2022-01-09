#ifndef __BUFFERED_FILE_WRITER_H
#define __BUFFERED_FILE_WRITER_H

#include <containers/safequeue.h>
#include <fs/file.h>
#include <system/terminatablethread.h>
#include <types/types.h>

#include <map>

#include "vod_common.h"

const unsigned WRITE_QUEUE_MAX_SIZE = 400 * 1024 * 1024 / DEFAULT_FILE_BLOCK_SIZE;  // 400 Mb max queue size

struct WriterData
{
    enum class Commands
    {
        wdNone,
        wdWrite,
        wdDelete,
        wdCloseFile
    };

    uint8_t* m_buffer;
    int m_bufferLen;
    AbstractOutputStream* m_mainFile;
    Commands m_command;

   public:
    WriterData() : m_buffer(0), m_bufferLen(0), m_mainFile(), m_command() {}

    void execute();
};

class BufferedFileWriter : public TerminatableThread
{
   public:
    BufferedFileWriter();
    ~BufferedFileWriter() override;
    void terminate();
    inline int getQueueSize() { return (int)m_writeQueue.size(); }
    inline bool addWriterData(const WriterData& data)
    {
        if (m_lastErrorCode == 0)
        {
            m_nothingToExecute = false;
            return m_writeQueue.push(data);
        }
        else
        {
            throw std::runtime_error(m_lastErrorStr);
            return false;
        }
    }
    bool isQueueEmpty() { return m_nothingToExecute; }

   protected:
    void thread_main() override;

   private:
    bool m_nothingToExecute;
    int m_lastErrorCode;
    std::string m_lastErrorStr;
    bool m_started;
    bool m_terminated;

    WaitableSafeQueue<WriterData> m_writeQueue;
};

#endif
