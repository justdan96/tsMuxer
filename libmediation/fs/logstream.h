#ifndef LOGSTREAM_H
#define LOGSTREAM_H

#include <sys/timeb.h>

#include <ctime>
#include <iomanip>
#include <iostream>

#include "alternativefilestream.h"
#include "system/mutex.h"

class LogStream;

//------------------------------------------------------------------------------------
//			loglevel manipulator class declaration
//------------------------------------------------------------------------------------
class loglevel
{
   public:
    loglevel(uint32_t);
    LogStream& operator()(LogStream&) const;

    uint32_t m_logLevel;

};  // loglevel

//------------------------------------------------------------------------------------
//			LogStream class declaration
//------------------------------------------------------------------------------------
// This class is thread-safe
class LogStream : public AlternativeFileStream
{
   public:
    static const uint32_t MessageLevel = 0x01;
    static const uint32_t MessageDate = 0x02;
    static const uint32_t MessageTime = 0x04;
    static const uint32_t MessageThreadID = 0x08;

    LogStream(uint32_t logLevel, const std::string& fileName, const std::string& oldFilesDir,
              const uint32_t secRotationPeriod, const uint32_t maxFileSizeBytes = 128 * 1024 * 1024,
              const bool rotateAtInitialization = true,
              AlternativeFileStream::DestDirectoryDetalization dirDetalization = AlternativeFileStream::None,
              uint32_t systemMessages = MessageLevel | MessageDate | MessageTime | MessageThreadID,
              const std::string& fileNamePrefix = "");
    virtual ~LogStream();

    virtual void forceMoveCurrentFile();
    void setLogLevel(uint32_t newLogLevel);
    uint32_t getLogLevel();

    virtual void write(const char* str, uint32_t count);
    virtual void endmsg();

    virtual AlternativeFileStream& operator<<(const char*);
    virtual AlternativeFileStream& operator<<(const std::string&);
    virtual AlternativeFileStream& operator<<(const LightString&);
    virtual AlternativeFileStream& operator<<(void*);
    virtual AlternativeFileStream& operator<<(const char&);
    virtual AlternativeFileStream& operator<<(const double&);
    virtual AlternativeFileStream& operator<<(const bool);
    virtual AlternativeFileStream& operator<<(const uint16_t);
    virtual AlternativeFileStream& operator<<(const uint32_t);
    virtual AlternativeFileStream& operator<<(const uint64_t);
    virtual AlternativeFileStream& operator<<(const int16_t);
    virtual AlternativeFileStream& operator<<(const int32_t);
    virtual AlternativeFileStream& operator<<(const int64_t);
    virtual AlternativeFileStream& operator<<(const mtime::Time&);
    virtual AlternativeFileStream& operator<<(const mtime::TimeInterval&);
    virtual AlternativeFileStream& operator<<(const mtime::DayTime&);
    virtual AlternativeFileStream& operator<<(const mtime::DayTimeInterval&);
    virtual AlternativeFileStream& operator<<(const IPAddress&);
    virtual AlternativeFileStream& operator<<(const IPNet&);

    virtual AlternativeFileStream& operator<<(AlternativeFileStream& (*_Pfn)(AlternativeFileStream&));

    virtual void width(size_t);
    virtual void fill(char);
    virtual void hex();
    virtual void dec();

   private:
    Mutex m_mutex;
    uint32_t m_logLevel;
    uint32_t m_currentLogLevel;
    uint32_t m_systemMessages;

    void setCurrentLogLevel(uint32_t newCurrentLogLevel);

    friend class loglevel;

};  // LogStream
//------------------------------------------------------------------------------------
void bufToLogStream(const char* const buf, const uint32_t& length, LogStream* const stream, bool printLength = true);

AlternativeFileStream& operator<<(AlternativeFileStream&, const loglevel& manip);

#endif
