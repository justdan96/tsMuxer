#include "abstractCoreContext.h"
#include "vodTransport.h"

#include "vod_common.h"
#include <fs/systemlog.h>

#define NO_SCALING     0
#define FFMPEG_SCALING 1
#define SM_SCALING     2

using namespace std;

namespace vodcore
{

class VodCoreContext: public AbstractCoreContext
{
    typedef std::pair < uint8_t, double > ParamPair;
    typedef std::pair < string, ParamPair > InfoPair;
    

public:
	unsigned startStreaming(    const char* session,
								int streamingMode,
								double ntpPos, 
								int scale,
								const char* fileName, 
								const char* address, 
								unsigned short port,
								const char* rtpInfo,
								long tcpSocketHandle) 
	{
		return vodcore::startStreaming(session, streamingMode, ntpPos, scale, fileName, address, port, rtpInfo, tcpSocketHandle);
	}

	unsigned  startSmartBridgeStreaming ( const char* sessionID, double ntpPos,
									      const char* url, 
									      const char* address, 
									      unsigned short port, const char* rtpInfo,
									      long tcpSocketHandle
                                        ) 
	{
		return vodcore::startSmartBridgeStreaming ( sessionID, ntpPos, url, address, port, rtpInfo, tcpSocketHandle );
	}

    void  setYTCacheParams ( const string& sCacheDirectory, const string& sLogDirectory, uint64_t nTotalCacheDuration ) 
    {
        
        m_ytCacheDirectory = sCacheDirectory;
        m_sLogDirectory = sLogDirectory;
        m_ytCacheDuration = nTotalCacheDuration;
    }

    void  setRTCacheParams ( const string& sCacheDirectory, uint64_t nTotalCacheDuration ) 
    {

        m_rtCacheDirectory = sCacheDirectory;
        m_rtCacheDuration = nTotalCacheDuration;
    }

	void removeYTDemuxer ( const std::string&  streamName) {
		Mutex::ScopedLock lock(&m_demuxerMutex);
		m_demuxers.erase(streamName);
	}

	void setYTDemuxer(const std::string& streamName, AbstractDemuxer* demuxer) { 
		Mutex::ScopedLock lock(&m_demuxerMutex);
		m_demuxers[streamName] = demuxer; 
	}

    void removeRTDemuxer ( const std::string&  streamName) {
        removeYTDemuxer ( streamName );
    }

    void setRTDemuxer(const std::string& streamName, AbstractDemuxer* demuxer) { 
        setYTDemuxer ( streamName, demuxer ); 
    }

	unsigned  seekYouTubeStreaming ( const char* streamName, double nptPos) 
	{
		AbstractDemuxer* demuxer = 0;
		{
			Mutex::ScopedLock lock(&m_demuxerMutex);
			map<string, AbstractDemuxer*>::iterator itr = m_demuxers.find(streamName);
			if (itr != m_demuxers.end()) 
				demuxer = itr->second;
		}
		if (demuxer)
		{
			demuxer->readSeek(1, nptPos);
			return ( unsigned )nptPos;
        }
		else 
			return  -1;
	}

    unsigned  seekRuTubeStreaming ( const char* streamName, double nptPos) 
    {
        return seekYouTubeStreaming ( streamName, nptPos );
    }

    uint64_t getRTStreamDuration ( const std::string& streamName ) 
    {
        return getYTStreamDuration ( streamName );
    }

    uint64_t getYTStreamDuration ( const std::string& streamName ) 
    {
		AbstractDemuxer* demuxer = 0;
        uint64_t duration = 0;
		{
			Mutex::ScopedLock lock(&m_demuxerMutex);
			map<string, AbstractDemuxer*>::iterator itr = m_demuxers.find(streamName);
			if ( itr != m_demuxers.end() )
				demuxer = itr->second;
		}
		if (demuxer)
			duration = demuxer->getDuration ();

        return  duration;
    }

    void  setYTVideoInfo ( const std::string&  streamName, uint8_t existingType, double fps )
    {
        m_videoInfoMap.insert ( InfoPair ( streamName, ParamPair ( existingType, fps ) ) );
    }

    void  setRTVideoInfo ( const std::string&  streamName, uint8_t existingType, double fps )
    {
        setYTVideoInfo ( streamName, existingType, fps );
    }

    void getYTVideoInfo ( const std::string&  streamName, uint8_t& existingType, double& fps, bool cleanUp )
    {
        if ( m_videoInfoMap.count ( streamName ) > 0 )
        {
            YTVideoInfoMap::iterator iter = m_videoInfoMap.find ( streamName );

            if ( iter != m_videoInfoMap.end() )
            {
                existingType = iter->second.first;
                fps = iter->second.second;

                if ( cleanUp )
                    m_videoInfoMap.erase ( streamName );
            }
        }
    }

    void getRTVideoInfo ( const std::string&  streamName, uint8_t& existingType, double& fps, bool cleanUp )
    {
        getYTVideoInfo ( streamName, existingType, fps, cleanUp );
    }

    uint64_t getYTCacheDuration () 
    {
        return m_ytCacheDuration;
    }

    uint64_t getRTCacheDuration () 
    {
        return m_rtCacheDuration;
    }

    string getYTCacheDirectory () { return m_ytCacheDirectory; }
    string getRTCacheDirectory () { return m_rtCacheDirectory; }

    string getLogDirectory () { return m_sLogDirectory; }

    void setYTDomesticHost ( const std::string& sDomesticHost )
    {
        if ( !sDomesticHost.empty () )
            m_sYTDomesticHost = sDomesticHost;
    }

    void  setYTVideoQuality ( const std::string& sQuality )
    {
        string sQ = strToLowerCase ( trimStr ( sQuality ) );

        if ( sQ == "high" || sQ == "mp4" )
            m_nYTVideoQuality = 18;
        else
            m_nYTVideoQuality = 5;
    }
    
    virtual uint32_t getYTLogoPos() { return m_ytLogoPos; }
    virtual string getYTLogoPath() { return m_ytLogoPath;}

    virtual void setYTLogoPos ( uint32_t pos ) { m_ytLogoPos = pos; }
    virtual void setYTLogoPath ( const string& path ) {  m_ytLogoPath = path; }


    void  disableYTCache ()
    {
        m_ytCacheDisabled = true;
    }

    void  disableRTCache ()
    {
        m_rtCacheDisabled = true;
    }

    void disableYTMp4Transcoding ()
    {
        m_ytMp4TranscodingDisabled = true;
    }

    void setYTVideoSize ( const uint32_t width, const uint32_t height, const bool autoPal )
    {
        m_ytVideoWidth  = !autoPal && width > 0 && width < uint32_t ( - 1 ) ? width : 352;
        m_ytVideoHeight  = !autoPal && height > 0 && height < uint32_t ( - 1 ) ? height : 288;

        m_ytAutoPal = autoPal;
    }

    void setRTVideoSize ( const uint32_t width, const uint32_t height, const bool autoPal )
    {
        m_rtVideoWidth  = !autoPal && width > 0 && width < uint32_t ( - 1 ) ? width : 352;//476
        m_rtVideoHeight  = !autoPal && height > 0 && height < uint32_t ( - 1 ) ? height : 288;//320

        m_rtAutoPal = autoPal;
    }

    virtual uint32_t getYTVideoWidth ()  { return m_ytVideoWidth;  }
    virtual uint32_t getYTVideoHeight () { return m_ytVideoHeight; }
    virtual uint32_t getYTAutoPal ()     { return m_ytAutoPal;     }

    virtual uint32_t getRTVideoWidth ()  { return m_rtVideoWidth;  }
    virtual uint32_t getRTVideoHeight () { return m_rtVideoHeight; }
    virtual uint32_t getRTAutoPal ()     { return m_rtAutoPal;     }


    bool isYTCacheDisabled ()
    {
        return m_ytCacheDisabled;
    }

    bool isRTCacheDisabled ()
    {
        return m_rtCacheDisabled;
    }

    void setRTScalingType  ( const std::string& sType )
    {
        string sScalingType = strToLowerCase ( trimStr ( sType ) );

        if ( sScalingType == "ffmpeg" )
            m_nRTScalingType = FFMPEG_SCALING;
        else if ( sScalingType == "smartlabs")
            m_nRTScalingType = SM_SCALING;
        else
            m_nRTScalingType = NO_SCALING;
    }

    uint8_t getRTScalingType () { 
        return m_nRTScalingType; 
    }

    void setYTScalingType  ( const std::string& sType )
    {
        string sScalingType = strToLowerCase ( trimStr ( sType ) );

        if ( sScalingType == "ffmpeg" )
            m_nYTScalingType = FFMPEG_SCALING;
        else if ( sScalingType == "smartlabs")
            m_nYTScalingType = SM_SCALING;
        else
            m_nYTScalingType = NO_SCALING;
    }

    uint8_t getYTScalingType () { 
		return m_nYTScalingType; 
	}

    uint8_t getYTVideoQuality () { return m_nYTVideoQuality; }

    bool isYTMp4TranscodingDisabled () { return m_ytMp4TranscodingDisabled; }

    string getYTDomesticHost ()
    {
        if ( m_sYTDomesticHost.empty() )
        {
            uint32_t nSize = 255;
            char* pDstBuff = new char [ nSize ];
            vodcore::getYTDomesticHost ( pDstBuff, nSize );
            m_sYTDomesticHost = string ( pDstBuff, nSize );
            delete [] pDstBuff;
        }

        return m_sYTDomesticHost;
    }

	int  createStreamingGroup(const char* address, 
							  const char* name, double nptPos, int scale, 
							  int lowLevelTransport,
							  int loopCount,
							  int eoplDelay,
							  int eofDelay,
                              bool  suspended
                             ) 
	{
		return vodcore::createStreamingGroup(address, name, nptPos, scale, lowLevelTransport, loopCount, eoplDelay, eofDelay, suspended);
	}

    void setSyncGroupCount ( int count )
    {
        return vodcore::setSyncGroupCount ( count );
    }

    void resumeStreamingGroup ( const char* name )
    {
        return vodcore::resumeStreamingGroup ( name );
    }
	int  startStreamingDVD(const char* groupName, int llTransport)
	{
		return vodcore::startStreamingDVD(groupName, llTransport);
	}

	int  startStreamingDVDExt(const char* groupName, int llTransport, 
							  const char* dvdDrive,
							  const char* folder) 
	{
		return vodcore::startStreamingDVDExt(groupName, llTransport, dvdDrive, folder);
	}

	double  getGroupNptPos(const char* groupName)
	{
		return vodcore::getGroupNptPos(groupName);
	}

	bool  isStreamingGroup(const char* fileName)
	{
		return vodcore::isStreamingGroup(fileName);
	}

	uint32_t  addDestination(const char* session,
							 const char* fileName,
						     const char* ip, uint16_t port,
						     const char* rtpInfo,
						     long tcpSocketHandle,
						     const char* ixface)
	{
		return vodcore::addDestination(session, fileName, ip, port, rtpInfo, tcpSocketHandle, ixface);
	}

	int  removeDestination(const char* fileName, long handle)
	{
		return vodcore::removeDestination(fileName, handle);
	}

	int  stopStreaming(unsigned streamer)
	{
		return vodcore::stopStreaming(streamer);
	}

	unsigned  pauseStreaming(unsigned readerID)
	{
		return vodcore::pauseStreaming(readerID);
	}

	unsigned  resumeStreaming(unsigned readerID)
	{
		return vodcore::resumeStreaming(readerID);
	}

	unsigned  changeScale(unsigned readerID, int scale)
	{
		return vodcore::changeScale(readerID, scale);
	}

	unsigned  setPosition(unsigned readerID, double position)
	{
		return vodcore::setPosition(readerID, position);
	}

	double  getNptPos(unsigned streamer)
	{
		return vodcore::getNptPos(streamer);
	}

	int  getRewindSpeed(int requestSpeed)
	{
		return vodcore::getRewindSpeed(requestSpeed);
	}
	
	unsigned  createIndexFile(const char* fileName, bool createRewind)
	{
		return vodcore::createIndexFile(fileName, createRewind);
	}

	unsigned  createIndexFileAsync(const char* displayName, const char* fileName, bool createRewind)
	{
		return vodcore::createIndexFileAsync(displayName, fileName, createRewind);
	}

	unsigned  getAssetStatus(const char* fileName)
	{
		return vodcore::getAssetStatus(fileName);
	}

	bool  getEncodedAssetList(char*rezBuffer, int bufferSize)
	{
		return vodcore::getEncodedAssetList(rezBuffer, bufferSize);
	}

	bool  startAutoIndexing(const std::string& dir, bool createRewind)
	{
		return vodcore::startAutoIndexing(dir, createRewind);
	}

	bool  stopAutoIndexing()
	{
		return vodcore::stopAutoIndexing();
	}


	long  multicastJoin(const char* assetName, const char* address, int port, bool overWrite, int cycleBufferLen, const char* ixface, bool createRewind, int nBlockSize, int splitByDurationSize )
	{
		return vodcore::multicastJoin(assetName, address, port, overWrite, cycleBufferLen, ixface, createRewind, nBlockSize, splitByDurationSize);
	}

	int  multicastLeave(const char* encoderName)
	{
		return vodcore::multicastLeave(encoderName);
	}

	void  setReadThreadCnt(int readThreadCnt)
	{
		return vodcore::setReadThreadCnt(readThreadCnt);
	}

	void  initLog(int messageLevel, 
				  const char* logName,
				  const char* logDir,
				  int logRotationPeriod)
	{
		return vodcore::initLog(messageLevel, logName, logDir, logRotationPeriod);
	}

	void  getAssetDescription ( const char* sessionID, const char* cfileName, int descrType, char* dstBuffer, unsigned len)
	{
		return vodcore::getAssetDescription ( sessionID, cfileName, descrType, dstBuffer, len );
	}

	void registerEventHandler(EVENT_HANDLER_PROC eventHandler)
	{
		return vodcore::registerEventHandler(eventHandler);
	}

	int  getAssetInfoFromFile(const char* fileName, char* dstBuffer, int bufSize)
	{
		return vodcore::getAssetInfoFromFile(fileName, dstBuffer ,bufSize);
	}

	int  deleteAsset(const char* fileName)
	{
		return vodcore::deleteAsset(fileName);
	}

	int  deleteStreamingGroup(const char* groupName)
	{
		return vodcore::deleteStreamingGroup(groupName);
	}
	
	bool  setRootDir(const char* rootDir)
	{
		return vodcore::setRootDir(rootDir);
	}
	
	bool  setM3uRoot(const char* rootDir)
	{
		return vodcore::setM3uRoot(rootDir);
	}
	
	bool  setHttpServerURL(const char* url)
	{
		return vodcore::setHttpServerURL(url);
	}
	
	bool  setDefaultRtspTimeout(const int timeout)
	{
		return vodcore::setDefaultRtspTimeout(timeout);
	}
	
	int LIB_STD_CALL muxTSFile( std::map<int, MuxTrackInfo> trackInfo,
								const std::string& outName,
								int   indexingType, int nBitrate, int nVBVLen, int nFileBlockSize )
	{
		return vodcore::muxTSFile(trackInfo, outName, indexingType, nBitrate, nVBVLen, nFileBlockSize );
	}
	AbstractReader* getReader(const char* streamName) {
		return vodcore::getReader(streamName);
	}
	
	
	bool checkTS( string fileName )
	{
	    return vodcore::checkTS ( fileName );
	}

    int  getTSDuration ( const char* fileName )
    {
        return vodcore::getTSDuration(fileName);
    }

    string getMediaInterface()
    {
        return m_sMediaInterface;
    }

    void setMediaInterface ( const string& sMediaInterface )
    {
        m_sMediaInterface = sMediaInterface; 
    }

    void setTsMtuSize ( const int value )
    {
        if  ( value > 1500 )
            LTRACE ( LT_WARN, 0, "Max TS MTU size in Ethernet is 1500B. Value " << value << " is wrong. Used default TS MTU size ( 1400B ). Line: " << __LINE__ )
        else if ( value > 0 )
        {
            m_tsMtuSize = value;
            LTRACE ( LT_DEBUG, 0, "TS MTU size is " << value << "B. Line: " << __LINE__ )
        }
        else
            LTRACE ( LT_WARN, 0, "TS MTU size must be > 0. Value " << value << " is wrong. Used default TS MTU size ( 1400B ). Line: " << __LINE__ )
    }

    void setMaxMIpPacketCount ( const int value )
    {
        if ( value == 0 )
            LTRACE ( LT_WARN, 0, "Value 'auto' is wrong for max multicast ip packet count. Used default value ( 6 ). Line: " << __LINE__ )
        else if ( value < 0 )
            LTRACE ( LT_WARN, 0, "Value " << value << " is wrong for max multicast ip packet count. Used default value ( 6 ). Line: " << __LINE__ )
        else if ( value * m_tsMtuSize > 16 * 1024 )
            LTRACE ( LT_WARN, 0, "Value " << value << " is wrong for max multicast ip packet count ( maxMIpPacketCount * tsMtuSize > 16K ). Used default value ( 6 ). Line: " << __LINE__ )
        else
        {
            m_maxMIpPacketCount = value;
            LTRACE ( LT_DEBUG, 0, "Max multicast ip packet count is " << value << ". Line: " << __LINE__ )
        }
    }

    void setMaxUIpPacketCount ( const int value )
    {
        if ( value < 0 )
            LTRACE ( LT_WARN, 0, "Value " << value << " is wrong for max unicast ip packet count. Used default value ( 6 ). Line: " << __LINE__ )
        else if ( value * m_tsMtuSize > 16 * 1024 )
            LTRACE ( LT_WARN, 0, "Value " << value << " is wrong for max unicast ip packet count ( maxUIpPacketCount * tsMtuSize > 16K ). Used default value ( 6 ). Line: " << __LINE__ )
        else
        {
            m_maxUIpPacketCount = value;
            string str = "'auto'";

            if ( value == 0 )
                LTRACE ( LT_DEBUG, 0, "Max unicast ip packet count is 'auto'. Line: " << __LINE__ )
            else
                LTRACE ( LT_DEBUG, 0, "Max unicast ip packet count is " << value << ". Line: " << __LINE__ )
        }
    }

    virtual uint32_t getTsMtuSize()         { return m_tsMtuSize; }
    virtual uint32_t getMaxMIpPacketCount() { return m_maxMIpPacketCount; }
    virtual uint32_t getMaxUIpPacketCount() { return m_maxUIpPacketCount; }
    
    virtual SocketAddress getDestinationForStream ( const string& streamName  )
    {
		Mutex::ScopedLock lock(&m_streamInfoMutex);
        AbstractCoreContext::StreamInfoMap::iterator itr = m_streamInfoMap.find ( streamName );
        return itr != m_streamInfoMap.end () ? itr->second.first : SocketAddress();
    }

    virtual string getUserAgentForStream ( const string& streamName  )
    {
		Mutex::ScopedLock lock(&m_streamInfoMutex);
        AbstractCoreContext::StreamInfoMap::iterator itr = m_streamInfoMap.find ( streamName );
        return itr != m_streamInfoMap.end () ? itr->second.second : "";
    }

    virtual bool sendBlockBySinglePackets()
    {
        return m_sendBlockBySinglePackets;
    }

    virtual void enableBlockSendingBySinglePackets()
    {
        m_sendBlockBySinglePackets = true;
    }
    
    virtual void  setTrafficBandWidth ( int value )
    {
        m_trafficBandWidth = value;
    }
    
    virtual uint32_t getTrafficBandWidth ()
    {
        return m_trafficBandWidth;
    }
    
    virtual int getStatisticForPeriod ( int period,
                                        double& avgBroadcastBitrate,
                                        double& maxBroadcastBitrate,
                                        double& avgRunoffBitrate,
                                        double& maxRunoffBitrate,
                                        char* interval,
                                        int intervalSize 
                                      )
    {
        return vodcore::getStatisticForPeriod ( period, avgBroadcastBitrate, maxBroadcastBitrate, avgRunoffBitrate, maxRunoffBitrate, interval, intervalSize );
    }
    
    
    virtual bool cyclicMulticastEncoder ( string name )
    {
        return vodcore::cyclicMulticastEncoder ( name );
    }

    virtual bool isMulticastEncoder ( string name )
    {
        return vodcore::isMulticastEncoder ( name );
    }

    virtual int cyclicMulticastEncoderLen ( string name )
    {
        return vodcore::cyclicMulticastEncoderLen ( name );
    }

	/*
    virtual double getCyclicNptPos ( string name )
    {
        return vodcore::getCyclicNptPos ( name );
    }
	*/
    
    virtual void addStreamInfo ( const string& streamName, const SocketAddress& address, const string& userAgent )
    {
		Mutex::ScopedLock lock(&m_streamInfoMutex);
        m_streamInfoMap [ streamName ] = make_pair ( address, userAgent );
    }

    virtual void delStreamInfo ( const string& streamName )
    {
		Mutex::ScopedLock lock(&m_streamInfoMutex);
        if ( m_streamInfoMap.count ( streamName ) == 1 )
            m_streamInfoMap.erase ( streamName );
    }
    
    virtual bool cacheDemoMode() { return m_cacheDemoMode; }
    virtual void enableCacheDemoMode() { m_cacheDemoMode = true; }
    virtual void enableMaxSpeedMode() { m_maxSpeedMode = true; }
    virtual bool maxSpeedMode() { return m_maxSpeedMode; }
    
private:
	std::map<std::string, AbstractDemuxer*> m_demuxers;
	Mutex m_demuxerMutex;
	Mutex m_streamInfoMutex;
};

AbstractCoreContext* vodCoreContext = new VodCoreContext();

//VodCoreContext mContext;
//AbstractCoreContext* vodCoreContext = &mContext;

}
