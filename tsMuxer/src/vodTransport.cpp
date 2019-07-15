#include "stdafx.h"

#include <iostream>

#include <errno.h>
#include <math.h>
#include "tsPacket.h"
#include "vodTransport.h"
#include "vodIndex.h"
#include "rtspRCGThread.h"
#include "multicastEncoder.h"
#include <include/fs/directory.h>
#include <include/fs/textfile.h>
#include "vod_common.h"
#include "textSubtitles.h"
#include "multiChannelStreamer.h"
#include <include/fs/systemlog.h>
#include <include/vodCoreException.h>
#include <include/sockets/tcpSocket.h>
#include <include/system/process.h>
#include <include/xmlparser/tinyxml.h>
#include <include/time/time.h>
#include "abstractDemuxer.h"
#include "metaDemuxer.h"
#include "avPacket.h"
#include "tsMuxer.h"
#include "singleFileMuxer.h"
#include "nalUnits.h"                      
//#include <include/time/time.h>
#include "dvdHelper.h"

#include "textSubtitles.h"
#include "base64.h"
#include "tsDemuxer.h"
#include "aac.h"
#include "abstractCoreContext.h"

#include <queue>
#include <vector>
#include <map>

#include "tsPacket.h"

#include "raptorEncoder.h"
#include "multicastRetransmitter.h"
//#include "playerCmdEmu.h"

//#define ENCODING_SERVER_MODE
//#define YT_TESTER_MODE

static const char* VERSION_STR = "1.0.58";
static const int WRITERS_COUNT = 2;

using namespace std;
using namespace mtime;
using namespace vodcore;

namespace vodcore
{

Mutex MemoryFileManager::m_mtx;
map<string, MemoryFileHandle*>  MemoryFileManager::m_handles;

enum SmartBridgeSource { sbsNone = 0, sbsUdp, sbsRadio, sbsYT, sbsRT, sbsDVT, sbsBBC };

DynamicLinkManager* _dlMan = NULL;

// ------------------- global vars
struct VodApplication 
{
	VodApplication() {
	    _dlMan = dlMan = new DynamicLinkManager();
		fileReader = new BufferedFileReader(DEFAULT_FILE_BLOCK_SIZE, DEFAULT_FILE_BLOCK_SIZE + MAX_TS_PACKET_LEN + MAX_AV_PACKET_SIZE, DEFAULT_FILE_BLOCK_SIZE / 2);
		tsAlignedReadManager = new BufferedReaderManager(2, DEFAULT_ROUND_BLOCK_SIZE, DEFAULT_ROUND_BLOCK_SIZE + MAX_AV_PACKET_SIZE);
		unalignedReadManager = new BufferedReaderManager(2, DEFAULT_FILE_BLOCK_SIZE, DEFAULT_FILE_BLOCK_SIZE + MAX_AV_PACKET_SIZE);
		ytReadManager = new BufferedReaderManager( 2, YT_ROUND_BLOCK_SIZE, YT_ROUND_BLOCK_SIZE + MAX_AV_PACKET_SIZE, YT_ROUND_BLOCK_SIZE / 2 );
		streamer = new MCVodStreamer();
		
		for (int i = 0; i < STREAMERS_COUNT; ++i)
			streamer->addWorkerThread();

		eventHandler = 0;
		autoIndexer = 0;
	}
	~VodApplication() {
		stopped = true;
		delete streamer;
		delete ytReadManager;
		delete tsAlignedReadManager;
		delete unalignedReadManager;
		delete fileReader;
		delete dlMan;
	}
	BufferedFileWriter& getBufferedFileWriter() 
	{
		bufferedWriterIndex = (bufferedWriterIndex+1) % WRITERS_COUNT;
		return bufferedFileWriters[bufferedWriterIndex];
	}
public:
	static bool stopped;
	DynamicLinkManager* dlMan;
	BufferedFileReader* fileReader;
	BufferedReaderManager* tsAlignedReadManager;
	BufferedReaderManager* unalignedReadManager;
	BufferedReaderManager* ytReadManager;
	BufferedFileWriter bufferedFileWriters[WRITERS_COUNT];
	map<string, MulticastEncoder*>  multicastEncoders;
	Mutex multicastEncodersLock;
	EVENT_HANDLER_PROC eventHandler;
	AutoIndexer* autoIndexer;
	vector<MCVodStreamerWorker*> m_StreamerThreads;
	MCVodStreamer* streamer;
	int bufferedWriterIndex;

	/*
	BufferedFileReader fileReader ( DEFAULT_ROUND_BLOCK_SIZE, DEFAULT_ROUND_BLOCK_SIZE + MAX_TS_PACKET_LEN + MAX_AV_PACKET_SIZE, DEFAULT_ROUND_BLOCK_SIZE / 2);
	BufferedReaderManager readManager ( 2 );
	BufferedReaderManager ytReadManager ( 2, YT_ROUND_BLOCK_SIZE, YT_ROUND_BLOCK_SIZE + MAX_TS_PACKET_LEN + MAX_AV_PACKET_SIZE, YT_ROUND_BLOCK_SIZE / 2 );
	BufferedFileWriter bufferedFileWrited;
	map<string, MulticastEncoder*>  multicastEncoders;

	EVENT_HANDLER_PROC eventHandler = 0;
	AutoIndexer* autoIndexer = 0;
	extern AbstractCoreContext* vodCoreContext;
	*/
};
bool VodApplication::stopped = false;
extern AbstractCoreContext* vodCoreContext;


VodApplication vodApplication;

//---------------------------------------------------------------------------------------


#ifdef WIN32
PROCESS_INFORMATION execProcessAsync(const string& appName, const string& commandLine)
{
	SECURITY_ATTRIBUTES security;
	memset(&security, 0, sizeof(security));
	STARTUPINFOA si;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW; // STARTF_USESHOWWINDOW;// + 
    si.wShowWindow = SW_SHOW;
	PROCESS_INFORMATION pi; 
    ZeroMemory( &pi, sizeof(pi) );
	char runbuffer[16384];
	//memcpy(runbuffer, (string(" ") + commandLine).c_str(), commandLine.size()+1);
	//runbuffer[commandLine.size()+1] = 0;
	memcpy(runbuffer, appName.c_str(), appName.size());
	runbuffer[appName.size()] = 0;
	string folder = extractFilePath(appName);
	//bool rez = CreateProcessA(appName.c_str(), runbuffer,  NULL, NULL, FALSE, 0, NULL,  
	//	                      folder.c_str(), &si, &pi);
    LTRACE ( LT_DEBUG, 0, "appName: " << appName )
    LTRACE ( LT_DEBUG, 0, "folder: " << folder )

	BOOL rez = CreateProcessA(NULL, runbuffer,  NULL, NULL, FALSE, 0, NULL,  
		                      folder.c_str(), &si, &pi);
    
    LTRACE ( LT_DEBUG, 0, "CreateProcessA res: " << rez )

	if ( rez == FALSE ) {
		int retCode = GetLastError();
		THROW(ERR_CANT_EXEC_PROCESS, "Can't execute " << appName << ". Error code: " << retCode);
	}

	return pi;
}
#endif

LIB_EXPORT bool LIB_STD_CALL checkTS ( string fileName )
{
    typedef vector<CheckStreamRez> ResultVector;
    typedef pair<AVChapters, ResultVector > ResultPair;    

    ResultPair rp = METADemuxer::DetectStreamReader ( *vodApplication.tsAlignedReadManager, fileName );

    ResultVector& rez = rp.second;

    int checkCount = 0;            

    for ( size_t i = 0; i < rez.size(); ++i )
        checkCount += rez[i].codecInfo.codecID ? 1 : 0;

    LTRACE ( LT_DEBUG, 0,"vodTransport::checkTS: tracks=" << checkCount )
    
    return checkCount >= 2;
}

LIB_EXPORT int64_t LIB_STD_CALL getFileLength(const char* fileName, double fromOffset)
{
	try {
		uint64_t fileSize;
		File file(fileName, File::ofRead);
		if (!file.size(&fileSize))
			return -1;

		AssetIndexManager* assetIndexManager = AssetIndexManager::getInstance();
		AssetIndex* assetIndex = assetIndexManager->getFileIndex(fileName);
		if (assetIndex)
		{
			int fileNum = -1;
			int64_t offset = 0;
			if (fromOffset > 0) {
				offset = assetIndex->getFrameNum(fromOffset, fileNum, stNormal);
				offset *= 188;
			}
			assetIndex->unlock();
			return fileSize - offset;
		}
		else {
			if (fromOffset > 0) {
				LTRACE(LT_WARN, 0, "getFileLength: No index found for file " << fileName << ". return full file length ignoring offset " << fromOffset);
			}
			return fileSize;
		}
	} catch(...) {
		return 0;
	}
}


LIB_EXPORT int LIB_STD_CALL getTSDuration(const char* fileName)
{
	try {
		if (vodApplication.stopped) return 0;
		int frameSize = 188;
		if (strEndWith(fileName, ".m2ts"))
			frameSize = 192;
		uint64_t fileSize;
		File file(fileName, File::ofRead);
		if (!file.size(&fileSize))
			return -1;
		fileSize = fileSize / frameSize * frameSize;
		fileSize -= frameSize * 2000; // Отступаем немного от конца файла
		if (fileSize < 0)
			fileSize = 0;

		uint8_t* tmpBuffer = new uint8_t[frameSize * 2000 + frameSize];

		// pcr from start of file
		int len = file.read(tmpBuffer, frameSize * 2000 + frameSize);
		uint8_t* curPtr = tmpBuffer;
		int64_t firstPcrVal = 0;
		while (curPtr < tmpBuffer + len) {
			TSPacket* tsPacket = (TSPacket*) (curPtr + frameSize - 188);
			if (tsPacket->afExists && tsPacket->adaptiveField.pcrExist) {
				firstPcrVal = tsPacket->adaptiveField.getPCR33();
				break;
			}
			curPtr += frameSize;
		}

		// pcr from end of file
		file.seek(fileSize, File::smBegin);
		len = file.read(tmpBuffer, frameSize * 2000 + frameSize);
		curPtr = tmpBuffer;
		int64_t lastPcrVal = -1;
		while (curPtr < tmpBuffer + len) {
			TSPacket* tsPacket = (TSPacket*) (curPtr + frameSize - 188);
			if (tsPacket->afExists && tsPacket->adaptiveField.pcrExist) {
				lastPcrVal = tsPacket->adaptiveField.getPCR33();
			}
			curPtr += frameSize;
		}

		file.close();
		delete [] tmpBuffer;
		
		return lastPcrVal != -1 ? (int)((lastPcrVal-firstPcrVal) / 90000) : 0;
	} catch(...) {
		return 0;
	}
}


BufferedFileReader& getFileReader()
{
	return *vodApplication.fileReader;
}

string extractPrefix(const char* streamName)
{
    string sRes = "";

    if ( streamName != 0 )
        for ( const char* ch = streamName; *ch; ++ch )
        {
            if ( ch > streamName+1 && *ch == '/' && ch[-1] == '/' && ch[-2] == ':' )
            {
                sRes = string ( streamName, 0, ch - streamName - 2 );
                break;
            }
        }

        return sRes;
}

LIB_EXPORT void LIB_STD_CALL getAssetDescription ( const char* sender, const char* cfileName, int descrType,
                                                 char* dstBuffer, unsigned len ) 
{
    *dstBuffer = 0;
	unsigned tmpLen = len;

	if (vodApplication.stopped) return;

    string errMessage;

	LTRACE(LT_DEBUG, 0, "getAssetDescription called. fileName=" << cfileName << " sender=" << ( sender ? sender : "" ) );

	bool exists = false;

    try
    {
        string assetDescr = "";

        string fileName = cfileName;

        string serviceName = fileName.substr ( 0, fileName.find ( ":" ) );

        uint8_t videoService = serviceName == "youtube" ? MCVodStreamer::vsYouTube : 
                             ( serviceName == "rutube"  ? MCVodStreamer::vsRuTube  :
                             ( serviceName == "dvtube"  ? MCVodStreamer::vsDvTube  :
                             ( serviceName == "bbc"     ? MCVodStreamer::vsBBC : MCVodStreamer::vsIndefinite ) ) );

        if ( videoService != MCVodStreamer::vsIndefinite )
        {
            SmartBridge* sb = new SmartBridge (  fileName );

            int size = len;

            sb->getDescription ( dstBuffer, size );

            dstBuffer [ size ] = 0;
        }
        else
        {
            AssetIndex* assetIndex = AssetIndexManager::getInstance()->getFileIndex ( fileName );

            if ( assetIndex )
            {
                string tmp = assetIndex->getFirstMainFileName();

                if (tmp.size() > 0 )
                    fileName = tmp;
            }
            
            assetDescr = vodApplication.streamer->getAssetDescr ( fileName.c_str(), descrType );

            if ( !assetDescr.empty() )
            {
                size_t size = assetDescr.size();

                if ( size < tmpLen )
                {
                    tmpLen = len = size;
                    memcpy ( dstBuffer, assetDescr.c_str(), len );
                    dstBuffer [ len ] = 0;
                }
                else
                    tmpLen = size;
            }

            if ( len < tmpLen )
                THROW ( ERR_COMMON_SMALL_BUFFER, "" )
        }
    }
    catch ( runtime_error& e ) 
    {
        LTRACE ( LT_ERROR, ERR_COMMON, "vodTransport::getAssetDescription runtime error: " << e.what() << ". Line: " << __LINE__ )
            errMessage = "ERROR:" + int32ToStr ( ERR_FILE_NOT_FOUND ) + ',' + e.what();

    }
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, e.m_errCode, "vodTransport::getAssetDescription error: " << e.m_errStr << ". Line: " << __LINE__ )
            errMessage = "ERROR:" + int32ToStr ( e.m_errCode ) + ',' + e.m_errStr;
    }

    if ( errMessage.empty() )
        len = tmpLen;
    else
    {
        if  ( errMessage.size() < len ) 
        {
            memcpy ( dstBuffer, errMessage.c_str(), errMessage.size() );
            dstBuffer [ errMessage.size() ] = 0;
        }
        else if ( len > 5 )
            memcpy ( dstBuffer, "ERROR", 6 );

        len = 0;
    }
}

LIB_EXPORT void LIB_STD_CALL freePorts ( const char* session)
{
	if (vodApplication.stopped) return;
	vodApplication.streamer->freePorts ( session );
}

LIB_EXPORT void LIB_STD_CALL reservePorts ( const char* session, int streamingMode, const char* address, 
                                            unsigned short port, const char* rtpInfo, char* outBuff, int& outBuffSize )
{
	if (vodApplication.stopped) return;
    try
    {
        LTRACE ( LT_DEBUG, 0, "reservePorts called. Adrress: " << address );

        string sRtpInfo = rtpInfo;

		vodApplication.streamer->reservePorts ( session, streamingMode, SocketAddress ( IPAddress ( address ), port ), sRtpInfo );

        int newOutBuffSize = sRtpInfo.size();

        if ( newOutBuffSize > outBuffSize || newOutBuffSize == 0 )
            *outBuff = 0;
        else
        {
            outBuffSize = newOutBuffSize;
            memcpy ( outBuff, sRtpInfo.c_str(), newOutBuffSize );
        }
    }
    catch (...) 
    {
        LTRACE ( LT_ERROR, 0, "Unknown exception at reservePorts" );
    }
}

LIB_EXPORT unsigned LIB_STD_CALL startStreaming ( const char* session,
											      int lowLevelTransport,
	                                              double ntpPos, 
												  int scale,
												  const char* fileName, 
												  const char* address, 
												  unsigned short port,
												  const char* rtpInfo,
												  long tcpSocketHandle,
												  bool doDownload,
												  int64_t downloadLimit
                                                )
{
    unsigned handle = 0;
	static int stCounter = 0;
    try 
    {
        if ( vodApplication.stopped ) return handle;

        LTRACE ( LT_DEBUG, 0, "vodTransport::StartStreaming called. FileName: " << fileName << ", ip: " << address << "." << " sessionID=" << (session ? session : ""));
        LTRACE ( LT_DEBUG, 0, "vodTransport::StartStreaming. total starts: " << ++stCounter);

        bool usePluginStreaming = strStartWith ( fileName, "youtube://" ) || 
                                  strStartWith ( fileName, "rutube://" )  ||
                                  strStartWith ( fileName, "bbc://" )     ||
                                  strStartWith ( fileName, "radio://" )   ||
                                  strStartWith ( fileName, "dvtube:/" )  ||
                                  strStartWith ( fileName, "udp://" );
        
        StreamingContext* streamingContext = 0;
        
        string assetName = fileName;
        
        /*if ( usePluginStreaming ) 
        {
            usePluginStreaming = !checkSmartBridgeCache ( fileName );
            
            if ( ntpPos <= 0 && strstr ( fileName, "&pos=" ) != NULL )
            {
                size_t pos = assetName.find ( "&pos" ) + 5;
                string p = assetName.substr ( pos, assetName.find ( '&', pos ) );
                ntpPos = strToDouble ( p.c_str() );
            }
        }*/

        if ( usePluginStreaming )
            handle = startSmartBridgeStreaming ( session, ntpPos, fileName, address, port, rtpInfo, tcpSocketHandle );
        else
        {
			/*
            if ( cyclicMulticastEncoder ( fileName ) )
            {
				Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
                MulticastEncoder* me = vodApplication.multicastEncoders [ fileName ];
				if (me) {
					if (ntpPos <= 0)
						ntpPos += me->getCurNptPos();
					ntpPos = FFMIN(FFMAX(0, ntpPos), me->getCurNptPos());
				}
            }
			*/
			
			
			{
				Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
				map<string, MulticastEncoder*>::iterator itr = vodApplication.multicastEncoders.find(fileName);
				MulticastEncoder* me = itr != vodApplication.multicastEncoders.end() ? itr->second : 0;
				
				if ( me )
				{
					if ( ntpPos <= 0 )
						ntpPos += me->getCurNptPos();
					else
						ntpPos = me->getCurNptPos() - (me->getCyclicBufferLength() - ntpPos);

					ntpPos = FFMIN ( FFMAX(0, ntpPos), me->getCurNptPos() );
				}
			}

            handle = vodApplication.streamer->startStreaming ( session,
													           usePluginStreaming ? 
                                                               *vodApplication.ytReadManager :
                                                               ( lowLevelTransport == StreamingContext::llTransportRAW ?
                                                               *vodApplication.unalignedReadManager : 
                                                               *vodApplication.tsAlignedReadManager 
                                                                ),
                                                               assetName.c_str(),
                                                               ntpPos,
                                                               scale, 
                                                               SocketAddress ( IPAddress ( address ), port ),
                                                               lowLevelTransport, 
                                                               rtpInfo, 
                                                               ( TCPSocket* ) tcpSocketHandle,
                                                               streamingContext,
                                                               doDownload, downloadLimit
                                                             );
                                                             
            //if ( ntpPos >= 0 && ntpPos < 1 && vodCoreContext->cyclicMulticastEncoder ( fileName ) )                                                             
            //    vodApplication.streamer->pauseStreaming ( handle );
        }
        if ( handle == 0 ) 
        {
            LTRACE ( LT_DEBUG, 0, "vodTransport::startStreaming: can't start streaming ( session: " << ( session ? session : "" ) << ", file:" << fileName << ". Remove streaming context. Line: " << __LINE__ )
	        delete streamingContext;
	    }
	}
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "vodTransport::startStreaming error: can't StartStreaming, " << e.m_errStr )
    }
    catch ( exception& e ) 
    {
        LTRACE ( LT_ERROR, 0, "vodTransport::startStreaming error: " << e.what() << ". Line: " << __LINE__ )
	}
	catch ( ... ) 
    {
        LTRACE ( LT_ERROR, 0, "vodTransport::startStreaming error: unknown. Line: " << __LINE__ )
	}

	return handle;
}

LIB_EXPORT void LIB_STD_CALL setSyncGroupCount ( int count )
{
	if (vodApplication.stopped) return;
    LTRACE ( LT_DEBUG, 0, "setSyncGroupCount called. count: " << count )

    try 
    {
#ifdef SUPPORT_SYNC_GROUP
        streamer.setSyncGroupCount ( count );
#endif
    } 
    catch (...) 
    {
        LTRACE ( LT_ERROR, 0, "Unknown exception at setSyncGroupCount" )
    }
}

LIB_EXPORT void LIB_STD_CALL resumeStreamingGroup(const char* groupName)
{
	if (vodApplication.stopped) return;
    LTRACE(LT_DEBUG, 0, "resumeStreamingGroup called. name: " << groupName)

    try 
    {
		vodApplication.streamer->resumeStreamingGroup ( groupName );
    } 
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at resumeStreamingGroup")
    }
}

unsigned getSBTSCounter() 
{
    static Mutex sbtsMtx;
    static unsigned tscounter = 0;
    Mutex::ScopedLock lock ( &sbtsMtx );
    return ++tscounter;
}

string startBridgeMuxing ( SmartBridgeSource sb, const string& url, TSMuxer* tsMuxer )
{

    int bitRate = YOUTUBE_BITRATE;
    
    typedef map<string, string> StrMap;

    string source = url;
    
    int vbvLength = YOUTUBE_VBV_LENGTH;
    
    bool keepVideo = false;
    bool keepAudio = false;
    
    tsMuxer->setFileBlockSize ( YT_DEFAULT_PREBUFFERING_TIME, YT_MEMORY_FILE_BUFFER_SIZE );
    
    tsMuxer->setAsyncMode ( false );
    tsMuxer->setAsyncThreadMode(true);
    tsMuxer->setNewStyleAudioPES ( false );

    StrMap audioParams, videoParams;
    
    audioParams["track"] = "1";
    videoParams["track"] = "2";

    string audioCodecName = "A_MP3";
    string videoCodecName = "V_MPEG-2";
    
	switch ( sb )
	{
	    case sbsUdp:
	    {
            vbvLength = 1000;
            
            bool transcode = strstr ( source.c_str(), "transcode" ) != NULL;
            
            keepAudio = transcode ? strstr ( source.c_str(), "keepsound=1" ) != NULL : keepAudio;
            keepVideo = true;
                        
            if ( transcode && strstr ( source.c_str(), "codec=h264" ) != NULL )
            {
                //audioCodecName = "A_AAC";
                videoCodecName = "V_MPEG4/ISO/AVC";
                
                size_t pos = string::npos;
                
                /*if ( ( pos = source.find ( "bitrate=" ) ) != string::npos )
                {
                    string s = source.substr ( pos + strlen( "bitrate=" ), source.size() );
                    
                    if ( ( pos = s.find ( "&" ) ) != string::npos )
                        s = s.substr ( 0,  pos );
                    
                    
                    bitRate = s.empty() ? 1000 : strToInt32 ( s.c_str() );
                    
                    bitRate *= 1024;
                }*/
            }
            
            source = "multicast" + source.substr ( 4, source.size() );
            
	    }break;
	    
	    case sbsRadio:
	    {
	        keepAudio = true;
	    }break;
	    
	    case sbsYT:
	    case sbsRT:
	    case sbsBBC:
	    {
	        keepAudio = true;
	        keepVideo = true;
	    }break;
	    
	    default:
	        THROW ( ERR_COMMON, "unknown bridge." )
	}

    if ( keepVideo )
        tsMuxer->addStream ( videoCodecName, source, videoParams );

    if ( keepAudio )
	    tsMuxer->addStream ( audioCodecName, source, audioParams );

    tsMuxer->parseMuxOpt ( string ( "--vbv-len=") + int32ToStr ( vbvLength ) + string (" --maxbitrate=" ) + doubleToStr( bitRate / 1000.0 ) );
    
    string memName = "memory://" + source + "_" + int32ToStr ( getSBTSCounter() );
    
    tsMuxer->doMux ( memName, true );
    
    //tsMuxer->doMux ( "test_mux.ts", true );
    //return 0;
    
	return memName;
}

LIB_EXPORT int LIB_STD_CALL createStreamingGroup ( const char* address, 
												   const char* name,
												   double nptPos,
												   int scale, 
 												   int lowLevelTransport,
												   int loopCount,
												   int eoplDelay,
												   int eofDelay,
												   bool suspended
												 )
{
	int res = -1;
	

	LTRACE(LT_DEBUG, 0, "CreateStreamingGroup called. address: " << address << " name:" << name);

	try 
	{
		if ( !vodApplication.stopped )
		{
		    /*
			if ( strstr ( address, "icca_demo_" ) )
            {
                nptPos = time().toInt() % 300;			
                LTRACE ( LT_DEBUG, 0, "Create streaming group at: " << (int)nptPos )
            }
            */
            
            
            
            SmartBridgeSource sb = strStartWith ( address, "radio://" ) ? sbsRadio : ( strStartWith ( address, "rutube://" ) ? sbsRT :
                                                                              ( strStartWith ( address, "youtube://" ) ? sbsYT :
                                                                              ( strStartWith ( address, "udpt://" ) ? sbsUdp : 
                                                                              ( strStartWith ( address, "bbc://" ) ? sbsBBC : sbsNone ) ) ) );
            
            BufferedReaderManager& arm = lowLevelTransport == StreamingContext::llTransportRAW ? *vodApplication.unalignedReadManager :
                                                                                                 *vodApplication.tsAlignedReadManager;
			const BufferedReaderManager& rm = sb == sbsNone || sb == sbsUdp ? arm : *vodApplication.ytReadManager;

			TSMuxer* tsMuxer = NULL;
			SmartBridge* bridge = NULL;
			
			string memName = address;
			
			if ( sb != sbsNone )
			{

                if ( sb == sbsDVT || sb == sbsUdp )
                {
                    memName = memName.substr ( string ( "udpt://" ).length(), memName.size() );
                    memName = "udp://" + memName;
                    
                    bridge = new SmartBridge ( memName );

                    memName = bridge->getDestination();

                    if ( !memName.empty() && bridge->isStatic() )
                    {
                        delete bridge;
                        bridge = NULL;
                    }
                    else
                        memName =  bridge->start();
                }
			}

			StreamingContext* context = vodApplication.streamer->createStreamingGroup ( rm, memName.c_str(), name, nptPos,
																						scale, lowLevelTransport,
																						loopCount, eoplDelay, eofDelay,
                                                                                        suspended
																					  );
			if ( context )
			{
				if ( tsMuxer )
				{
					if ( lowLevelTransport == StreamingContext::llTransportRTP )
						( ( StreamingContextRTP* )context )->setVBVMode ( StreamingContextRTP::VBV_BITRATE_AUTO,
																	      tsMuxer->getVBVLength()
																		); 
						
					context->setMuxer ( tsMuxer );
				}
				else if ( bridge )
				    context->setSmartBridge( bridge );

				res = 0;
			}
		}
		else
		    res = -2;

	}
	catch ( VodCoreException& e ) 
	{
	    res = -3;
		LTRACE ( LT_ERROR, 0, "Can't create streaming group: " << e.m_errStr )
    }
	catch ( exception& e) 
	{
	    res = -3;
		LTRACE ( LT_ERROR, 0, e.what() )
	}
	catch (...) 
	{
	    res = -3;
		LTRACE ( LT_ERROR, 0, "Unknown exception at createStreamingGroup" )
	}

	return res;
}

LIB_EXPORT bool LIB_STD_CALL isStreamingGroup(const char* fileName)
{
	if (vodApplication.stopped) return false;
	try{
		//LTRACE(LT_DEBUG, 0, "isStreamingGroup called. fileName: " << fileName);
		return vodApplication.streamer->isStreamingGroup(fileName) != 0;
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't create streaming group: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at createStreamingGroup");
	}
	return false;
}

LIB_EXPORT uint32_t LIB_STD_CALL addDestination(const char* session,
									   const char* fileName,
									   const char* ip, uint16_t port,
									   const char* rtpInfo,
									   long tcpSocketHandle,
									   const char* ixface )
{
	if (vodApplication.stopped) return 0;
	try {
		LTRACE(LT_DEBUG, 0, "addDestination called0. fileName: " << fileName);
		return vodApplication.streamer->addDestination(session, fileName, SocketAddress(ip, port), rtpInfo, 
			                           (TCPSocket*) tcpSocketHandle, ixface);
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't create streaming group: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at createStreamingGroup");
	}
	return 0;
}

LIB_EXPORT int LIB_STD_CALL removeDestination(const char* fileName, long handle) 
{
	if (vodApplication.stopped) return 0;
	try {
		LTRACE(LT_DEBUG, 0, "remoteDestination called. fileName: " << fileName);
		return vodApplication.streamer->remoteDestination(fileName, handle);
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't create streaming group: " << e.m_errStr);
        } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at createStreamingGroup");
	}
	return 0;
}

/*
LIB_EXPORT double LIB_STD_CALL getCyclicNptPos ( std::string name )
{
    double res = 0.0;

    try 
    {
        if ( !vodApplication.stopped )
        {
			Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
            map < string, MulticastEncoder* >::iterator itr =  vodApplication.multicastEncoders.find ( name );

            if ( itr != vodApplication.multicastEncoders.end())
                res = itr->second->getCurNptPos();
        }

    } 
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Can't get cyclic ntp of multicast encoder '" << name <<  "': " << e.m_errStr )
    }
    catch ( exception& e) 
    {
        LTRACE ( LT_ERROR, 0, e.what() )
    }
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at getCyclicNptPos" )
    }

    return res;
}
*/

LIB_EXPORT int cyclicStreamingFirstFileNum(const char* streamName)
{
	if (!cyclicMulticastEncoder(streamName))
		return -1;
	AssetIndexManager* assetIndexManager = AssetIndexManager::getInstance();
	AssetIndex* assetIndex = assetIndexManager->getFileIndex(streamName);
	int fileNum = -1;
	if (assetIndex)
	{
		double nptPos = -INT_MAX; // relative pos for cyclic streaming (get begining of buffer)
		assetIndex->getFrameNum(nptPos, fileNum, stNormal);
		assetIndex->unlock();
	}
	return fileNum;
}

LIB_EXPORT int cyclicStreamingLastFileNum(const char* streamName)
{
	if (!cyclicMulticastEncoder(streamName))
		return -1;
	AssetIndexManager* assetIndexManager = AssetIndexManager::getInstance();
	AssetIndex* assetIndex = assetIndexManager->getFileIndex(streamName);
	int fileNum = -1;
	if (assetIndex) {
		double nptPos = INT_MAX;
		assetIndex->getFrameNum(nptPos, fileNum, stNormal);
		assetIndex->unlock();
	}
	return fileNum;
}

LIB_EXPORT int LIB_STD_CALL cyclicMulticastEncoderLen ( std::string name )
{
    int res = 0;
    
    try 
    {
        if ( !vodApplication.stopped )
        {
			Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
            map < string, MulticastEncoder* >::iterator itr =  vodApplication.multicastEncoders.find ( name );
            if (itr == vodApplication.multicastEncoders.end()) {
          	  if (strEndWith(name, ".main.ts")) {
          		name = name.substr(0, name.length() - 8);
          		int pos = name.find_last_of('.');
          		if (pos >= 0)
          		  name = name.substr(0, pos);
          	  }
          	  itr =  vodApplication.multicastEncoders.find ( name );
            }

            if ( itr != vodApplication.multicastEncoders.end())
			{
            
                res = itr->second->getCyclicBufferLength();
            }
        }
            
    } 
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Can't check cyclic of multicast encoder '" << name <<  "': " << e.m_errStr )
    }
    catch ( exception& e) 
    {
        LTRACE ( LT_ERROR, 0, e.what() )
    }
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at cyclicMulticastEncoder" )
    }
    
    return res;
}

LIB_EXPORT bool LIB_STD_CALL cyclicMulticastEncoder ( std::string name )
{
    bool res = false;
    
    try 
    {
        if ( !vodApplication.stopped )
        {
			Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
            map < string, MulticastEncoder* >::iterator itr =  vodApplication.multicastEncoders.find ( name );
            if (itr == vodApplication.multicastEncoders.end()) {
          	  if (strEndWith(name, ".main.ts")) {
          		name = name.substr(0, name.length() - 8);
          		int pos = name.find_last_of('.');
          		if (pos >= 0)
          		  name = name.substr(0, pos);
          	  }
          	  itr =  vodApplication.multicastEncoders.find ( name );
            }

            if ( itr != vodApplication.multicastEncoders.end())
			{
            
                res = itr->second->cyclic();
            }
        }
            
    } 
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Can't check cyclic of multicast encoder '" << name <<  "': " << e.m_errStr )
    }
    catch ( exception& e) 
    {
        LTRACE ( LT_ERROR, 0, e.what() )
    }
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at cyclicMulticastEncoder" )
    }
    
    return res;
}

LIB_EXPORT bool LIB_STD_CALL isMulticastEncoder ( std::string name )
{
    bool res = false;
	for (int i = 0; i < name.size(); ++i) if (name[i] == '\\') name[i] = '/';
    try 
    {
        if ( !vodApplication.stopped )
        {
			Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
            map < string, MulticastEncoder* >::iterator itr =  vodApplication.multicastEncoders.find ( name );
            if (itr == vodApplication.multicastEncoders.end()) {
          	  if (strEndWith(name, ".main.ts")) {
          		name = name.substr(0, name.length() - 8);
          		int pos = name.find_last_of('.');
          		if (pos >= 0)
          		  name = name.substr(0, pos);
          	  }
          	  itr =  vodApplication.multicastEncoders.find ( name );
            }

            if ( itr != vodApplication.multicastEncoders.end())
			{
                res = true;
            }
        }
            
    } 
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Can't check cyclic of multicast encoder '" << name <<  "': " << e.m_errStr )
    }
    catch ( exception& e) 
    {
        LTRACE ( LT_ERROR, 0, e.what() )
    }
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at cyclicMulticastEncoder" )
    }
    
    return res;
}

LIB_EXPORT double LIB_STD_CALL getNptPos(unsigned readerID)
{
	
	double res = 0.0;
	
	try 
	{
	    if ( !vodApplication.stopped)
        {
            string streamName = vodApplication.streamer->getStreamName( readerID );

            res = vodApplication.streamer->getNptPos ( readerID );
            
			/*
            if ( cyclicMulticastEncoder ( streamName ) )
            {
				Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
				MulticastEncoder* me = vodApplication.multicastEncoders [ streamName ];
				if (me) {
					res -= me->getCurNptPos();
					res = FFMAX(-me->getCyclicBufferLength(), FFMIN(0, res));
				}
            }
			*/
			{
				Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
				map<string, MulticastEncoder*>::iterator itr = vodApplication.multicastEncoders.find(streamName);
				MulticastEncoder* me = itr != vodApplication.multicastEncoders.end() ? itr->second : 0;
				if (me) {
					res -= me->getCurNptPos();
					res = FFMAX(-me->getCyclicBufferLength(), FFMIN(0, res));
				}
			}
        }
        
	} 
	catch( VodCoreException& e ) 
	{
		LTRACE ( LT_ERROR, 0, "Can't get nptPos: " << e.m_errStr )
	}
	catch ( exception& e) 
	{
		LTRACE ( LT_ERROR, 0, e.what() )
	}
	catch (...) 
	{
		LTRACE ( LT_ERROR, 0, "Unknown exception at getNptPos" )
	}
	
	return res;
}

LIB_EXPORT double LIB_STD_CALL getGroupNptPos(const char* groupName)
{
	if (vodApplication.stopped) return 0.0;
	try {
		return vodApplication.streamer->getGroupNptPos(groupName);
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't get nptPos: " << e.m_errStr);
	} catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at getGroupNptPos");
	}
	return 0;
}

LIB_EXPORT int LIB_STD_CALL stopStreaming(unsigned readerID)
{
    int res = -1;
	static int stCounter = 0;
    
    try 
    {
        LTRACE(0,LT_DEBUG, "Stop streaming called. handle " << readerID);

        if ( !vodApplication.stopped )
            res =(int) vodApplication.streamer->stopStreaming ( readerID );
	}
    catch(VodCoreException& e) 
    {
		LTRACE(LT_ERROR, 0, "Can't create streaming group: " << e.m_errStr);
        res = -1;
    }
    catch ( exception& e) 
    {
		LTRACE(LT_ERROR, 0, e.what());
        res = -1;
	}
	catch (...) 
    {
		LTRACE(LT_ERROR, 0, "Unknown exception at stopStreaming");
        res = -1;
	}

    LTRACE ( LT_DEBUG, 0, "vodTransport::stopStreaming. total stops: " << ++stCounter);
    
	return res;
}

LIB_EXPORT unsigned LIB_STD_CALL pauseStreaming(unsigned readerID)
{
	if (vodApplication.stopped) return 0;
	try {
		LTRACE(0,LT_DEBUG, "Pause streaming called. ReaderID " << readerID);
		vodApplication.streamer->pauseStreaming(readerID);
		return 0;
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't pause stream: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at pauseStreaming");
	}
	return -1;
}

LIB_EXPORT unsigned LIB_STD_CALL resumeStreaming(unsigned readerID)
{
	if (vodApplication.stopped) return 0;
	try {
		LTRACE(0,LT_DEBUG, "Resume streaming called. ReaderID " << readerID);
		double res = vodApplication.streamer->resumeStreaming(readerID);
		return 0;
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't resume stream: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at resumeStreaming");
	}
	return -1;
}


LIB_EXPORT unsigned LIB_STD_CALL changeScale(unsigned readerID, int scale)
{
	if (vodApplication.stopped) return 0;
	try {
		LTRACE(0,LT_DEBUG, "ChangeScale called. ReaderID " << readerID);
		vodApplication.streamer->changeScale(readerID, scale);
		return 0;
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't change scale stream: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at ChangeScale");
	}
	return -1;
}

LIB_EXPORT int LIB_STD_CALL setPosition(unsigned readerID, double position)
{
	if (vodApplication.stopped) return 0;
	try 
	{
		LTRACE(0,LT_DEBUG, "setPosition to " << position << ". ReaderID " << readerID);
		
		string streamName = vodApplication.streamer->getStreamName ( readerID );

		/*
		if ( cyclicMulticastEncoder ( streamName ) && position <= 0 )
        {
			Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
            MulticastEncoder* me = vodApplication.multicastEncoders [ streamName ];
			if (me) {
				position += me->getCurNptPos();
				position = FFMIN(me->getCyclicBufferLength(), FFMAX(0, position));
			}
        }
		*/
		{
			Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
			map<string, MulticastEncoder*>::iterator itr = vodApplication.multicastEncoders.find(streamName);
			MulticastEncoder* me = itr != vodApplication.multicastEncoders.end() ? itr->second : 0;
			if (me) {
				if (position <= 0 ) {
					position += me->getCurNptPos();
				}
				else {
					position = me->getCurNptPos() - (me->getCyclicBufferLength() - position);
				}
				position = FFMIN(me->getCurNptPos(), FFMAX(0, position));
			}
		}		    
		return vodApplication.streamer->setPosition(readerID, position );
		
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't change position. Stream: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at setPosition");
	}
	return -1;
}

bool supportedRTPCodecs(const string& fileName)
{
	pair<AVChapters, vector<CheckStreamRez> > fullRez = METADemuxer::DetectStreamReader(*vodApplication.tsAlignedReadManager, fileName);
	vector<CheckStreamRez>& rez = fullRez.second;

		for ( size_t i = 0; i < rez.size(); i++)
			if (rez[i].codecInfo.codecID) 
			{
				if (rez[i].codecInfo.programName != "V_MPEG4/ISO/AVC" &&
					rez[i].codecInfo.programName != "A_AAC")
					return false;
			}
	return true;
}

///////////////
vector<string> assetRootDirs;

LIB_EXPORT bool LIB_STD_CALL setRootDir(const char* rootDir)
{
	if (vodApplication.stopped) return false;
    assetRootDirs.clear();

    assetRootDirs = splitStr ( rootDir, ';' );

    int count = assetRootDirs.size();

    for ( int i ( 0 ); i < count; ++i )
	    assetRootDirs [i] = closeDirPath ( assetRootDirs [i] );

	return true;
}

LIB_EXPORT int LIB_STD_CALL updateIndexes()
{
    uint64_t start_time = clockGetTimeEx();
    
    int res = -1;
    string at_str = " exception at vodTransport.updateIndexes";

    try
    {
        
        AssetIndexManager* aim = AssetIndexManager::getInstance();
        
        int count = assetRootDirs.size();

        for ( int i ( 0 ); i < count; ++i )
        {
            string dir = assetRootDirs [ i ];
            
            vector < string > files;
            
            getFileListByMask ( dir + "*.idx", &files, true );
            
            size_t filesCount = files.size();
            
            for ( size_t i = 0; i < filesCount; ++i )
            {
                string fileName = files [ i ];
                
                AsyncIndexWorker* aiw = aim->getAssetAsyncWorker ( fileName.c_str() );
                
                if ( aiw && !aiw->isFinished() )
                    continue;
                else
                {
                    AssetIndex* assetIndex = new AssetIndex ( fileName, false );
                    
                    if ( assetIndex->open() )
                    {
                        assetIndex->setDataCRC();
                        assetIndex->setAssetLength();
                        assetIndex->wirteMainIndex();
                        assetIndex->writeCRC();
                    }
                    
                    delete assetIndex;
                }
            }
        }
        
        res = 0;
    }
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Vod core" << at_str << ": " << e.m_errStr << ". Line: " << __LINE__ )
    }
    catch ( exception& e ) 
    {
        LTRACE(LT_ERROR, 0, "Std" << at_str << ": " << e.what() << ". Line: " << __LINE__ )
    } 
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown" << at_str << ". Line: " << __LINE__ )
    }
    
    uint64_t end_time = clockGetTimeEx();
    
    LTRACE ( LT_DEBUG, 0, "vodTransport.updateIndexes time: " << (uint32_t)( end_time - start_time ) )

    return res;
}

static string m3uRootDir = "";
LIB_EXPORT bool LIB_STD_CALL setM3uRoot(const char* rootDir)
{
	if (vodApplication.stopped) return false;
	m3uRootDir = closeDirPath(rootDir);
	return true;
}

static string httpServerURL = "";
LIB_EXPORT bool LIB_STD_CALL setHttpServerURL(const char* url)
{
	if (vodApplication.stopped) return false;
	httpServerURL = closeDirPath(url);
	return true;
}

static int defaultRTPSPTimeout = 3000;
LIB_EXPORT bool LIB_STD_CALL setDefaultRtspTimeout(const int timeout)
{
	if (vodApplication.stopped) return false;
	defaultRTPSPTimeout = timeout;
	return true;
}

string getLongFileLocation ( const string& fileName )
{
    string res = "";

    size_t count = assetRootDirs.size();

    size_t maxLen = 0;

    for ( size_t i ( 0 ); i < count; ++i )
    {
        string dir = assetRootDirs [ i ];

        if ( strstr ( fileName.c_str(), dir.c_str() ) && dir.length() > maxLen )
            res = dir;
    }

    if ( res.empty() )
        res = extractFileDir ( fileName );

    return res;
}

string assetFullNameToShortName(const string& fullName)
{
	return fullName.substr ( getLongFileLocation ( fullName ).length(), 32768 );
}

bool generateVLCM3uFile(const string& fullName)
{
	int port = 80;
	vector<string> tmp = splitStr(httpServerURL.c_str(), ':');
	if (tmp.size() == 3) {
		port = strToInt32(tmp[2].c_str());
		if (port == 0)
			port = 80;
	}
	string ext = extractFileExt(fullName);
	string shortName = assetFullNameToShortName ( fullName );
	string m3uName = m3uRootDir + shortName.substr(0, shortName.length() - ext.length()) + "m3u";
	TextFile file;
	if (!file.open(m3uName.c_str(), File::ofWrite))
		return false;
	file.writeLine("#EXTM3U");
	file.writeLine("#EXTVLCOPT--input-repeat=-1");
	file.writeLine("#EXTVLCOPT--http-reconnect=true");
	file.writeLine(string("#EXTVLCOPT:realrtsp-caching=") + int32ToStr(defaultRTPSPTimeout));
	file.writeLine(string("#EXTVLCOPT:rtsp-caching=")  + int32ToStr(defaultRTPSPTimeout));
	if (supportedRTPCodecs(fullName))
	{
		file.writeLine("#EXTVLCOPT:rtsp-tcp");
		file.writeLine("#EXTVLCOPT:rtsp-http");
		file.writeLine("#EXTVLCOPT:no-rtsp-kasenna");
		file.writeLine(string("#EXTVLCOPT:rtsp-http-port=") + int32ToStr(port));
	}
	else {
		file.writeLine("#EXTVLCOPT:rtsp-udp");
		file.writeLine("#EXTVLCOPT:rtsp-kasenna");
		file.writeLine(string("#EXTVLCOPT:rtsp-http-port=") + int32ToStr(port));
	}
	string nFileName = shortName;
	for (size_t i = 0; i < nFileName.length(); i++)
		if (nFileName[i] == '\\')
			nFileName[i] = '/';
	file.writeLine(httpServerURL +  nFileName);
	file.close();
	return true;
}


LIB_EXPORT unsigned LIB_STD_CALL createIndexFile(const char* fileName, bool createRewind)
{
	if (vodApplication.stopped) return 0;
	if (AssetIndexManager::getInstance()->isReadOnlyIndexes()) {
		LTRACE(LT_WARN, 0, "Can't start file indexing for file " << fileName << " because readOnly mode selected.");
		return 0;
	}
	try 
	{
		AssetIndexer indexer;
		int rez = indexer.createIndexFile(fileName, getFileReader(), createRewind);
		if (rez && m3uRootDir.size() > 0) {
			rez = generateVLCM3uFile(fileName);
		}
		return rez;
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't start indexing: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at createIndexFile");
	}
	return 0;
}

LIB_EXPORT unsigned LIB_STD_CALL getAssetStatus(const char* fileName)
{
	{
		Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
		if ( vodApplication.stopped ||
			 ( vodApplication.multicastEncoders.count ( fileName ) && vodApplication.multicastEncoders [ fileName ]->isWorking() ) )
			return 0;
	}
    AssetIndexManager* aim = AssetIndexManager::getInstance();
    
    AsyncIndexWorker* aiw = aim->getAssetAsyncWorker ( fileName );

    double rez ( 0.0 );

    if ( aiw && !aiw->isFinished() )
    {
        rez = aiw->getProgress ();
        rez = rez >= 0 ? rez*1000+0.5 : rez;
    }
    else
    {
        bool indexIsReady ( false ); 
        
        AssetIndex* assetIndex = new AssetIndex ( fileName, aim->isReadOnlyIndexes());
        
        uint32_t crc = 0;
        
        if ( assetIndex->indexFileExists() )
        {
            assetIndex->open();
            
            crc = assetIndex->getDataCRC();
            uint32_t currCrc = assetIndex->getCurrentDataCRC();

            uint64_t assetLength = assetIndex->getAssetLength();
            uint64_t currAssetLength = assetIndex->getCurrentAssetLength();

            if ( crc == 0 )
            {
                uint64_t file_size =  assetIndex->getIndexFileSize();  

                AssetIndexHeaderVector& h_list = assetIndex->getHeaderList();

                uint32_t size ( h_list.size() );
                uint32_t ai_size ( MainIndexHeader::SIZE );

                for ( size_t i ( 0 ); i < size; ++i )
                {
                    AssetIndexHeader* header = h_list[i];
                    ai_size += header->m_indexCurDuration * AssetIndexHeader::ASSET_INDEX_DATA_SIZE + AssetIndexHeader::HEADER_SIZE;
                }

                indexIsReady = file_size == ai_size;
            }
            else
                indexIsReady = crc == currCrc;

            string firstMainFileName = assetIndex->getFirstMainFileName();

            if ( ( firstMainFileName.empty() || !fileExists ( firstMainFileName ) ) && indexIsReady && assetLength != 0 )
                indexIsReady = assetLength == currAssetLength;
        }
        
        delete assetIndex;
        
        if ( !indexIsReady )
        {
            LTRACE ( LT_WARN, 0, "Invalid index file ( " << ( crc == 0 ? "file contet check failed" : "CRC check failed" ) <<  " ) of asset '" <<  fileName << "'. Asset will be re-indexed. Line" << __LINE__ )
                aim->deleteFileIndex ( fileName );
        }
        else
            rez = 1000;
    }

    return (int)rez;
}

LIB_EXPORT bool LIB_STD_CALL getEncodedAssetList(char*rezBuffer, int bufferSize)
{
	if (vodApplication.stopped) return false;
	AssetIndexManager* indexManager = AssetIndexManager::getInstance();
	return indexManager->getEncodedAssetList(rezBuffer, bufferSize);
}

LIB_EXPORT unsigned LIB_STD_CALL createIndexFileAsync(const char* displayName, const char* fileName, bool createRewind)
{
    if (vodApplication.stopped)
        return 0;

	if (AssetIndexManager::getInstance()->isReadOnlyIndexes()) {
		LTRACE(LT_WARN, 0, "Can't start file indexing for file " << fileName << " because readOnly mode selected.");
		return 0;
	}

	try {
		AssetIndexer indexer;
		int rez = indexer.createIndexFileAsync(displayName, fileName, getFileReader(), createRewind);
		return rez;
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't start indexing: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at createIndexFileAsync");
	}
	return 0;
}

LIB_EXPORT int LIB_STD_CALL deleteAsset(const char* fileName)
{
    int res = -5;

	try 
	{
	    if ( !vodApplication.stopped )
        {
            if ( AssetIndexManager::getInstance()->deleteFileIndex ( fileName ) )
            {
                File file;

                if ( fileExists ( fileName ) )
                {
                    if ( deleteFile(fileName ) )
                    {
                        LTRACE ( LT_WARN, 0, "Media file " << fileName << " deleted successfully" )
                        res = 0;
                    }
                    else
                    {
                        LTRACE ( LT_WARN, 0, "Can't delete media file " << fileName )
                        res = -1;
                    }

                }
                else
                {
                    LTRACE ( LT_WARN, 0, "File " << fileName << "doesn't exist")
                    res = -2;
                }
            }
            else
            {
                LTRACE(LT_WARN, 0, "Can't delete index files for asset: " << fileName )
                res = -3;
            }
        }
        else 
            res = -4;
	}
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Vod core exception at deleteAsset: " << e.m_errStr << ". Line: " << __LINE__ )
    }
    catch ( exception& e ) 
    {
        LTRACE(LT_ERROR, 0, "Exception at deleteAsset: " << e.what() << ". Line: " << __LINE__ )
    } 
	catch (...) 
	{
		LTRACE(LT_ERROR, 0, "Unknown exception at deleteAsset for: " << fileName << ". Line: " << __LINE__ )
	}
	
	return res;
}

LIB_EXPORT int LIB_STD_CALL deleteStreamingGroup(const char* groupName)
{
	if (vodApplication.stopped) return 0;
	LTRACE(LT_DEBUG, 0, "deleteStreamingGroup called. groupName:" << groupName);
	try {
		int rez =  vodApplication.streamer->removeStreamingGroup(groupName);
		LTRACE(LT_DEBUG, 0, "deleteStreamingGroup finished return code: " << rez);
		return rez;
	} catch(VodCoreException& e) {
		LTRACE(LT_ERROR, 0, "Can't create streaming group: " << e.m_errStr);
    } catch ( exception& e) {
		LTRACE(LT_ERROR, 0, e.what());
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at deleteStreamingGroup");
	}
	return -1;
}

LIB_EXPORT int LIB_STD_CALL getRewindSpeed(int requestSpeed)
{
	if (vodApplication.stopped) return 0;
	if (requestSpeed > 1)
		return DEFAULT_REWIND_SPEED;
	if (requestSpeed < 0)
		return -DEFAULT_REWIND_SPEED;
	else
		return 1;
}

LIB_EXPORT long LIB_STD_CALL multicastJoin(const char* assetName, const char* address, int port, 
										  bool overWrite, int cycleBufferLen, const char* ixface,
										  bool createRewind, int nBlockSize, int splitByDurationSize)
{
    LTRACE ( LT_DEBUG, 0, "vodTransport::multicastJoin called. assetName:" << assetName << ", source=" << address << ":" << port )
    
    int res = -4;
    
	try 
    {
        if ( !vodApplication.stopped )
        {
		    if ( !overWrite && fileExists ( assetName ) ) 
		    {
		        res = -2;
                LTRACE ( LT_WARN, 0, "vodTransport::multicastJoin: file '" << assetName << "' exists and can't be overwrited because param overWrite is false. Line: " << __LINE__ )
            }
            else
            {
				Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
                map < string, MulticastEncoder*>::iterator itr =  vodApplication.multicastEncoders.find ( assetName );
                if ( itr == vodApplication.multicastEncoders.end())
                {
					if (nBlockSize == 0) {
						if (splitByDurationSize)
							nBlockSize = DEFAULT_ROUND_BLOCK_SIZE;
						else
							nBlockSize = DEFAULT_FILE_BLOCK_SIZE;
					}
                    MulticastEncoder* rez = MulticastEncoder::addEncoder(address, port, vodApplication.getBufferedFileWriter(), ixface, nBlockSize );

                    rez->setCreateRewind(createRewind);

                    res = rez->start(assetName, cycleBufferLen, splitByDurationSize);

                    if ( res == 0) 
                        vodApplication.multicastEncoders.insert ( make_pair(assetName, rez));
                    else 
                        delete rez;
                }
                else
                    res = -1;
            }
        }
        else
            res = -3;
            
	}
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Vod core exception at vodTransport::multicastJoin: " << e.m_errStr << ". Line: " << __LINE__ )
    }
    catch ( exception& e ) 
    {
        LTRACE(LT_ERROR, 0, "Exception at vodTransport::multicastJoin: " << e.what() << ". Line: " << __LINE__ )
    } 
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at vodTransport::multicastJoin for: " << assetName << ". Line: " << __LINE__ )
    }

	return res;
}

#ifdef USE_RAPTOR_FEC
static std::vector<RaptorEncoder*> raptorEncoders;
#endif

LIB_EXPORT void LIB_STD_CALL startRaptorEncoding()
{
#ifdef USE_RAPTOR_FEC
	for (int i = 0; i < raptorEncoders.size(); ++i) {
		raptorEncoders[i]->start();
	}
#endif
}

static vector < MulticastRetransmitter* > mcstRetransmitters;

LIB_EXPORT void LIB_STD_CALL retransmitStream ( const char* srcAddress, const char* srcIxface, const char* dstAddress, const char* fwdAddress, bool emulateLack )
{
    MulticastRetransmitter* mr = MulticastRetransmitter::create ( srcAddress, srcIxface, dstAddress, fwdAddress, emulateLack );
    
    if ( mr )
        mcstRetransmitters.push_back ( mr );
}

LIB_EXPORT long LIB_STD_CALL createRaptorStream(const char* srcAddress, int srcPort, const char* srcIxface,
												const char* dstAddress, int dstPort, const char* dstIxface,
												int repairSymbolCount, int symbolCount)
{
	LTRACE ( LT_DEBUG, 0, "vodTransport::createRaptorStream called. srcAddress:" << srcAddress << ":" << srcPort << ", dstAddress=" << dstAddress << ":" << dstPort )
	
#ifdef USE_RAPTOR_FEC
	
	try 
    {
        if ( !vodApplication.stopped )
        {
			RaptorEncoder* currentEncoder = 0;
			for (int i = 0; i < raptorEncoders.size(); ++i) {
				if (raptorEncoders[i]->canAcceptSource(srcAddress, srcPort)) {
					currentEncoder = raptorEncoders[i];
					break;
				}
			}
			if (currentEncoder == 0) {
				raptorEncoders.push_back(new RaptorEncoder());
				currentEncoder = raptorEncoders[raptorEncoders.size()-1];
			}

            currentEncoder->addEncoder(srcAddress, srcPort, srcIxface, dstAddress, dstPort, dstIxface, repairSymbolCount, symbolCount);
			//return (long) rez;
			return raptorEncoders.size();
		}
	}
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Vod core exception at vodTransport::createRaptorStream: " << e.m_errStr << ". Line: " << __LINE__ )
    }
    catch ( exception& e ) 
    {
        LTRACE(LT_ERROR, 0, "Exception at vodTransport::createRaptorStream: " << e.what() << ". Line: " << __LINE__ )
    } 
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at vodTransport::createRaptorStream for: srcAddress=" << srcAddress << ". Line: " << __LINE__ )
    }
    
#endif
        
	return -1;
}


LIB_EXPORT int LIB_STD_CALL multicastLeave(const char* encoderName)
{
	int res = -3;
	
	LTRACE ( LT_DEBUG, 0, "vodTransport::multicastLeave called. assetName:" << encoderName )
	
	try 
	{
	    if ( !vodApplication.stopped )
        {
			Mutex::ScopedLock lock(&vodApplication.multicastEncodersLock);
            map < string, MulticastEncoder*>::iterator itr =  vodApplication.multicastEncoders.find (encoderName );
            if (itr != vodApplication.multicastEncoders.end())
            {
                MulticastEncoder* enc = itr->second;
                
                if ( enc )
                {
                    enc->terminate();

                    delete enc;
                    enc = NULL;

                    vodApplication.multicastEncoders.erase ( encoderName );

                    res = 0;
                }
            }
            else
                res = vodApplication.streamer->GROUP_NOT_FOUND;
        }
        else
            res = -1;
	}
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Vod core exception at multicastLeave: " << e.m_errStr << ". Line: " << __LINE__ )
    }
    catch ( exception& e ) 
    {
        LTRACE(LT_ERROR, 0, "Exception at multicastLeave: " << e.what() << ". Line: " << __LINE__ )
    } 
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at multicastLeave for: " << encoderName << ". Line: " << __LINE__ )
    }

    LTRACE ( LT_DEBUG, 0, "vodTransport::multicastLeave finished" )            
	return res;
}

int assertBreak = 0;

LIB_EXPORT void LIB_STD_CALL setReadOnlyIndexes(bool value)
{
	AssetIndexManager::getInstance()->setReadOnlyIndexes(value);
}

LIB_EXPORT void LIB_STD_CALL initLog(int messageLevel, 
									 const char* logName,
									 const char* logDir,
									 int logRotationPeriod)
{


	if (vodApplication.stopped) return;


	SystemLog::initializeSystemLog(
			messageLevel, 
			logName,
			logDir,
			logRotationPeriod*60,
			50*1024*1024,
			true,
            AlternativeFileStream::Day,
			LogStream::MessageLevel | LogStream::MessageDate | LogStream::MessageTime | LogStream::MessageThreadID, "");
	LTRACE(LT_INFO, 0, "SmartMedia core. version="  << VERSION_STR);
	//assert(1 == 0);
}

LIB_EXPORT void LIB_STD_CALL setReadThreadCnt(int readThreadCnt)
{
	if (vodApplication.stopped) return;
	vodApplication.tsAlignedReadManager->setReadThreadsCnt(readThreadCnt);
	vodApplication.unalignedReadManager->setReadThreadsCnt(readThreadCnt);
}

LIB_EXPORT int LIB_STD_CALL getAssetInfoFromFile ( const char* fileName, char* dstBuffer, int bufSize )
{
    int res = -3;
    
    *dstBuffer = 0;
    
	ostringstream stream;
	
	try 
	{
	    if ( !vodApplication.stopped )
	    {
		    pair<AVChapters, vector<CheckStreamRez> > fullRez = METADemuxer::DetectStreamReader(*vodApplication.tsAlignedReadManager, fileName);
		    vector<CheckStreamRez>& rez = fullRez.second;
		    
		    int duration = getTSDuration(fileName);
		    
		    stream << "Asset duration: " << duration << endl;

		    uint64_t fileSize = 0;
		    
		    File tmpFile(fileName, File::ofRead);
		    
		    tmpFile.size(&fileSize);
		    tmpFile.close();

		    double bitrate = 0;
		    
		    if (duration > 0)
			    bitrate = fileSize / (double) duration  / 1024.0 * 8;
			    
		    stream << "Asset bitrate: " << bitrate << endl;

		    for ( size_t i = 0; i < rez.size(); i++)
		    {
			    if (rez[i].trackID != 0)
				    stream << "Track ID:    " << rez[i].trackID << endl;
			    if (rez[i].codecInfo.codecID) {
				    stream << "Stream type: " << rez[i].codecInfo.displayName << endl;
				    stream << "Stream ID:   " << rez[i].codecInfo.programName << endl;
				    stream << "Stream info: " << rez[i].streamDescr << endl;
			    }
			    else
				    stream << "Can't detect stream type" << endl;
		    }
		    
            
        }
        else
            res = -1;
            
        if ( stream.str().size() > ( size_t )bufSize ) 
        {
            res = -2;
            
            LTRACE ( LT_WARN, 0, "vodTransport::getAssetInfoFromFile: too small out buffer size. Line: " << __LINE__ )
        }
	} 
    catch ( VodCoreException& e ) 
    {
        LTRACE ( LT_ERROR, 0, "Vod core exception at getAssetInfoFromFile: " << e.m_errStr << ". Line: " << __LINE__ )
    }
    catch ( exception& e ) 
    {
        LTRACE(LT_ERROR, 0, "Exception at getAssetInfoFromFile: " << e.what() << ". Line: " << __LINE__ )
    } 
    catch (...) 
    {
        LTRACE(LT_ERROR, 0, "Unknown exception at getAssetInfoFromFile for: " << fileName << ". Line: " << __LINE__ )
    }
    
    if ( res != 0  )
    {    
        stream.clear();
        stream << "Error: " << res  << endl;
    }

    memcpy ( dstBuffer, stream.str().c_str(), stream.str().size() );

    return res;	
}

LIB_EXPORT void registerEventHandler(EVENT_HANDLER_PROC eventHandler)
{
	if (vodApplication.stopped) return;
	vodApplication.eventHandler = eventHandler;
	vodApplication.streamer->setEventHandler(eventHandler);
}

void LIB_STD_CALL testHandler(long handle, int eventID, const char* message)
{
	//
}

string closeSlash(const string& str) {
	if (str.size() > 0 && (str[str.size()-1] == '/' || str[str.size()-1] == '\\'))
		return str;
	else
		return str + '\\';
}


unsigned LIB_STD_CALL testRuTubeStreaming( uint32_t nPort, const string& sVideoID )
{ 
    vodCoreContext->setRTCacheParams ( "./RuCache", 3600 * 24 * 365 );
    vodCoreContext->disableRTCache ();

    string sPort = int32uToStr ( nPort );

    /*char dstBuffer[1024*2];
    getAssetDescription ( sPort.c_str(), ( "rutube://" + sVideoID ).c_str(), 0, dstBuffer, sizeof(dstBuffer));
    */
	int mcasthandle = multicastJoin("tstv2.ts", "239.255.2.100", 5500, true, 20, 0, true, 0,0);
	Process::sleep(1000 * 30);
    int sHandle = startStreaming ( 0, StreamingContext::llTransportRAW, -2, 1, "tstv2.ts", "127.0.0.1", 35000, 0, 0);
	Process::sleep(1000 * 5);
	int pos = getNptPos(sHandle);
	LTRACE(LT_DEBUG, 0, "npt pos before pause:" << pos);
	pauseStreaming(sHandle);
	Process::sleep(1000 * 5);
	pos = getNptPos(sHandle);
	LTRACE(LT_DEBUG, 0, "npt pos after pause:" << pos);
	resumeStreaming(sHandle);
	pos = getNptPos(sHandle);
	LTRACE(LT_DEBUG, 0, "npt pos after resume:" << pos);
	while (1) {
		Process::sleep(1000 * 5);
		LTRACE(LT_DEBUG, 0, "npt pos after resume 2:" << pos);
	}

	//setPosition(sHandle, -8);
	//Process::sleep(1000 * 2);
	//int pos = getNptPos(sHandle);
	Process::sleep(1000 * 2000);


    //unsigned handle = startStreaming ( sPort.c_str(), StreamingContext::llTransportRAW, 0, 1, ( "rutube://" + sVideoID /*+ "&pos=20"*/ ).c_str(), "127.0.0.1", nPort, 0, 0 );

    unsigned handle = startStreaming ( sPort.c_str(), StreamingContext::llTransportRAW, 0, 1, ( "rutube://" + sVideoID /*+ "&pos=20"*/ ).c_str(), "127.0.0.1", nPort, 0, 0 );

    return handle;
}

unsigned LIB_STD_CALL testYouTubeStreaming( uint32_t nPort, const string& sVideoID )
{ 
    // Сохранение
    /*string fullUrl = "youtube://" + sVideoID + "&pos=14";

    TSMuxer* tsMuxer = new TSMuxer ( ytReadManager );

    tsMuxer->setFileBlockSize ( YT_DEFAULT_PREBUFFERING_TIME*2 );
    tsMuxer->setAsyncMode ( false );
    tsMuxer->setNewStyleAudioPES ( false );

    map<string, string> videoParams;

    videoParams["track"] = "1";

    tsMuxer->addStream ( "V_MPEG-2", fullUrl.c_str(), videoParams );

    map<string, string> audioParams;

    audioParams["track"] = "2";

    tsMuxer->addStream ( "A_MP3", fullUrl.c_str(), audioParams );
    tsMuxer->parseMuxOpt ( string ( "--vbv-len=350 --maxbitrate=" ) + int32ToStr ( YOUTUBE_BITRATE ) );

    tsMuxer->doMux ( "test" + int32ToStr ( nPort )  +".ts", true );

    return 0;*/
    //Вещание
//    vodCoreContext->setYTCacheParams ( "C:/MediaCache", "./log", 24 * 3600 * 365 );
    vodCoreContext->setYTCacheParams ( "C:/rtspServer/Media", "./log", 3600 * 24 * 365 );
    vodCoreContext->setYTScalingType ( "ffmpeg" );
    vodCoreContext->setYTVideoQuality ( "high" );
	vodCoreContext->setYTVideoSize ( 0, 0, true );
	vodCoreContext->setYTLogoPath ( "C:/rtspServer/Media/logo/in5.png" );
	vodCoreContext->setYTLogoPos ( makeXY ( 0, 0 ) );
	
    vodCoreContext->disableYTCache ();
    //vodCoreContext->enableCacheDemoMode();
    //vodCoreContext->disableYTMp4Transcoding ();
    
    char dstBuffer[1024*2];
    string sPort = int32uToStr ( nPort );
    //getAssetDescription ( sPort.c_str(), ( "youtube://" + sVideoID ).c_str(), 0, dstBuffer, sizeof(dstBuffer));
    //vodCoreContext->setMediaInterface ( "10.40.1.30" );

    unsigned handle = startStreaming ( sPort.c_str(), StreamingContext::llTransportRAW, 0, 1, ( "youtube://" + sVideoID /*+ "&pos=20"*/ ).c_str(),
                                       "127.0.0.1", nPort, 0, 0 );

    //vodCoreContext->setYTCacheParams ( "c:/rtspSrever/MediaCache", "c:/rtspSrever/log", 3600 * 24 * 365 );
    //
    //unsigned handle = startStreaming ( StreamingContext::llTransportRAW, 0, 1, ( "bbc://" + sVideoID ).c_str(),
    //                                   "127.0.0.1", nPort, 0, 0 );

    return handle;
}

int LIB_STD_CALL muxTSFile( map<int, MuxTrackInfo> trackInfo,
							const string& outName,
							int   indexingType, int nBitrate, int nVBVLen, int nFileBlockSize )
{
	int res = -1;
	
	try 
	{
		if ( !vodApplication.stopped )
		{
			TSMuxer* tsMuxer = NULL;

			if ( nFileBlockSize == YT_DEFAULT_PREBUFFERING_TIME*2 )
				tsMuxer = new TSMuxer( *vodApplication.ytReadManager );
			else
				tsMuxer = new TSMuxer(*vodApplication.tsAlignedReadManager);

			tsMuxer->setFileBlockSize ( nFileBlockSize, nFileBlockSize );
			tsMuxer->setAsyncMode(false);
			tsMuxer->setNewStyleAudioPES(false);

			tsMuxer->parseMuxOpt ( string ( "--vbv-len=" ) + int32ToStr ( nVBVLen ) + string ( " --maxbitrate=" ) + doubleToStr( nBitrate/1000.0 ) );        

			for (map<int, MuxTrackInfo>::const_iterator itr = trackInfo.begin(); itr != trackInfo.end(); ++itr)
			{
				map<string, string> params;
				params["track"] = int32ToStr(itr->first);
				const string& codecName = itr->second.codecName;
				for (map<string, string>::const_iterator paramItr = itr->second.addParam.begin(); 
					paramItr != itr->second.addParam.end(); ++paramItr)
				{
					params[paramItr->first] = paramItr->second;
				}
				tsMuxer->addStream(codecName.c_str(), itr->second.streamName, params);
			}
			AssetIndexer* indexer = 0;
			if (indexingType > 0) {
				indexer = new AssetIndexer ( indexingType > 1, nFileBlockSize );
				indexer->initAssetEncoding(outName.c_str());
				tsMuxer->setIndexer(indexer);
			}
			tsMuxer->doMux(outName);
			tsMuxer->close();
			
			delete tsMuxer;
			
			if (indexer) {
				indexer->flush();
				delete indexer;
			}

			res = 0;
		}
	} 
	catch (VodCoreException& e) 
	{
		LTRACE(LT_ERROR, 0, e.m_errStr)
	} 
	catch (BitStreamException& e) 
	{
		LTRACE(LT_ERROR, 0, "Bitstream exception in muxTSFile: " << e.what() )
		res = -2;
	}
	catch  ( exception& e ) 
	{
		LTRACE(LT_ERROR, 0, e.what())
		res = -3;
	}
	catch (...) 
	{
		LTRACE(LT_ERROR, 0, "Unknown exception in muxTSFile")
		res = -4;
	}

	return res;
}

LIB_EXPORT void LIB_STD_CALL setTsMtuSize ( const int value )
{
    if (vodApplication.stopped) return;

    vodCoreContext->setTsMtuSize ( value );
}
LIB_EXPORT void LIB_STD_CALL setMaxMIpPacketCount ( const int value )
{
    if (vodApplication.stopped) return;

    vodCoreContext->setMaxMIpPacketCount ( value );
}

LIB_EXPORT void LIB_STD_CALL setMaxUIpPacketCount ( const int value )
{
    if (vodApplication.stopped) return;

    vodCoreContext->setMaxUIpPacketCount ( value );
}

LIB_EXPORT void LIB_STD_CALL enableBlockSendingBySinglePackets()
{
    if (vodApplication.stopped) return;

    vodCoreContext->enableBlockSendingBySinglePackets();
}

LIB_EXPORT void LIB_STD_CALL enableCacheDemoMode() 
{
    if (vodApplication.stopped) return;

    vodCoreContext->enableCacheDemoMode();
}

LIB_EXPORT void LIB_STD_CALL setTrafficBandWidth ( int value )
{
    if (vodApplication.stopped) return;

    vodCoreContext->setTrafficBandWidth ( value );

}

LIB_EXPORT void LIB_STD_CALL setYTLogoPos ( uint32_t pos )
{
    if ( !vodApplication.stopped )
        vodCoreContext->setYTLogoPos ( pos );    
}

LIB_EXPORT void LIB_STD_CALL setYTLogoPath ( const string& path )
{
    if ( !vodApplication.stopped )
        vodCoreContext->setYTLogoPath ( path );    
}


LIB_EXPORT void LIB_STD_CALL setYTCacheParams ( const string&  sCacheDirectory, const string&  sLogDirectory, uint64_t nTotalCacheDuration )
{
	if (vodApplication.stopped) return;
    vodCoreContext->setYTCacheParams ( sCacheDirectory, sLogDirectory, nTotalCacheDuration );
}

LIB_EXPORT void LIB_STD_CALL setRTCacheParams ( const string&  sCacheDirectory, uint64_t nTotalCacheDuration )
{
    if (vodApplication.stopped) return;
    vodCoreContext->setRTCacheParams ( sCacheDirectory, nTotalCacheDuration );
}

LIB_EXPORT void LIB_STD_CALL getYTVideoInfo ( const string&  streamName, uint8_t& existingType, double& fps, bool cleanUp )
{
   if (vodApplication.stopped) return;

   vodCoreContext->getYTVideoInfo ( streamName, existingType, fps, cleanUp );
}

LIB_EXPORT void LIB_STD_CALL getRTVideoInfo ( const string&  streamName, uint8_t& existingType, double& fps, bool cleanUp )
{
    if (vodApplication.stopped) return;

    vodCoreContext->getRTVideoInfo ( streamName, existingType, fps, cleanUp );
}

LIB_EXPORT void LIB_STD_CALL setYTDomesticHost ( const string& sDomesticHost )
{
	if (vodApplication.stopped) return;
    LTRACE ( LT_DEBUG, 0, "setYTDomesticHost called. sDomesticHost: " << sDomesticHost )
    vodCoreContext->setYTDomesticHost ( sDomesticHost );
}

LIB_EXPORT void LIB_STD_CALL setYTVideoQuality ( const string& sQuality )
{
	if (vodApplication.stopped) return;
    vodCoreContext->setYTVideoQuality ( sQuality );
}

LIB_EXPORT void LIB_STD_CALL setYTScalingType  ( const string& sType )
{
	if (vodApplication.stopped) return;
    vodCoreContext->setYTScalingType ( sType );
}

LIB_EXPORT void LIB_STD_CALL setRTScalingType  ( const string& sType )
{
    if (vodApplication.stopped) return;
    vodCoreContext->setRTScalingType ( sType );
}


LIB_EXPORT void LIB_STD_CALL disableYTCache ()
{
    if (vodApplication.stopped) return;
 
    vodCoreContext->disableYTCache ();
}

LIB_EXPORT void LIB_STD_CALL disableRTCache ()
{
    if (vodApplication.stopped) return;

    vodCoreContext->disableRTCache ();
}

LIB_EXPORT void LIB_STD_CALL setYTVideoSize ( const uint32_t width, const uint32_t height, const bool autoPal )
{
    if (vodApplication.stopped) return;

    vodCoreContext->setYTVideoSize ( width, height, autoPal );
}

LIB_EXPORT void LIB_STD_CALL setRTVideoSize ( const uint32_t width, const uint32_t height, const bool autoPal )
{
    if (vodApplication.stopped) return;

    vodCoreContext->setRTVideoSize ( width, height, autoPal );
}

LIB_EXPORT void LIB_STD_CALL disableYTMp4Transcoding()
{
    if (vodApplication.stopped) return;

    vodCoreContext->disableYTMp4Transcoding();
}


LIB_EXPORT void LIB_STD_CALL setMediaInterface ( const string& sMediaInterface )
{
	if (vodApplication.stopped) return;
    LTRACE ( LT_DEBUG, 0, "setMediaInterface called. sMediaInterface: " << sMediaInterface )
    vodCoreContext->setMediaInterface ( sMediaInterface );
}

LIB_EXPORT void LIB_STD_CALL getYTDomesticHost ( char* pDstBuff, uint32_t& nDstBuffSize )
{
	if (vodApplication.stopped) return;
    LTRACE ( LT_DEBUG, 0, "getYTDomesticHost called." )

    string sRes;

    typedef void ( LIB_STD_CALL *GetDomesticHostFuncPtr )( char*, uint32_t& );
    GetDomesticHostFuncPtr pfGetDH;

    DynamicLink* pDLink = PluginContainer::loadLibrary ( "youtube" );

    if (!pDLink)
        return;

#ifdef WIN32
    void* sym = pDLink->getSym("_getDomesticHost@8");
#else
    void* sym = pDLink->getSym("getDomesticHost");
#endif
    pfGetDH = ( GetDomesticHostFuncPtr )sym;
    
    ( *pfGetDH )( pDstBuff, nDstBuffSize );
}

LIB_EXPORT bool LIB_STD_CALL checkSmartBridgeCache ( const char* streamName )
{
    bool res = false;

    if ( vodApplication.stopped ) return res;

    LTRACE ( LT_DEBUG, 0, "vodTransport::checkSmartBridgeCache called." )

    string prefix = extractPrefix ( streamName );

    if ( prefix != "igmp" && prefix != "radio" && prefix != "dvtube" )
    {
        typedef bool ( LIB_STD_CALL *CheckCacheFuncPtr )( AbstractCoreContext*, const char* );

        CheckCacheFuncPtr pfCheckCache;
        int pos = prefix.size() + 3;
        int len = prefix == "rutube" ? 32 : ( prefix == "bbc" ? 8 : 11 );

        string videoID = string ( streamName ).substr ( pos, len );

        DynamicLink* dlink = PluginContainer::loadLibrary ( prefix.c_str() );

        if ( dlink )
        {

#ifdef WIN32
            void* sym = dlink->getSym("_checkCache@8");
#else
            void* sym = dlink->getSym("checkCache");
#endif

            pfCheckCache = ( CheckCacheFuncPtr )sym;

            res = ( *pfCheckCache )( vodCoreContext, videoID.c_str() );
        }
    }

    return res;
}

unsigned LIB_STD_CALL startSmartBridgeStreaming ( const char* sessionID,
                                                  double ntpPos, 
											      const char* url, 
											      const char* address, 
											      unsigned short port, 
                                                  const char* rtpInfo,
                                                  long tcpSocketHandle
											    )
{
	if ( vodApplication.stopped ) return 0;

    uint32_t handle ( 0 );

    try
    {
        string fullUrl = url;
        string sid;

		if (sessionID)
			sid = sessionID;

        int32_t nPos = fullUrl.find ( "&pos=" );

        if ( ntpPos > 0 && nPos == string::npos )
        {
            fullUrl += "&pos=";
            fullUrl += doubleToStr(ntpPos);
        }

        //if ( !sid.empty() )
          //  fullUrl += "&sid=" + sid;

        TSMuxer* tsMuxer = NULL;
        
        SmartBridge* bridge = NULL;
        
        SmartBridgeSource sb = strStartWith ( url, "radio://" ) ? sbsRadio : ( strStartWith ( url, "rutube://" ) ? sbsRT :
                                                                      ( strStartWith ( url, "youtube://" ) ? sbsYT :
                                                                      ( strStartWith ( url, "udp://" ) ? sbsUdp : 
                                                                      ( strStartWith ( url, "dvtube:/" ) ? sbsDVT : 
                                                                      ( strStartWith ( url, "bbc://" ) ? sbsBBC : sbsNone ) ) ) ) );
            
		string memName = "";
		
		if ( sb == sbsDVT || sb == sbsUdp )
		{
		    bridge = new SmartBridge ( fullUrl );
		    
		    memName = bridge->getDestination();

		    if ( !memName.empty() && bridge->isStatic() )
		    {
		        delete bridge;
		        bridge = NULL;
		    }
		    else
		        memName =  bridge->start();
        }
		else
		{
		    tsMuxer = new TSMuxer ( *vodApplication.ytReadManager );
		    memName = startBridgeMuxing ( sb, fullUrl, tsMuxer );
        }

		StreamingContext* context = 0;
		
        handle = vodApplication.streamer->startStreaming ( sessionID, *vodApplication.ytReadManager, memName.c_str(), ntpPos, 1, SocketAddress ( IPAddress ( address ), port ),
                                           StreamingContext::llTransportRAW, rtpInfo, ( TCPSocket* )tcpSocketHandle,
                                           context 
                                         );

        if ( context )
        {
            if ( tsMuxer )
                context->setMuxer(tsMuxer);
            else if ( bridge )
                context->setSmartBridge( bridge );
            
            context->setURL ( fullUrl );
        }

    }
    catch ( SocketException& e )
    {
        LTRACE ( LT_DEBUG, 0, "DNM_T vodTransport::startSmartBridgeStreaming exception" )
    }
    catch (...)
    {
        LTRACE ( LT_DEBUG, 0, "DNM_T vodTransport::startSmartBridgeStreaming exception" )
    }
	
    return handle;
}

#ifdef WIN32

int32_t getInitialDelay(const string& toolsRoot, const string& groupName)
{
	int32_t lastDelay = 0;
	TextFile file;
	if (file.open((closeSlash(toolsRoot) + "stream_delay.txt").c_str(), File::ofRead))
	{
		string line;
		while (file.readLine(line))
		{
			if (line.size() == 0)
				continue;
			size_t pos = line.find(string("program:")+groupName);
			if (pos != string::npos) {
				pos = line.find("delay: ");
				if (pos != string::npos)
					lastDelay = strToInt32(line.substr(pos + 7).c_str());
			}
		}
	}
	return lastDelay;
}

//static const int ENCODING_MAX_STREAMING_BITRATE = 2500;
static const double TS_BITRATE_COEFF = 1.30f;

LIB_EXPORT unsigned LIB_STD_CALL startEncodingServer ( const char* xmlConfigFileName )
{
	if (vodApplication.stopped) return 0;
    LTRACE ( LT_DEBUG, 0, "startEncodingServer called." )

    string sFileName;
	if (xmlConfigFileName == 0)
		sFileName = "encodingServerCfg.xml";
	else
		sFileName = xmlConfigFileName;

    TiXmlDocument xmlDoc ( sFileName );
    xmlDoc.LoadFile ( sFileName );
    
    TiXmlElement* pConfig = xmlDoc.FirstChildElement( "Config" );

    string v = pConfig->Attribute ( "videoBitrate" );
    v = v.empty() ? "0" : v;
    
    int encVideoBitrate = strToInt32 ( v.c_str() );
    
    v = pConfig->Attribute ( "audioBitrate" );
    v = v.empty() ? "0" : v;
    
    int encAudioBitrate = strToInt32 ( v.c_str() );
    
    v = pConfig->Attribute ( "tsBitrate" );
    v = v.empty() ? "0" : v;
    
    double tsBitrate = strToInt32 ( v .c_str());

    v = pConfig->Attribute ( "useRaptor" );
    v = v.empty() ? "false" : v;
    
    v = strToLowerCase ( v );
    
    bool useRaptor = v == "true" || v == "1" || v == "yes" || v == "да" || v == "t" || v == "y" || v == "д";
    
    v = pConfig->Attribute ( "repairSymbolCount" );
    v = v.empty() ? "0" : v;
    
    int repairSymbolCount = strToInt32 ( v.c_str() );
    
    v = pConfig->Attribute ( "codedBlockSize" );
    v = v.empty() ? "0" : v;
    
    int codedBlockSize = strToInt32 ( v.c_str() );
    
    v = pConfig->Attribute ( "vbvBuffer" );
    v = v.empty() ? "0" : v;

    int coderVBVBufferSize = strToInt32 ( v.c_str() );
        
    repairSymbolCount = repairSymbolCount <= 0 ? FEC_R : repairSymbolCount;
    codedBlockSize = codedBlockSize <= 0 ? FEC_K : codedBlockSize;
    tsBitrate = tsBitrate <= 0 ? (encVideoBitrate + encAudioBitrate) * TS_BITRATE_COEFF : tsBitrate;
	coderVBVBufferSize = coderVBVBufferSize <= 0 ?encVideoBitrate/2 : coderVBVBufferSize;
		
	double vbvBufferLength = 1000.0 * (double)coderVBVBufferSize / encVideoBitrate;

	const string defaultEncodePattern = "--level 3.1 --keyint 25 --min-keyint 5 --subme 6 --bframes 2 --ref 3 --analyse p8x8,b8x8,i4x4,p4x4 --ratetol 0.1 --qcomp 0.1 --me umh --threads 0 --thread-input --no-psnr --no-ssim --sar 48:45";
	
    string encodePattern = pConfig->Attribute ( "encodePattern" );
    
	if (encodePattern.empty())
		encodePattern = defaultEncodePattern;

    string toolsDir = closeDirPath ( pConfig->Attribute ( "toolsDir" ) );

    TextFile batFile; 
    
    if ( batFile.open ( ( toolsDir + "capture_all.bat" ).c_str(), File::ofWrite ) )
    {
        //batFile.writeLine ( ( "x264.exe --bitrate " + int32ToStr ( encVideoBitrate ) + " --vbv-bufsize " + int32ToStr ( encVideoBitrate/2 ) + " --vbv-maxrate " + int32ToStr ( encVideoBitrate ) + " --level 3.1 --keyint 25 --min-keyint 5 --subme 6 --bframes 2 --ref 3 --analyse p8x8,b8x8,i4x4,p4x4 --ratetol 0.1 --qcomp 0.1 --me umh --threads 0 --thread-input --no-psnr --no-ssim --sar 48:45 --output \\\\.\\pipe\\sm_encoded_video " + toolsDir + "capture_all.avs" ).c_str() );
		batFile.writeLine ( ( "x264.exe --bitrate " + int32ToStr ( encVideoBitrate ) + string(" ") + encodePattern + " --vbv-bufsize " + int32ToStr (coderVBVBufferSize) + " --vbv-maxrate " + int32ToStr ( encVideoBitrate ) + " --output \\\\.\\pipe\\sm_encoded_video " + toolsDir + "capture_all.avs" ).c_str() );
        batFile.close();
    }

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	execProcessAsync ( closeSlash ( toolsDir ) + "capture_all.bat", "");

	//Process::sleep(3000);
	
    TSMuxer* tsMuxer = new TSMuxer(*vodApplication.tsAlignedReadManager);
	
    tsMuxer->parseMuxOpt("--maxbitrate=" + doubleToStr ( tsBitrate ) );
	LTRACE(LT_DEBUG, 0, "TS mux bitrate:" << tsBitrate << ". ts VBV buffer=" << vbvBufferLength);
    tsMuxer->parseMuxOpt("--vbv-len=" + int32ToStr((int32_t)vbvBufferLength));
    tsMuxer->setAsyncMode(false);
    tsMuxer->setNewStyleAudioPES(false);
    
    tsMuxer->setAsyncMode(false);
	tsMuxer->setNewStyleAudioPES(false);
	
    map<string, string> audioParams;
	tsMuxer->addStream("A_AAC", "pipe://sm_encoded_audio", audioParams);
	
    map<string, string> h264Params;
	tsMuxer->addStream("V_MPEG4/ISO/AVC", "pipe://sm_encoded_video", h264Params);
	tsMuxer->doMux("memory://sm_encoded_ts", true);

    StreamingContext* context = NULL;
    
    if ( useRaptor )
    {
        //repairSymbolCount, codedBlockSize
        context = NULL;
    }
    else
        context = vodApplication.streamer->createStreamingGroup ( *vodApplication.tsAlignedReadManager, "memory://sm_encoded_ts", "live_encoding",
                                                                  0, 1, StreamingContext::llTransportRAW, 0, 0, 0, false );
    
    if ( context )                                                              
	{
	    ((StreamingContextRTP*)context)->setVBVMode(StreamingContextRTP::VBV_BITRATE_AUTO, (int)vbvBufferLength); 
        TiXmlElement* pDest = pConfig->FirstChildElement ( "dest" );

        while ( pDest )
        {
            string dstAddress = pDest->Attribute( "ip_address" );
            int port = strToInt32 ( pDest->Attribute ( "port" ) );

            if ( port > 0 && port < 65536 && !dstAddress.empty() )
                addDestination(0, "live_encoding", dstAddress.c_str(), port, 0, 0, 0);

            pDest = pDest->NextSiblingElement ( "dest" );
        }  

        context->setMuxer(tsMuxer);
    }
	
    return (int) context;
}

long LIB_STD_CALL startEncodingServer2(const char* groupName, const char* srcAddr, const char* ipAddr, const char* rtpInfo)
{
	if (vodApplication.stopped) return 0;
	AACCodec& rawAAC = TSDemuxer::getAACRawCodec();
	rawAAC.m_channels = 2;
	rawAAC.m_sample_rate = 32000;
	rawAAC.m_id = 1; // MPEG2
	rawAAC.m_profile = 0; // AAC_LC
	rawAAC.m_layer = 0;
	rawAAC.m_rdb = 0;

	Process::sleep(5);
	long handle = createStreamingGroup(srcAddr, groupName,
		                                   0, 1, StreamingContext::llTransportRTP, 0, 0, 0, false);
	Process::sleep(5);
	addDestination(0, groupName, ipAddr , 0, rtpInfo, 0, 0);
	return handle;
}

unsigned LIB_STD_CALL startEncodingServer3(const string transport,
										   const string groupName, 
										   const string& toolsRoot, 
										   const string& batName, 
										   const string& videoPipeName,
										   const string& audioPipeName,
										   const string& syncSockAddr,
										   const char* ipAddr, const char* rtpInfo,
										   int vbvBufferSize)
{
	if (vodApplication.stopped) return 0;
	execProcessAsync(closeSlash(toolsRoot) + batName, "");
	TSMuxer* tsMuxer = new TSMuxer(*vodApplication.tsAlignedReadManager);
	tsMuxer->setAsyncMode(false);
	tsMuxer->setNewStyleAudioPES(false);
	map<string, string> audioParams;
	map<string, string> h264Params;
	tsMuxer->addStream("V_MPEG4/ISO/AVC", videoPipeName, h264Params);
	LTRACE(LT_INFO, 0, "Added H.264 video stream from source " << videoPipeName);
	if (audioPipeName != "none") {
		tsMuxer->addStream("A_AAC", audioPipeName, audioParams);
		LTRACE(LT_INFO, 0, "Added AAC audio stream from source " << audioPipeName);
	}
	else
		LTRACE(LT_INFO, 0, "Skip audio stream");
	bool isExtSync = false;
	if (syncSockAddr.size() != 0 && syncSockAddr != "0") 
	{
		LTRACE(LT_INFO, 0, "Set external sync info to udp://" << syncSockAddr);
		tsMuxer->setExternalSync(strToInt32(syncSockAddr.c_str()));
		isExtSync = true;
	}
	int llTransport = StreamingContext::llTransportRTP;
	if (transport == "RAW")
		llTransport = StreamingContext::llTransportRAW;
	tsMuxer->doMux(string("memory://prog_") + groupName, true);
	StreamingContext* context = vodApplication.streamer->createStreamingGroup( *vodApplication.tsAlignedReadManager, (string("memory://prog_") + groupName).c_str(),
		                                   ("prog_"+groupName).c_str(),
		                                   0, 1, llTransport, 0, 0, 0);
	if (vbvBufferSize > 0)
		((StreamingContextRTP*)context)->setVBVMode(vbvBufferSize, 3 );
	if (isExtSync)
		context->setCorrectTimeOnWait(false);
	if (llTransport == StreamingContext::llTransportRTP)
		addDestination(0, ("prog_"+groupName).c_str(), ipAddr , 0, rtpInfo, 0, 0);
	else {
		int port = strToInt32(rtpInfo);
		addDestination(0, ("prog_"+groupName).c_str(), ipAddr , port, 0, 0, 0);
	}
	context->setMuxer(tsMuxer);
	return (int) context;
}

unsigned LIB_STD_CALL startEncodingServer3Test(const string groupName, 
										   const string& toolsRoot, 
										   const string& batName, 
										   const string& videoPipeName,
										   const string& audioPipeName,
   										   const string& syncSockAddr,
										   const char* ipAddr, const char* rtpInfo)
{
	if (vodApplication.stopped) return 0;
	BOOL rez = SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	if ( rez == FALSE )  
		LTRACE(LT_WARN, 0, "Can't change process priority.");
	execProcessAsync(closeSlash(toolsRoot) + batName, "");
	//Process::sleep(3000);
	TSMuxer* tsMuxer = new TSMuxer(*vodApplication.tsAlignedReadManager);
	tsMuxer->setAsyncMode(false);
	tsMuxer->setNewStyleAudioPES(false);
	map<string, string> audioParams;
	map<string, string> h264Params;
	tsMuxer->addStream("V_MPEG4/ISO/AVC", videoPipeName, h264Params);
	LTRACE(LT_INFO, 0, "Added H.264 video stream from source " << videoPipeName);
	int32_t initDelay = getInitialDelay(toolsRoot, groupName);
	audioParams["timeshift"] = int32ToStr(initDelay) + string("ms");
	if (audioPipeName != "none") {
		tsMuxer->addStream("A_AAC", audioPipeName, audioParams);
		LTRACE(LT_INFO, 0, "Added AAC audio stream from source " << audioPipeName);
		if (initDelay != 0)
			LTRACE(LT_INFO, 0, "stream delay: " << initDelay << "ms");
	}
	else
		LTRACE(LT_INFO, 0, "Skip audio stream");

	if (syncSockAddr.size() != 0) {
		LTRACE(LT_INFO, 0, "Set external sync info to udp://" << syncSockAddr);
		tsMuxer->setExternalSync(strToInt32(syncSockAddr.c_str()));
	}

	tsMuxer->doMux(string("d:/prog_") + groupName + ".ts");
	return 0;
}

#endif

LIB_EXPORT int LIB_STD_CALL startStreamingDVD(const char* groupName, int llTransport)
{
	if (vodApplication.stopped) return 0;
	string dvdDrive = DVDHelper::getFirstDVDDrive();
	return startStreamingDVDExt(groupName, llTransport, dvdDrive.c_str(), "");
}

LIB_EXPORT int LIB_STD_CALL startStreamingDVDExt(const char* groupName, int llTransport, 
												 const char* drive,
												 const char* folder)
{
	if (vodApplication.stopped) return 0;
	TSMuxer* tsMuxer = 0;
	string sDrive = drive;
	if (!strEndWith(sDrive,"\\") && !strEndWith(sDrive,"/"))
		sDrive += "\\";
	string sFolder = folder;
	if (sFolder.size() > 0)
		if (!strEndWith(sFolder,"\\") && !strEndWith(sFolder,"/"))
			sFolder += "\\";
	try {
		LTRACE(LT_DEBUG, 0, "startStreamingDVD called");
		tsMuxer = new TSMuxer(*vodApplication.tsAlignedReadManager);
		tsMuxer->setAsyncMode(false);
		tsMuxer->setNewStyleAudioPES(false);
		vector<string> vobList = DVDHelper::getFilmVobList(sDrive.c_str(), sFolder.c_str());
		if (vobList.size() == 0) {
			LTRACE(LT_ERROR, 0, "Can't find VOB files on DVD disk.");
			return 0;
		}
		 string sourceFile;
		LTRACE(LT_DEBUG, 0, "after get VOB list");
		for ( size_t i = 0; i < vobList.size(); i++) {
			if (i > 0)
				sourceFile += "+";
			sourceFile += quoteStr(vobList[i]);
		}
		LTRACE(LT_DEBUG, 0, "detect stream reader");
		pair<AVChapters, vector<CheckStreamRez> > fullRez = METADemuxer::DetectStreamReader(*vodApplication.tsAlignedReadManager, vobList[0]);
		vector<CheckStreamRez>& tracks = fullRez.second;
		LTRACE(LT_DEBUG, 0, "add tracks");
		tsMuxer->setLoopMode(true);
		for ( size_t i = 0; i < tracks.size(); i++) {
			if (tracks[i].streamDescr.size() > 0) { // tracks recognized
				map<string, string> param;
				param["track"] = int32ToStr(tracks[i].trackID);
				tsMuxer->addStream(tracks[i].codecInfo.programName, sourceFile, param);
			}
		}
		LTRACE(LT_DEBUG, 0, "doMux async to memory");
		tsMuxer->doMux("memory://sm_DVD", true);
		LTRACE(LT_DEBUG, 0, "create streaming group");
		//unsigned handle = createStreamingGroup("memory://sm_DVD", groupName, 0, 1, llTransport, 0, 0, 0);
		//StreamingContext* context = (StreamingContext*) handle;
		StreamingContext* context = vodApplication.streamer->createStreamingGroup( *vodApplication.tsAlignedReadManager, "memory://sm_DVD", groupName, 0, 1, llTransport, 0, 0, 0);
		context->setMuxer(tsMuxer);
		LTRACE(LT_DEBUG, 0, "startStreamingDVD finished");
		//return handle;
		return context ? 1 : 0;
	} catch(VodCoreException& e) 
	{
		LTRACE(LT_ERROR, 0, "Can't start DVD streaming: " << e.m_errStr);
		return 0;
    } catch ( exception& e) 
	{
		LTRACE(LT_ERROR, 0, e.what());
		return 0;
	}
	catch (...) {
		LTRACE(LT_ERROR, 0, "Unknown exception at startStreamingDVD");
		return 0;
	}
}


LIB_EXPORT bool LIB_STD_CALL startAutoIndexing(const string& dir, bool createRewind)
{
	if (vodApplication.stopped) return false;
	if (vodApplication.autoIndexer == 0) {
		vodApplication.autoIndexer = new AutoIndexer(dir, createRewind);
	}
	return true;
}

LIB_EXPORT bool LIB_STD_CALL stopAutoIndexing()
{
	if (vodApplication.stopped) return false;
	if (vodApplication.autoIndexer) {
		delete vodApplication.autoIndexer;
		vodApplication.autoIndexer = 0;
	}
	return true;
}

//#define __ENCODE_SERVER_MODE

LIB_EXPORT void LIB_STD_CALL enableMaxSpeedMode()
{
    vodCoreContext->enableMaxSpeedMode();
}

LIB_EXPORT AbstractReader* LIB_STD_CALL getReader(const char* streamName)
{
	AbstractReader* ar = NULL;
	if ( !vodApplication.stopped )
	{

		BufferedReaderManager* brm = ( strStartWith ( streamName, "youtube://" ) || 
			strStartWith ( streamName, "rutube://" ) ||
			strStartWith ( streamName, "bbc://" ) 
			) ? vodApplication.ytReadManager : vodApplication.tsAlignedReadManager;

		ar = brm->getReader ( streamName );
	}

    return ar;
}

LIB_EXPORT void LIB_STD_CALL loadSmartBridge ( const char* configDir )
{
    try
    {
        LTRACE ( LT_DEBUG, 0, "vodTransport::loadSmartBridge called. from: " << configDir )
        
        if ( vodApplication.stopped )
            THROW ( ERR_COMMON, "vodApplication stopped. Line: " << __LINE__ )
        
        PluginContainer::loadBridge ( configDir );    
    
    }
    catch ( VodCoreException& e )
    {
        LTRACE ( LT_ERROR, e.m_errCode, "vodTransport::loadSmartBridge error: " << e.m_errStr )
    }
    catch ( ... )
    {
        LTRACE ( LT_ERROR, e.m_errCode, "vodTransport::loadSmartBridge error: unknown. Line: " << __LINE__ )
    }
}

LIB_EXPORT int LIB_STD_CALL getChecksumForAsset ( const char* fileName, char* buffer, int& size )
{    
    LTRACE ( LT_DEBUG, 0, "vodTransport::getChecksumForAsset called. Asset: " << fileName )
    
    int res = 0;    
    int32_t dataSize = 4096;
    uint8_t* data = new uint8_t [ dataSize ];
    
    try
    {
        if ( vodApplication.stopped )
        {
            res = -1;
            THROW ( ERR_COMMON, "vodApplication stopped. Line: " << __LINE__ )
        }
        else if ( buffer )
        {
            *buffer = 0;
            
            if ( fileExists ( fileName ) )
            {
                File file;    
                
                if ( file.open ( fileName, File::ofRead ) )
                {
                    int32_t count = 0;
                    uint32_t CS = 0;
                    uint64_t fileSize;
                    
                    file.size ( &fileSize );
                    
                    if ( fileSize == 0 )
                    {
                        res = -2; 
                        THROW ( ERR_COMMON, "file '" << fileName << "' is empty. Line: " << __LINE__ )
                    }
                    
                    while ( ( count = file.read ( data, dataSize ) ) > 0 )
                        CS = calculateCRC32 ( data, count, CS );
                    
                    file.close();
                    
                    if ( count == -1 )
                    {
                        res = -3; 
                        THROW ( ERR_COMMON, "couldn't read file '" << fileName << "'. Line: " << __LINE__ )
                    }
                    
                    string checkSum = int32ToStr ( CS, 16 );
                    
                    int checkSumSize  = checkSum.size();
                    
                    if ( checkSumSize == 0 )
                    {
                        res = -4;
                        THROW ( ERR_COMMON, "can't represent checksum like a hex string. Line: " << __LINE__ )
                    }
                    else if ( checkSumSize > 0 && checkSumSize <= size )
                    {
                        size = checkSumSize;
                        memcpy ( buffer, checkSum.c_str(), size );        
                    }
                    else
                    {
                        res = -5;
                        THROW ( ERR_COMMON, "not enough out buffer. Line: " << __LINE__ )
                    }
                }
                else
                {
                    res = -6; 
                    THROW ( ERR_COMMON, "couldn't open file '" << fileName << "'. Line: " << __LINE__ )
                }
            }
            else
            {
                res = -7;
                THROW ( ERR_COMMON, "file '" << fileName << "' not found. Line: " << __LINE__ )
            }
        }
    }
    catch ( VodCoreException& e )
    {
        LTRACE ( LT_ERROR, e.m_errCode, "vodTransport::getChecksumForAsset error: " << e.m_errStr )
    }
    catch ( ... )
    {
        res = -8;
        LTRACE ( LT_ERROR, e.m_errCode, "vodTransport::getChecksumForAsset error: unknown. Line: " << __LINE__ )
    }

    delete [] data;
    return res;
}

uint8_t assumeStat ( TrafficMonitor* tm, int period, double& avgBitrate, double& maxBitrate )
{
    int res = 0;
    
    if ( tm )
    {
        double mbr = 0.0;
        double abr = 0.0;

        tm->getStatisticForPeriod ( period, abr, mbr );

        if ( abr > 0 )
        {
            avgBitrate += abr;
            res = 1;
        }
        
        maxBitrate = mbr > maxBitrate ? mbr : maxBitrate;
    }

    return res;
}

LIB_EXPORT int LIB_STD_CALL getStatisticForPeriod ( int period, 
                                                    double& avgBroadcastBitrate,
                                                    double& maxBroadcastBitrate,
                                                    double& avgRunoffBitrate,
                                                    double& maxRunoffBitrate,
                                                    char* interval,
                                                    int intervalSize 
                                                  )
{
	size_t size = 0;

	LTRACE ( LT_DEBUG, 0, "vodTransport::getStatisticForPeriod called." )

    string err = "vodTransport::getStatisticForPeriod error: ";
    
	if ( vodApplication.stopped )
	{
		size = 0;
		LTRACE ( LT_ERROR, ERR_COMMON, err + "vodApplication stopped." )
	}
	else if ( interval )
	{
		uint8_t nB = 0;
		uint8_t nR = 0;
		
		MCVodStreamer* s = vodApplication.streamer;

		avgBroadcastBitrate = avgRunoffBitrate = maxRunoffBitrate = maxBroadcastBitrate = 0.0;

		*interval = 0;

		bool isValidPeriod = period == 60 || period == 600 ||  period == 1800 || period == 3600 || period == 86400;

        if ( isValidPeriod )
		{
		    for ( int i  = 0; i < STREAMERS_COUNT; ++i )
		    {
                nB += assumeStat ( s->getTrafficMonitor ( StreamingContext::smBroadcast, i ), period, avgBroadcastBitrate, maxBroadcastBitrate );
                nR += assumeStat ( s->getTrafficMonitor ( StreamingContext::smRunoff, i ), period, avgRunoffBitrate, maxRunoffBitrate  );
		    }
		
            avgBroadcastBitrate = nB > 0 ? avgBroadcastBitrate / nB : 0.0;
            avgRunoffBitrate = nR > 0 ? avgRunoffBitrate / nR : 0.0;
        }
        
		if ( isValidPeriod )
		{
		    if ( nB == 0 )
		        LTRACE ( LT_WARN, 0, err + "no broadcast traffic info." )
		        
            if ( nR == 0 )
                LTRACE ( LT_WARN, 0, err + "no runoff traffic info." )
                
			int32_t currSecond = time().toInt();
			int32_t intEnd = currSecond - ( currSecond % period );

			TimeInterval ti ( Time ( intEnd - period ), Time ( intEnd ) );

			string str_interval = ti.toString();
			size = str_interval.size();

			if ( size < (size_t)intervalSize )
			{
				memcpy ( interval, str_interval.c_str(), size );
				interval [ size ] = 0;
			}
			else 
			{
				size = -1;
				LTRACE ( LT_ERROR, ERR_COMMON, err + "not enough out buffer for interval." )
			}
		}
		else
		{
			size = -2;
			LTRACE ( LT_ERROR, ERR_COMMON, err + "invalid period." )
		}
	}

	return size;
}

}

void testContinuityCounter ()
{
    string sFileName = "test50000.ts";
    string sMes;

    uint8_t nSize = 188;
    uint8_t* buffer = new uint8_t [ nSize ];

    File log_file;
    log_file.open ( "counter.log", File::ofWrite );

    StreamedFile ts_file;
    ts_file.open ( sFileName.c_str(), File::ofRead );

    map < int16_t, uint8_t > counters;
    while ( ts_file.read ( buffer, nSize ) != -1 )
    {
        TSPacket* tsPacket = ( TSPacket* )buffer;
        int16_t pid = tsPacket->getPID();
        int8_t  cnt = tsPacket->counter;

        if ( pid == 4113  || pid == 4352 )
        {
            if ( cnt != 0 && counters [ pid ] - cnt != 0 )
            counters [ pid ] = cnt;
        }

        sMes = "PID: " + int16ToStr ( pid ) + "\n Counter: " + int8ToStr ( cnt ) + "\n\n";

        log_file.write ( sMes.c_str(), sMes.size() );
    }

    ts_file.close ();
    log_file.close ();
};

void getEncodedServerDestinations ( /*EncodedServerDestinations& esDests*/ )
{

}

void trimFile(const string& src, const string& dst, uint32_t size)
{
	uint8_t* tmp = new uint8_t[size];
	File srcFile;
	srcFile.open(src.c_str(), File::ofRead);
	srcFile.read(tmp, size);
	File dstFile;
	dstFile.open(dst.c_str(), File::ofWrite);
	dstFile.write(tmp, size);
}

bool convertCacheInfoFile ( const string& fileName )
{
    bool res = false;
    File file, newFile;
    char* id = new char [ 11 ];    
    
    try
    {
        if ( !fileExists ( fileName ) )
            THROW ( ERR_COMMON, "cache info file '" << fileName << "' not faound. Line: " << __LINE__  )
        
        const int ts = 24;
        
        struct RatioData
        {
            bool       stored;
            uint32_t   duration;
            double ratio [ ts ];
        };
        
        RatioData rd;
        
        if ( !file.open ( fileName.c_str(), File::ofRead ) ) 
            THROW ( ERR_COMMON, "can't open file '" << fileName << "'. Line: " << __LINE__ )

        if ( !newFile.open ( "new_cache.inf", File::ofWrite ) ) 
            THROW ( ERR_COMMON, "can't open file dump file. Line: " << __LINE__ )            

        uint32_t count ( 24 );
        uint32_t data_size = sizeof ( RatioData );
        
        newFile.write ( &count, sizeof ( count ) );
        
        count = 0;
        file.read ( &count, sizeof ( count ) );
        
        for ( uint32_t i ( 0 ); i < count; ++i )
        {
            size_t   timeSpaces ( 0 );
            
            file.read ( id, 11 );
            
            file.read ( &rd.duration,   sizeof ( uint32_t ) );
            file.read ( &rd.stored,     sizeof ( bool ) );
            file.read ( &timeSpaces, sizeof ( timeSpaces ) );
            
            for ( size_t j ( 0 ); j < timeSpaces; ++j )
            {
                file.read ( &( rd.ratio [ j ] ), sizeof ( double ) );
                //rd.ratio [ j ];
            }

            newFile.write ( id, 11 );
            newFile.write ( &rd, data_size );
        }
    }
    catch ( VodCoreException& e )
    {
        LTRACE ( LT_WARN, e.m_errCode, "convertCacheInfoFile error: " << e.m_errStr )
    }
    catch ( ... )
    {
        LTRACE ( LT_FATAL_ERROR, __LINE__, "convertCacheInfoFile error: unknown. Line: " << __LINE__ )
    }
    
    delete [] id;
    
    file.close();
    newFile.close();
    
    return res;
}

#ifdef WIN32
    #include "mmsystem.h"
#endif

void testYT()
{
    //convertCacheInfoFile ( "cache.inf" );
    
    vector < string > films;
    
    films.push_back ( "IdmwHljfN4Q" );
	films.push_back ( "1bev1RHrt2o" );
	films.push_back ( "Dn_yvRqPWhA" );  
	films.push_back ( "IZDTkxWudXo" );
	films.push_back ( "xdhLQCYQ-nQ" );
	films.push_back ( "VCJsyJdf8WA" );
	films.push_back ( "tl_fzIxnRqI" );
	films.push_back ( "aZpD0btOZx8" );
    films.push_back ( "ajwnmkEqYpo" );
    films.push_back ( "9ghMgTkVpJI" );
    //films.push_back ( "OlWLL6NAN98" );
    //films.push_back ( "NKONZT1uV0Q" );
    //films.push_back ( "SvqR_jbeil4" );
    //films.push_back ( "OLRs2CSu2UA" );
    //films.push_back ( "Hiieqlgbn2c" );
    //films.push_back ( "mO-WF1aKFY4" );		
    
	uint32_t nCount = films.size();
    uint32_t nPort ( 50000 );
    
    cout << "Films count: " << nCount << endl;
    cout << "--------------------------------------------------------------------------------" << endl;

    vector < uint32_t > handles;
		
    for ( uint32_t i ( 0 ); i < nCount; ++i )
    {
        cout << i+1  << ". " << films [ i ] << endl;
        //handles.push_back ( testRuTubeStreaming ( nPort++, films [ i ] ) );
        handles.push_back ( testYouTubeStreaming ( nPort++, films [ i ] ) );
        
        cout << "--------------------------------------------------------------------------------" << endl;
    }
    
   //Process::sleep ( 15000 );
   //stopStreaming ( handles [ 0 ] );
   //setPosition ( handles[0], 60 );
   /*Process::sleep ( 10000 );
   setPosition ( handles[0], 60 );
   Process::sleep ( 10000 );
   setPosition ( handles[0], 120 );*/
}

void testTCP()
{
    //vodCoreContext->enableBlockSendingBySinglePackets();
    
    int err = 0;
    SocketAddress adr ( "127.0.0.1",5000 );
    
    TCPSocket* sck = new TCPSocket();
    //sck->setUnblockedMode(true);
    sck->setReuseAddr(true);
    sck->bindToLocalPort(5000);
    sck->setReadTimeout(1000);    
    sck->connect( adr );
    
    
    /*TCPSocket* sck2 = new TCPSocket();
    int h2 = sck2->getHandle();
    sck2->setUnblockedMode(true);
    sck2->setReuseAddr(true);
    sck2->setReadTimeout(1000);
    sck2->connect( adr );
    //sck->bindToLocalPort(5000);
    */
    
                  
    
    
//    TCPSocket* new_sck = sck->accept();
    
  //  if (new_sck)
    //    new_sck->setReadTimeout(1000);
    
    startStreaming ( 0, StreamingContext::llTransportRAW, 1024*1024, 1, "udp://239.255.4.97:5500", "127.0.0.1", 50000, 0, (long)sck, false );
    //startStreaming ( 0, StreamingContext::llTransportRAW, 1024*1024, 1, "C:/rtspServer/Media/valli.ts", "127.0.0.1", 50000, 0, 0, false );
    
    cout << "Press enter to terminate..." << endl;
    
    int len = 1024*10;
    uint8_t* buff = new uint8_t[len];
    File fl;
    
    fl.open("222.ts", File::ofWrite);
    
    TCPSocket* acc = NULL;
    
    while ( true )    
    {
        int res = sck->readBuffer ( buff, len );
        
        if ( res <= 0 )
            continue;
        
        if (res > len )
            res = len;
            
        fl.write ( buff, res );
        
        /*if ( buff [ 0 ] == '$' )
        {
            uint16_t size = my_ntohs ( *( ( ( uint16_t* )buff ) + 1 ) );
            
            while ( size > 0 )
            {
                res = sck->readBuffer ( buff, size );
                
                if ( res > 0 )
                {
                    if ( res > size )
                        res = size;
                        
                    size -= res;
                    
                    fl.write ( buff, res );
                }
            }
        }*/
        
        /*if ( !acc )
        {
            acc = sck->accept();
            
            if ( acc )
                acc->setReadTimeout(1000);
        }
        
        if ( acc )
        {
            int res = 1;
            
            while ( res > 0 )
            {
                res = acc->readBuffer ( buff, len );
            
                if (res > len )
                    res = len;
                    
                if ( res > 0 )    
                    fl.write ( buff, res );
            }
        }*/
        
        Process::sleep(500);
    }                                          
    
    
    
    fl.close();
    
    delete [] buff;
    delete sck;
    //delete sck2;
    
    getchar();
        
    return;
}

void testYTLoad()
{
    string id; 
    vector < string > idList;

    initLog(6, "ytTester.log", "./", 60 );

    TextFile idListFile ( "ytTesterIDList.txt", File::ofRead );

    idListFile.readLine ( id );

    while( id.length() > 0 ) 
    {
        id = trimStr ( id );

        if ( id.length() == 0 || id [ 0 ] == '#' )
        {
            idListFile.readLine( id );
            continue;
        }

        idList.push_back ( id );

        idListFile.readLine ( id );
    }

    uint32_t idListSize = idList.size();
	
	disableYTCache();
	
	bool nextLoop = true;
	
	while ( nextLoop )
	{
		if ( idListSize > 0 )
		{
			cout << idListSize << " video id(s) was read" << endl;
			cout << "--------------------------------------------------------------------------------" << endl;

			uint32_t concurrentlyStreamingFilms = min ( 1u, idListSize );

			/*const char* pCount = argv [ 1 ];

			if ( pCount )
				concurrentlyStreamingFilms = strToInt32u ( pCount );*/

			srand ( ( unsigned )time( NULL ) );
	        

			int RANGE_MIN = 0;
			typedef map < int32_t, pair < uint32_t, uint32_t > > StreamingMap;

			for ( int j = 0; j < 1; ++j  )
			{
				int RANGE_MAX = idListSize - 1;
	            
				StreamingMap usingIndexes;
				uint32_t port = 50000;
	            
				for ( int i = 0; i < concurrentlyStreamingFilms; ++i )
				{
					int32_t randomIndex;
	                
					do
					{
					   randomIndex = ( ( ( double ) rand() / ( double ) RAND_MAX ) * RANGE_MAX + RANGE_MIN );

					} while ( usingIndexes.count ( randomIndex ) != 0 );


                    randomIndex = i;
					string rid = idList [ randomIndex ];

					cout << i + 1 << ". " << rid << " on port " << port << endl;

					LTRACE ( LT_DEBUG, 0, "DBG ytTester action start for '" << "youtube://" + rid + "&sid=" + int32uToStr ( port ) << "'" )
					LTRACE ( LT_DEBUG, 0, "DBG ytTester action start: before start" )

					uint32_t handle = testYouTubeStreaming ( port, rid );
					//uint32_t handle = startStreaming ( 0, StreamingContext::llTransportRAW, 0 , 1,  "C:\\rtspServer\\Media\\300.ts", "127.0.0.1", 24000, 0, 0 );

					LTRACE ( LT_DEBUG, 0, "DBG ytTester action start: after start" )

					rid = "started";

					if ( handle > 0 )
						usingIndexes [ randomIndex ] = make_pair ( handle, port++ );
					else
						rid = "failed";

					cout << "Statues: " << rid << endl;
					cout << "--------------------------------------------------------------------------------" << endl;

					Process::sleep ( 500 );
				}

				RANGE_MAX = 100;

				Process::sleep ( 5000 );
				
				for ( StreamingMap::iterator i = usingIndexes.begin(); i != usingIndexes.end(); ++i )
				{
					int32_t randomAction = ( ( ( double ) rand() / ( double ) RAND_MAX ) * RANGE_MAX + RANGE_MIN );

					int32_t handle = i->second.first;
					string streamName = "youtube://" + idList [ i->first ] + "&sid=" + int32uToStr ( i->second.second );
					
					/*if ( randomAction < 50 )
					{
						uint64_t duration = 40 * 1000000; //vodCoreContext->getYTStreamDuration ( streamName );

						RANGE_MAX = floor ( ( double )duration / 1000000.0 );

						uint32_t randomPos = ( ( ( double ) rand() / ( double ) RAND_MAX ) * RANGE_MAX + RANGE_MIN );

						randomPos = 120;
	                    
						LTRACE ( LT_DEBUG, 0, "DBG ytTester action set-pos for '" << streamName << "', pos = " << randomPos << ", handle = " << handle )
						LTRACE ( LT_DEBUG, 0, "DBG ytTester action set-pos: before setPos" )
						setPosition ( handle, randomPos );
						LTRACE ( LT_DEBUG, 0, "DBG ytTester action set-pos: after setPos" )

						Process::sleep ( 5000 );
					}
					
					Process::sleep ( 10000 );
					*/

					LTRACE ( LT_DEBUG, 0, "DBG ytTester action stop for '" << streamName << "', handle: " << handle )
	                
					stopStreaming ( handle );

					LTRACE ( LT_DEBUG, 0, "DBG ytTester action stop-pos: after stop" )

					//Process::sleep ( 5000 );
				}
			}
		}
		else
		{
			cout << "WARN: No any valid video id." << endl;        
			cout << "--------------------------------------------------------------------------------" << endl;
		}

		nextLoop = false;
	}
}

void startEncodingServer()
{
    char buff[1024];

    initLog(6, "vodTransport.log", "./", 3600 );

    LTRACE ( LT_DEBUG, 0, "Encoding server started." )

    if ( fileExists ( "encodingServerCfg.xml" ) )
    {
        startEncodingServer ();

        while ( true )
            Process::sleep(50);
    }
    else
    {
        cout << "SmartLabs encoding server need config file encodeServerCfg.xml!" << endl;
        LTRACE ( LT_DEBUG, 0, "No config. Exit." )
    }

    /*multicastJoin ( "C:\\1.ts", "239.255.0.100", 5500, true, 0, 0, true );
    while(true);*/
}

void testStreamingGroup()
{
    /*
    string codec = "mpeg2"; 
        
        setTsMtuSize(1316);
        setMaxUIpPacketCount(1);
        setMaxMIpPacketCount(1);*/
        //string codec = "h264su";
    
    createStreamingGroup ( "C:\\rtspServer\\Media\\300_10sec.ts", "demuxerTest", 0, 1, StreamingContext::llTransportRAW, 0,
            0, 0, false );
    
    addDestination( 0, "demuxerTest", "239.255.11.113", 5000, 0, 0, 0 );
    
    
    addDestination( 0, "demuxerTest", "127.0.0.1", 1234, 0, 0, 0 );

    //createStreamingGroup ( "udp://239.255.0.100:5500", "demuxerTest", 0, 1, StreamingContext::llTransportRAW, 0,
    //createStreamingGroup ( "C:\\rtspServer\\Media\\300.ts", "demuxerTest", 0, 1, StreamingContext::llTransportRAW, 0,
    //0, 0, false, false, 128, 96, 256, 25, 27, false, 0, true );

    //addDestination( 0, "demuxerTest", "239.255.11.111", 5000, 0, 0, 0 );

}

void testIndexer()
{
    //int rez = createIndexFile ( "C:/rtspServer/Media/300.ts" , true);
    //int rez = createIndexFile ( "C:/rtspServer/Media/NPVR_RECORD_5200455.ts" , true);
    int rez = createIndexFile ( "C:/rtspServer/Media/NPVR_RECORD_5199513.ts" , true);
    
        
    /*string src = "C:\\dump\\CTI_sample_h264_4.5M.ts";
    string dst = "C:\\dump\\testCopy.ts";
    uint64_t fileSize = 0;
    
    File file;
    file.open ( src.c_str(), File::ofRead );
    file.size ( &fileSize );
    file.close();
    
    uint64_t rmin = 1;
    uint64_t rmax = fileSize;
    srand ( ( unsigned )time( NULL ) );
    
    for (int i = 0; i < 1000; ++i) 
    {
    
        fileSize = (int)( ( ( double ) rand() / ( double ) RAND_MAX ) * rmax + rmin );
        
        copyFile ( src, dst, true );
        
        File cf;
        cf.open ( dst.c_str(), File::ofWrite );
        cf.truncate ( fileSize );
        cf.close();
        
        if ( AssetIndexManager::getInstance()->deleteFileIndex ( dst.c_str() ) )
            int rez = createIndexFile ( dst.c_str(), true );
        
    }*/	  

}

void testStat()
{
    /*double avgBitrate, maxBitrate; 
    char* interval = new char [265];
    
    int s = getStatisticForPeriod ( 1800, avgBitrate, maxBitrate, interval, 256 );
    */
}

void testMulticastJoin()
{
    //multicastJoin( "C:/rtspServer/Media/cyclic_records/pause_live/pause_live.ts", "239.255.0.92", 5500, 1, 300, 0, true , 0, 0 );
    //multicastJoin( "C:/rtspServer/Media/pause_live.ts", "239.255.0.101", 5500, 1, 0, 0, true , 0, 0 );
    //multicastJoin( "C:/rtspServer/ffmpeg.ts", "229.255.0.101", 5700, 1, 0, 0, false , 0, 0 );
    MulticastRetransmitter* mr = MulticastRetransmitter::create ( "239.255.2.100:5500", NULL, "239.255.2.105:5700", "10.65.10.81:11500", true );
    //tsRetransmit ( "239.255.0.92", 5500,0, "229.255.1.92", 5700, true );
    //tsRetransmit ( "239.255.0.92", 5500,0, "127.0.0.1", 50000, false );
    
    return;
    //Process::sleep ( 10000 );
    
    //char dstBuffer [ 1024*2 ];
    //getAssetDescription ( 0, "C:/rtspServer/Media/pause_live.ts", 0, dstBuffer, sizeof ( dstBuffer ) );
    
    /*
    Process::sleep ( 1000 );
    
    pauseStreaming ( readerID );
    
    Process::sleep ( 2000 );
    
    resumeStreaming ( readerID );
    
    Process::sleep ( 2000 ); */
    //int scale = 8;
    
    //int readerID = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, "C:/rtspServer/Media/pause_live.ts", "127.0.0.1", 50000 , 0, 0 );
    
    int readerID = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, "C:/rtspServer/ffmpeg.ts", "127.0.0.1", 50000 , 0, 0 );
    
    //Process::sleep ( 3000 );
    
    /*for ( int i = 0; i < 50; ++i )
    {
        
        cout << i << endl;
        double nptPos = setPosition ( readerID, 2 );
        pauseStreaming(readerID);
        Process::sleep ( 1000 );
        resumeStreaming ( readerID );
        unsigned int r = changeScale ( readerID, scale );
        scale = scale == 8 ? 1 : 8;
        /*Process::sleep ( 3000 );
        stopStreaming ( readerID );
    }*/
    
    //stopStreaming ( readerID );
    
        /*
    
    setPosition ( readerID , -3 );
    
    float nptPos = getNptPos ( readerID );
    
    Process::sleep ( 2000 );
    
    //changeScale ( readerID, 8 );
    setPosition ( readerID , -5 );
    
    //Process::sleep ( 5000 );
    
    //changeScale ( readerID, 1 );
    
    //Process::sleep ( 2000 );
    
    nptPos = getNptPos ( readerID );
        
    Process::sleep ( 5000 );

    stopStreaming ( readerID );    
    
    nptPos = getNptPos ( readerID );
    
    Process::sleep ( 5000 );
    
    nptPos = getNptPos ( readerID );
    
    
    //multicastLeave ( "test_rec1.ts" );  */ 
}

void test101()
{
    //unsigned handle = startStreaming ( "", StreamingContext::llTransportRAW, 0, 1, "radio://http://www.101.ru/play.m3u?uid=1&bit=2&serv=", "127.0.0.1", 50000, 0, 0 );
    
    createStreamingGroup ( "radio://uid=31&proxy=10.65.10.4&port=3128", "radioTest", 0, 1, StreamingContext::llTransportRAW, 0, 0, 0, false );

    addDestination( 0, "radioTest", "239.255.11.113", 5600, 0, 0, 0 );
}

void testRTSPServer ( int count )
{
    vector < string > list;
    vector < RTSPRCGThread* > t_list;
    
    list.push_back ( "300.ts" );
    list.push_back ( "valli.ts" );
    list.push_back ( "sample.ts" );
    list.push_back ( "invalid.ts" );
    list.push_back ( "t2_3mbps.ts" );

    //int count = ( int )list.size();
    
    int port = 40000;
    
    string name = "";
    
    for ( int i = 0; i < count; ++i )
    {
        name  = "300";
        
        if ( i > 0 )
            name += "_" + int32ToStr ( i );
        
        name += ".ts";
        
        RTSPRCGThread* t = new RTSPRCGThread ( "10.65.10.50:50000", "10.65.10.79:" + int32ToStr ( port++ ), name/*list [ i % count ]*/ );
        
        t_list.push_back ( t );
        
        Process::sleep ( 1000 );
    }
}

void testMulticastStreamer()
{
    
    //unsigned handle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, "udp://239.255.0.100:5500", "127.0.0.1", 50000, 0, 0 );
    //Process::sleep ( 5000 );
    //stopStreaming ( handle );
    //handle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, "udp://239.255.0.101:5500", "127.0.0.1", 50000, 0, 0 );
    
    //createStreamingGroup ( "igmp://239.255.0.101:5500/transcode?bitrate=900&keepsound=1&codec=h264ld", "mcstTest", 0, 1, StreamingContext::llTransportRAW, 0, 0, 0, false );

    //addDestination( 0, "mcstTest", "239.255.111.120", 5600, 0, 0, 0 );
    
    vector < string > list_src;
    vector < string > list_dst;

    list_src.push_back ( "239.255.0.101:5500" );
    /*list_src.push_back ( "239.255.0.102:5500" ); */
    //list_src.push_back ( "239.255.0.50:5500" );
    //list_src.push_back ( "239.255.0.53:5500" );
    //list_src.push_back ( "239.255.0.52:5500" );

    list_dst.push_back ( "229.255.0.101" );
    /*list_dst.push_back ( "229.255.0.102" );*/
    //list_dst.push_back ( "229.255.0.50" );
    //list_dst.push_back ( "229.255.0.53" );
    //list_dst.push_back ( "229.255.0.52" );

    int count = ( int )list_src.size();

    string name;
    
    for ( int i = 0; i < count; ++i )
    {
        name = "mcstTest" + int32ToStr ( i );
        
        createStreamingGroup ( ( "udp://" + list_src [ i ] + "/transcode?bitrate=500&keepsound=1&codec=h264ld&width=480&height=320").c_str(), name.c_str() , 0, 1, StreamingContext::llTransportRAW, 0, 0, 0, false );

        addDestination( 0, name.c_str(), list_dst [ i ].c_str(), 5500, 0, 0, 0 );    
    }
}

#include "stdlib.h"
void testInternetStreaming()
{
    int sHandle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, "udp://239.255.4.97:5500", "10.65.10.79", 50000, 0, 0);
  /*while (1) {
        LTRACE(LT_DEBUG, 0, "before start streaming");
        TCPSocket* tcpSocket = new TCPSocket();
        tcpSocket->connect(SocketAddress("10.65.100.7",8080));                        //           rtsp://195.34.30.227:50000/udp://239.255.4.97:5500
    int sHandle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, "udp://239.255.4.97:5500", "10.65.10.79", 35000, 0, (int)tcpSocket);
        LTRACE(LT_DEBUG, 0, "after start streaming");
    int r = rand() % 6000;
    Process::sleep(r);
        LTRACE(LT_DEBUG, 0, "before sttop streaming");
    stopStreaming(sHandle);
        LTRACE(LT_DEBUG, 0, "after stop streaming");
  }*/
}

void testStartStreaming()
{
    int h = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, "C:/rtspServer/Media/300.ts", "127.0.0.1", 50000, 0, 0);
    
    Process::sleep ( 10000 );
    
    int npt = getNptPos ( h );
    
    changeScale ( h, 8 );
    
    Process::sleep ( 10000 );
    
    npt = getNptPos ( h );
    
    changeScale ( h, 1 );
    
    Process::sleep ( 10000 );
    
    npt = getNptPos ( h );
    //int sHandle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 8, "C:/rtspServer/Media/cyclic_records/pause_live/pause_live.ts", "127.0.0.1", 50000, 0, 0);
    
    //int sHandle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 8, "C:/rtspServer/ffmpeg.ts", "127.0.0.1", 50000, 0, 0);
    //int sHandle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 8, "udp://127.0.0.1:55000", "127.0.0.1", 50000, 0, 0);
}
//-----------------------------------------------------------------------------------------------------------------------------------------

void testSmartBridge()
{
    loadSmartBridge ( "C:/rtspServer/" );
    
    string name = "udpt://239.255.0.92:5500";
    
    //int sHandle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, name.c_str(), "127.0.0.1", 50000, 0, 0 );
    createStreamingGroup ( name.c_str(), "test" , 0, 1, StreamingContext::llTransportRAW, 0, 0, 0, false );

    addDestination( 0, "test", "229.255.0.92", 5600, 0, 0, 0 );    
    
    //string name = "dvtube:/0faG7fwD8Wy2_13733.flv";
    //rtsp://10.65.10.81:50000/dvtube:/s58X0LD5UfsE_13875.flv
 /*   string name = "dvtube:/s58X0LD5UfsE_13875.flv";
    //string name = "dvtube:/MediaItems/18.mpg";
    //string name = "C:/rtspServer/Media/300.ts";
    
    int size = 4096;
    char* buffer = new char [ size ];
    
    getAssetDescription ( "127.0.0.1", name.c_str(), 0, buffer, size );
    
    //int sHandle = startStreaming ( 0, StreamingContext::llTransportRAW, 0, 1, name.c_str(), "127.0.0.1", 50000, 0, 0 );
    
    delete [] buffer;*/
    //Process::sleep ( 5000 );
    //stopStreaming ( sHandle );
}
//-----------------------------------------------------------------------------------------------------------------------------------------

uint32_t getUI24 ( uint8_t* pData ) 
{ 
    if ( pData + 3 == NULL) return 0;
    else return ( pData[2] + ( pData[1] << 8 ) + ( pData[0] << 16 ) ); 
}

void fileCut()
{
    File f;
    
    typedef unsigned char ui_24[3];
    
    int s = sizeof ( ui_24 );
    
    //if ( f.open ( "v_13875.flv", File::ofRead ) )
    if ( f.open ( "v_13733.flv", File::ofRead ) )
    {
        int buffSize = 9;//195892;
        
        uint8_t* buffer = new uint8_t [ buffSize ];
        
        int read = f.read ( buffer, buffSize );
        
        //int mds = getUI24 ( buffer + 14 );
        
        f.seek ( 1469000, File::smBegin );
        
        //if ( read == offset1 )
        //{
            File fo;

            if ( fo.open ( "test_out4.flv", File::ofWrite ) )
            {
                fo.write ( buffer, 5529 );
                
                while ( ( read = f.read ( buffer, buffSize ) ) > 0 )
                    fo.write ( buffer, buffSize );
                    
                fo.close();
            }
        //}
        
        f.close();
    }
}

void testPlayerCmdEmu ( int argc, char** argv )
{
    //PlayerCmdEmu pcu ( argv, argc );
    
    //pcu.processCmd();
}/*
 class Foo
 {
 public:
     Foo ( int j )
     {
        i = new int[j]; //потенциальная лажа. Решение: i = j > 0 ? new int [ j ] : NULL;
     }
     
     ~Foo()       // потенциальный memory leak. Решение: virtual ~Foo(){ ... }
     {
        delete i; // потенциальный memory leak. Решение: delete [] i; 
     }
 private:
    int* i;
 };

 class Bar: Foo
 {
 public:
     Bar(int j) 
     { 
        i = new char[j]; // потенциальная лажа. Решение: i = j > 0 ? new int [ j ] : NULL; ( k = j > 0 ? new int [ j ] : NULL; )
     }
     ~Bar()
     {
        delete i; // потенциальный memory leak. Решение: delete [] i; ( delete [] k; )
     }
 private:
    char* i;  // потенциальный memory leak. Решение: char* k;
 };


 void main()
 {
     Foo* f = new Foo ( 100 );
     Foo* b = new Bar ( 200 ); //Запрещенное преобразование из указателя на производный класс в указатель на базовый класс ( защита доступа private  ). Решение: class Bar: public Foo
     
     *f = *b; // потенциальный memory leak. Решение: явно определить Foo& Foo::operator= ( const Foo& s ){...}
     
     delete f;
     delete b; //лажа. так как из-за действий выше b->i может указывать на почиканую память. Решение: char* Bar::k + предыдущий комментарий.
 }   
 
class Foo
{
public:
    Foo ( int j ) 
    {
        i = new int[j]; // здесь потенциальная лажа. i = j > 0 ? new int [ j ] ? NULL;
    }
    virtual ~Foo()
    {
        delete i; //
    }
protected:
    int* i;
};

class Bar: public Foo // 
{
    typedef Foo base_class;
public:
    Bar ( int j ):       // Foo: нет подходящего конструктора по умолчанию
        base_class ( j ) 
    { 
        i = new char[j]; // реинит, утечка, требуется другое имя
    }
    ~Bar()
    {
        delete i; 
    }
protected:
    char* i;
};

class class1
{
public:
    void out()
    {
        cout << "class1" << endl;
    }
    
    void out2()
    {
        out();
    }
};

class class2: public class1
{
public:
    void out()
    {
        cout << "class2" << endl;
    }
};

void f ( class1* c )
{
    c->out();
}

/*void main()
{
/*    class2 c2;
    c2.out();
    c2.out2();
    f((class1*)&c2);
    
    /*int x = 5;
    int y = 6;
    
    x = x^y;
    y = y^x;
    x = x^y; 
    
    Foo* f=new Foo(100);
    Foo* b=new Bar(200);
    //*f=*b;
    delete f;
    А, Г, 4, 9
    delete b;
    
    string err;

    File pf;

    if ( !pf.open ( "\\\\.\\pipe\\m_test", File::ofCreateNew ) )
    {
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
        
        str = str;

    }
}*/

int main(int argc, char** argv)
{
	//testRuTubeStreaming(0,"");
    const char* count = argv[1];
    
    int i_count = count ? strToInt32 ( count ) : 160;
//#ifdef ENCODING_SERVER_MODE
//    startEncodingServer()     ;
//#elif defined ( YT_TESTER_MODE )
//    testYTLoad();
//#else
//
        initLog(6, "vodTransport.log", "./", 60 );

    //testStartStreaming();
    //testPlayerCmdEmu ( argc, argv );
    //fileCut();
    //testSmartBridge();
        //testMulticastJoin();

    //testRTSPServer ( i_count );
    //test101();
    //testMulticastStreamer();
    testMulticastJoin();
    
    //testStartStreaming();
    //testIndexer();
    //testInternetStreaming();
    //testTCP();

//#endif

    cout << "Press 'Enter' to terminate..." << endl;
    getchar();

    return 0;
}
