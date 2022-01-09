
#include "bufferedFileWriter.h"

#include <fs/systemlog.h>

void WriterData::execute()
{
    switch (m_command)
    {
    case Commands::wdWrite:
        if (m_mainFile)
        {
            m_mainFile->write(m_buffer, m_bufferLen);
        }
        delete[] m_buffer;
        break;
    default:
        break;
    };
}

BufferedFileWriter::BufferedFileWriter() : m_started(false), m_terminated(false), m_writeQueue(WRITE_QUEUE_MAX_SIZE)
{
    m_lastErrorCode = 0;
    m_nothingToExecute = true;
    run(this);
}

BufferedFileWriter::~BufferedFileWriter()
{
    terminate();
    while (!m_writeQueue.empty())
    {
        WriterData writerData = m_writeQueue.pop();
        writerData.execute();
    }
}

void BufferedFileWriter::thread_main()
{
    while (!m_terminated)
    {
        WriterData writerData = m_writeQueue.pop();
        try
        {
            writerData.execute();
        }
        catch (std::runtime_error& e)
        {
            m_lastErrorStr = e.what();
            m_lastErrorCode = -1;
            LTRACE(LT_ERROR, 0, "BufferedFileWriter::thread_main() throws runtime_error: " << e.what());
        }
        catch (std::exception& e)
        {
            m_lastErrorStr = e.what();
            m_lastErrorCode = -1;
            LTRACE(LT_ERROR, 0, "BufferedFileWriter::thread_main() throws exception: " << e.what());
        }
        catch (...)
        {
            m_lastErrorStr = "Unknown expcetion";
            m_lastErrorCode = -1;
            LTRACE(LT_ERROR, 0, "BufferedFileWriter::thread_main() throws unknown exception");
        }
        m_nothingToExecute = m_writeQueue.size() == 0;
    }
}

void BufferedFileWriter::terminate()
{
    if (m_terminated)
        return;
    m_terminated = true;
    WriterData data;
    data.m_command = WriterData::Commands::wdNone;
    m_writeQueue.push(data);
    join();
}
