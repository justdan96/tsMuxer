#ifndef __ABSTRACT_CORE_CONTEXT_H
#define __ABSTRACT_CORE_CONTEXT_H

#include <string>
#include <map>
#include <types/types.h>

#include "abstractreader.h"
#include "abstractDemuxer.h"

#ifdef WIN32
#define LIB_EXPORT extern "C" __declspec(dllexport)
#define LIB_CLASS_EXPORT __declspec(dllexport)
#define LIB_STD_CALL __stdcall
#else
#define LIB_EXPORT extern "C"
#define LIB_CLASS_EXPORT
#define LIB_STD_CALL
#endif


namespace vodcore
{

//class AbstractDemuxer;

typedef void (LIB_STD_CALL *EVENT_HANDLER_PROC) (const long handle, const int eventID, const char* message);
struct MuxTrackInfo {
	std::string streamName;
	std::string codecName;
	std::map<std::string, std::string> addParam;
};

class AbstractCoreContext
{
public:
    typedef std::string string;
#ifndef __TS_MUXER_COMPILE_MODE
    typedef std::multimap < string, std::pair < uint8_t, double > > YTVideoInfoMap;
    typedef std::pair < SocketAddress, string > StreamInfoPair;
    typedef std::map < string, StreamInfoPair > StreamInfoMap;
#endif
    AbstractCoreContext():
        m_ytCacheDuration ( 0 ),
        m_rtCacheDuration ( 0 ),
        m_nYTVideoQuality ( 1 ),
        m_nYTScalingType ( 0 ),
        m_nRTScalingType ( 0 ),
        m_ytCacheDisabled ( false ),
        m_rtCacheDisabled ( false ),
        m_ytMp4TranscodingDisabled ( false ),
        m_tsMtuSize ( 1400 ),
        m_maxMIpPacketCount ( 6 ),
        m_maxUIpPacketCount ( 6 ),
        m_ytVideoWidth ( 352 ),
        m_ytVideoHeight ( 288 ),
        m_ytAutoPal ( false ),  

        m_rtVideoWidth ( 352 ),
        m_rtVideoHeight ( 288 ),
        m_rtAutoPal ( false ),  
        
        m_cacheDemoMode ( false ),
        m_maxSpeedMode ( false ),

        m_sendBlockBySinglePackets ( false ),
        m_trafficBandWidth ( 1000 ),
        m_ytLogoPos ( 0 ),
        m_ytLogoPath ( "ytLogo.bmp" )
    {}
    
    virtual ~AbstractCoreContext()
    {
    }

	virtual unsigned startStreaming(const char* session,
						        int streamingMode,
								double ntpPos, 
								int scale,
								const char* fileName, 
								const char* address, 
								unsigned short port,
								const char* rtpInfo,
								long tcpSocketHandle) = 0 ;

	virtual unsigned  startSmartBridgeStreaming ( const char* sessionID, double ntpPos, 
								                  const char* url, 
								                  const char* address, 
								                  unsigned short port, 
                                                  const char* rtpInfo, long tcpSocketHandle ) = 0 ;

	virtual unsigned  seekYouTubeStreaming ( const char* streamName, double ntpPos) = 0;
    virtual uint64_t  getYTStreamDuration ( const std::string& streamName ) = 0;

    virtual unsigned  seekRuTubeStreaming ( const char* streamName, double ntpPos) = 0;
    virtual uint64_t  getRTStreamDuration ( const std::string& streamName ) = 0;

    virtual void setYTCacheParams ( const std::string&  sCacheDirectory, const std::string&  sLogDirectory, uint64_t nTotalCacheDuration ) = 0;
    virtual void setRTCacheParams ( const std::string&  sCacheDirectory, uint64_t nTotalCacheDuration ) = 0;
    
    virtual void setYTVideoInfo ( const std::string&  streamName, uint8_t existingType, double fps ) = 0;
    virtual void getYTVideoInfo ( const std::string&  streamName, uint8_t& existingType, double& fps, bool cleanUp = false ) = 0;

    virtual void setRTVideoInfo ( const std::string&  streamName, uint8_t existingType, double fps ) = 0;
    virtual void getRTVideoInfo ( const std::string&  streamName, uint8_t& existingType, double& fps, bool cleanUp = false ) = 0;


    virtual void setYTDemuxer    ( const std::string&  streamName, AbstractDemuxer* demuxer) = 0;
    virtual void removeYTDemuxer ( const std::string&  streamName) = 0;

    virtual void setRTDemuxer    ( const std::string&  streamName, AbstractDemuxer* demuxer) = 0;
    virtual void removeRTDemuxer ( const std::string&  streamName) = 0;
    
    virtual void resumeStreamingGroup ( const char* name ) = 0;
    virtual void setSyncGroupCount ( int count ) = 0;
    virtual int  createStreamingGroup(const char* address, 
							  const char* name, double nptPos, int scale, 
							  int lowLevelTransport,
							  int loopCount,
							  int eoplDelay,
							  int eofDelay, 
                              bool suspended ) = 0 ;
	virtual int  startStreamingDVD(const char* groupName, int llTransport) = 0;
	virtual int  startStreamingDVDExt(const char* groupName, int llTransport, 
							  const char* dvdDrive,
							  const char* folder) = 0 ;
	virtual double  getGroupNptPos(const char* groupName) = 0 ;
	virtual bool  isStreamingGroup(const char* fileName) = 0 ;
	virtual uint32_t  addDestination(const char* session,
							 const char* fileName,
						     const char* ip, uint16_t port,
						     const char* rtpInfo,
						     long tcpSocketHandle,
						     const char* ixface) = 0 ;
	virtual int  removeDestination(const char* fileName, long handle) = 0 ;
	virtual int  stopStreaming(unsigned streamer) = 0 ;
	virtual unsigned  pauseStreaming(unsigned readerID) = 0 ;
	virtual unsigned  resumeStreaming(unsigned readerID) = 0 ;
	virtual unsigned  changeScale(unsigned readerID, int scale) = 0 ;
	virtual unsigned  setPosition(unsigned readerID, double position) = 0 ;
	virtual double  getNptPos(unsigned streamer) = 0 ;
	virtual int  getRewindSpeed(int requestSpeed) = 0 ;
	virtual unsigned  createIndexFile(const char* fileName, bool createRewind) = 0 ;
	virtual unsigned  createIndexFileAsync(const char* displayName, const char* fileName, bool createRewind) = 0 ;
	virtual unsigned  getAssetStatus(const char* fileName) = 0 ;
	virtual bool  getEncodedAssetList(char*rezBuffer, int bufferSize) = 0 ;
	virtual bool  startAutoIndexing(const string& dir, bool createRewind) = 0 ;
	virtual bool  stopAutoIndexing() = 0 ;
	virtual long  multicastJoin(const char* assetName, const char* address, int port, bool overWrite, int cycleBufferLen, const char* ixface, bool createRewind, int nBlockSize, int splitByDurationSize ) = 0 ;
	virtual int  multicastLeave(const char* encoderName) = 0 ;
	virtual void  setReadThreadCnt(int readThreadCnt) = 0 ;
	virtual void  initLog(int messageLevel, 
				  const char* logName,
				  const char* logDir,
				  int logRotationPeriod) = 0 ;
	virtual void  getAssetDescription ( const char* sessionID, const char* cfileName, int descrType,
													 char* dstBuffer, unsigned len) = 0 ;
	virtual void registerEventHandler(EVENT_HANDLER_PROC eventHandler) = 0 ;
	virtual int getAssetInfoFromFile(const char* fileName, char* dstBuffer, int bufSize) = 0 ;
    virtual int  getTSDuration ( const char* fileName ) = 0;
	virtual int  deleteAsset(const char* fileName) = 0 ;
	virtual int  deleteStreamingGroup(const char* groupName) = 0 ;
	virtual bool  setRootDir(const char* rootDir) = 0 ;
	virtual bool  setM3uRoot(const char* rootDir) = 0 ;
	virtual bool  setHttpServerURL(const char* url) = 0 ;
	virtual bool  setDefaultRtspTimeout(const int timeout) = 0 ;
	virtual void  setTrafficBandWidth ( int value ) = 0;
	virtual uint32_t getTrafficBandWidth () = 0;
	
	virtual int getStatisticForPeriod ( int period,
                                        double& avgBroadcastBitrate,
                                        double& maxBroadcastBitrate,
                                        double& avgRunoffBitrate,
                                        double& maxRunoffBitrate,
                                        char* interval,
                                        int intervalSize 
                                      ) = 0;
                                      
	virtual int LIB_STD_CALL muxTSFile( std::map<int, MuxTrackInfo> trackInfo,
								const string& outName,
								int   indexingType, int nBitrate, int nVBVLen, int nFileBlockSize ) = 0;
	virtual AbstractReader* getReader(const char* streamName) = 0;

    virtual uint64_t getYTCacheDuration () = 0;
    virtual string getYTCacheDirectory () = 0;

    virtual uint64_t getRTCacheDuration () = 0;
    virtual string getRTCacheDirectory () = 0;

    virtual string getLogDirectory () = 0;
    
    virtual string  getYTDomesticHost () = 0;
    virtual void    setYTDomesticHost ( const string& sDomesticHost ) = 0;
    
    virtual uint8_t getYTVideoQuality () = 0;
    virtual void    setYTVideoQuality ( const string& sQuality ) = 0;
    
    virtual bool    isYTCacheDisabled () = 0;
    virtual void    disableYTCache () = 0;

    virtual bool    isRTCacheDisabled () = 0;
    virtual void    disableRTCache () = 0;
    
    virtual bool isYTMp4TranscodingDisabled() = 0;
    virtual void disableYTMp4Transcoding () = 0;

    virtual void setYTVideoSize ( const uint32_t width, const uint32_t height, const bool autoPal ) = 0;
    
    virtual uint32_t getYTVideoWidth () = 0;
    virtual uint32_t getYTVideoHeight () = 0;
    virtual uint32_t getYTAutoPal () = 0;
    
    virtual uint32_t getYTLogoPos() = 0;
    virtual string getYTLogoPath() = 0;
    
    virtual void setYTLogoPos ( uint32_t pos ) = 0;
    virtual void setYTLogoPath ( const string& path ) = 0;

    virtual void setRTVideoSize ( const uint32_t width, const uint32_t height, const bool autoPal ) = 0;

    virtual uint32_t getRTVideoWidth () = 0;
    virtual uint32_t getRTVideoHeight () = 0;
    virtual uint32_t getRTAutoPal () = 0;

    virtual uint8_t getRTScalingType () = 0;
    virtual void    setRTScalingType  ( const std::string& sType ) = 0;
    
    virtual bool cacheDemoMode() = 0;
    virtual void enableCacheDemoMode() = 0;

    virtual uint8_t getYTScalingType () = 0;
    virtual void    setYTScalingType  ( const std::string& sType ) = 0;

    virtual string  getMediaInterface() = 0;
    virtual void    setMediaInterface ( const string& sMediaInterface ) = 0;

    virtual bool checkTS( string fileName ) = 0;
    
    virtual void setTsMtuSize ( const int value ) = 0;
    virtual void setMaxMIpPacketCount ( const int value ) = 0;
    virtual void setMaxUIpPacketCount ( const int value ) = 0;

    virtual uint32_t getTsMtuSize() = 0;
    virtual uint32_t getMaxMIpPacketCount() = 0;
    virtual uint32_t getMaxUIpPacketCount() = 0;
    virtual bool     sendBlockBySinglePackets() = 0;
    virtual void     enableBlockSendingBySinglePackets() = 0;
    virtual void     enableMaxSpeedMode() = 0;
    virtual bool     maxSpeedMode() = 0;
    
    virtual bool cyclicMulticastEncoder ( string name ) = 0;
    virtual int cyclicMulticastEncoderLen ( string name ) = 0;
    virtual bool isMulticastEncoder ( string name ) = 0;


    //virtual double getCyclicNptPos ( string name ) = 0;
    
#ifndef __TS_MUXER_COMPILE_MODE
    virtual string getUserAgentForStream ( const string& streamName  ) = 0;
	virtual SocketAddress getDestinationForStream ( const string& streamName  ) = 0;
    virtual void addStreamInfo ( const string& streamName, const SocketAddress& address, const string& userAgent ) = 0;
    virtual void delStreamInfo ( const string& streamName ) = 0;
#endif
    
protected:
    uint64_t m_ytCacheDuration, m_rtCacheDuration;
    string   m_sLogDirectory,   m_sMediaInterface;
    string   m_sYTDomesticHost, m_ytCacheDirectory, m_rtCacheDirectory;
    uint8_t  m_nYTVideoQuality;
    uint8_t  m_nYTScalingType, m_nRTScalingType;

    bool     m_ytCacheDisabled, m_ytMp4TranscodingDisabled, 
             m_ytAutoPal, m_sendBlockBySinglePackets,
             m_rtAutoPal, m_rtCacheDisabled, m_cacheDemoMode, m_maxSpeedMode;

    uint32_t m_ytVideoWidth, m_ytVideoHeight,
             m_rtVideoWidth, m_rtVideoHeight;

    uint32_t m_tsMtuSize, m_maxMIpPacketCount, m_maxUIpPacketCount, m_trafficBandWidth;
    
    uint32_t m_ytLogoPos;
    string   m_ytLogoPath;

#ifndef __TS_MUXER_COMPILE_MODE
    YTVideoInfoMap m_videoInfoMap;
    StreamInfoMap m_streamInfoMap;
#endif
};
}
#endif

