#include "bufferedFileReader.h"
#include <iostream>
#include <fs/systemlog.h>
#include "vod_common.h"
#include "vodCoreException.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace std;

bool FileReaderData::openStream() 
{ 
    base_class::openStream();

    bool rez =  m_file.open ( m_streamName.c_str(), File::ofRead );
    
    if ( !rez ) 
    {
#ifdef WIN32
        char msgBuf [ 32*1024 ];
        
        memset ( msgBuf, 0, sizeof(msgBuf) );
        
        DWORD dw = GetLastError(); 

        FormatMessageA ( FORMAT_MESSAGE_FROM_SYSTEM,
                         NULL,
                         dw,
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                         ( LPSTR ) &msgBuf,
                         sizeof ( msgBuf ), 
                         NULL 
                       );

        string str ( msgBuf );
        LTRACE ( LT_ERROR, 0, msgBuf );
#endif
    }

    return rez;
}

BufferedFileReader::BufferedFileReader (uint32_t blockSize, uint32_t allocSize, uint32_t prereadThreshold): 
	BufferedReader(blockSize, allocSize, prereadThreshold)
{
}

bool BufferedFileReader::openStream(uint32_t readerID, const char* streamName, int pid, const CodecInfo* codecInfo)
{
	FileReaderData* data = (FileReaderData*) getReader(readerID);
	
	if (data == 0) 
	{
		LTRACE(LT_ERROR, 0, "Unknown readerID " << readerID);
		return false;
	}
	data->m_firstBlock = true;
	data->m_lastBlock = false;
	data->m_streamName = streamName;
	data->m_fileHeaderSize = 0;
	data->closeStream();
	if (!data->openStream()) {
		return false;
	}
	return true;
}
bool BufferedFileReader::gotoByte(uint32_t readerID, uint64_t seekDist)
{
	FileReaderData* data = (FileReaderData*) getReader(readerID);
	if (data) {
		data->m_blockSize = m_blockSize - (uint32_t)(seekDist % (uint64_t)m_blockSize);    
		uint64_t seekRez = data->m_file.seek(seekDist +  data->m_fileHeaderSize, File::smBegin);
		bool rez = seekRez != (uint64_t)-1;
		if (rez) {
			data->m_eof = false;
			data->m_notified = false;
		}
		return rez;
	}
	else
		return false;
}
