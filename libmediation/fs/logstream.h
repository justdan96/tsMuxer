/*
$Log: logstream.h,v $
Revision 1.7  2006/08/30 03:50:05  mike
Исправлена ошибка при удалении узлов по тегу. Заменены int на uint32_t

Revision 1.6  2006/08/18 16:16:25  andreyk
В netflow collector v4 добавлена поддержка Netflow v9.

Revision 1.5  2006/07/20 16:26:18  andreyk
Из класса VoipSession перенесены в VoipProcessor функции по обработке сессии. В классе VoipSession оставлены только данные, связанные с сессией.

Revision 1.4  2006/05/29 13:24:23  andreyk
Пересобрана libmediation

Revision 1.3  2006/05/02 10:49:00  andreyk
*** empty log message ***

Revision 1.2  2006/04/28 13:46:18  andreyk
*** empty log message ***

Revision 1.1  2006/04/19 17:00:00  mike
*** empty log message ***

Revision 1.23  2006/03/24 13:28:29  andreyk
Добавлен функционал

Revision 1.22  2006/02/20 13:40:11  andreyk
Убрана ненужная директива include

Revision 1.21  2006/01/27 10:56:19  andreyk
Добавлена функция для вывода в лог произвольного буфера

Revision 1.20  2006/01/18 11:10:10  mike
*** empty log message ***

Revision 1.19  2005/12/27 11:52:01  mike
Реализованы собственные объекты синхронизации

Revision 1.18  2005/12/23 15:40:38  mike
*** empty log message ***

Revision 1.17  2005/09/21 18:22:36  mike
*** empty log message ***

*/
#ifndef LOGSTREAM_H
#define LOGSTREAM_H

#include <sys/timeb.h>
#include <ctime>
#include <iostream>
#include <iomanip>

#include "system/mutex.h"

#include "alternativefilestream.h"

class LogStream;

//------------------------------------------------------------------------------------
//			loglevel manipulator class declaration
//------------------------------------------------------------------------------------
class LIBMEDIATION_API loglevel 
{
public:
	loglevel(uint32_t);
	LogStream& operator() ( LogStream& ) const; 

	uint32_t m_logLevel;

};	// loglevel

//------------------------------------------------------------------------------------
//			LogStream class declaration
//------------------------------------------------------------------------------------
// This class is thread-safe
class LIBMEDIATION_API LogStream 
: 
	public AlternativeFileStream 
{
public:
	static const uint32_t MessageLevel = 0x01;
	static const uint32_t MessageDate = 0x02;
	static const uint32_t MessageTime = 0x04;
	static const uint32_t MessageThreadID = 0x08;

	LogStream(
		uint32_t logLevel,
		const std::string& fileName, 
		const std::string& oldFilesDir, 
		const uint32_t secRotationPeriod,
		const uint32_t maxFileSizeBytes = 128*1024*1024,
		const bool rotateAtInitialization = true,
		AlternativeFileStream::DestDirectoryDetalization 
			dirDetalization = AlternativeFileStream::None,
		uint32_t systemMessages = MessageLevel | MessageDate | MessageTime | MessageThreadID,
		const std::string& fileNamePrefix = "" );
	virtual ~LogStream();

	virtual void forceMoveCurrentFile();
	void setLogLevel( uint32_t newLogLevel );
	uint32_t getLogLevel();

	virtual void write( 
		const char *str, 
		uint32_t count );
	virtual void endmsg();

	virtual AlternativeFileStream& operator<<( const char* );
	virtual AlternativeFileStream& operator<<( const std::string& );
	virtual AlternativeFileStream& operator<<( const LightString& );
	virtual AlternativeFileStream& operator<<( void* );
	virtual AlternativeFileStream& operator<<( const char& );
	virtual AlternativeFileStream& operator<<( const double& );
	virtual AlternativeFileStream& operator<<( const bool );
	virtual AlternativeFileStream& operator<<( const uint16_t );
	virtual AlternativeFileStream& operator<<( const uint32_t );
	virtual AlternativeFileStream& operator<<( const uint64_t );
	virtual AlternativeFileStream& operator<<( const int16_t );
	virtual AlternativeFileStream& operator<<( const int32_t );
	virtual AlternativeFileStream& operator<<( const int64_t );
	virtual AlternativeFileStream& operator<<( const mtime::Time& );
	virtual AlternativeFileStream& operator<<( const mtime::TimeInterval& );
	virtual AlternativeFileStream& operator<<( const mtime::DayTime& );
	virtual AlternativeFileStream& operator<<( const mtime::DayTimeInterval& );
	virtual AlternativeFileStream& operator<<( const IPAddress& );
	virtual AlternativeFileStream& operator<<( const IPNet& );

	virtual AlternativeFileStream& operator<<( AlternativeFileStream& (*_Pfn)(AlternativeFileStream&) );

	virtual void width( size_t );
	virtual void fill( char );
	virtual void hex();
	virtual void dec();

private:
	Mutex m_mutex;
	uint32_t m_logLevel;
	uint32_t m_currentLogLevel;
	uint32_t m_systemMessages;

	void setCurrentLogLevel( uint32_t newCurrentLogLevel );

	friend class loglevel;

};	// LogStream
//------------------------------------------------------------------------------------
void LIBMEDIATION_API bufToLogStream( 
	const char* const buf, 
	const uint32_t& length,
	LogStream* const stream,
  	bool printLength = true );

LIBMEDIATION_API AlternativeFileStream& operator<<( AlternativeFileStream&, const loglevel& manip );

#endif
