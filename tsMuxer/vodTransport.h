#ifndef __VOD_TRANSPORT_H
#define __VOD_TRANSPORT_H

#include "abstractCoreContext.h"
#include "crc32.h"

namespace vodcore
{

#define FEC_R 6
#define FEC_K 128

LIB_EXPORT void LIB_STD_CALL reservePorts ( const char* session,
										    int streamingMode,
                                            const char* address,
                                            unsigned short port,
                                            const char* rtpInfo,
                                            char* outBuff,
                                            int& outBuffSize 
                                          );

LIB_EXPORT void LIB_STD_CALL freePorts ( const char* session);


LIB_EXPORT unsigned LIB_STD_CALL startStreaming(const char* session,
												int lowLevelTransport,
	                                            double ntpPos, 
												int scale,
												const char* fileName, 
												const char* address, 
												unsigned short port,
												const char* rtpInfo,
												long tcpSocketHandle,
												bool doDownload = false,
												int64_t downloadLimit = 0);
												
LIB_EXPORT void LIB_STD_CALL setTrafficBandWidth ( int value );

LIB_EXPORT unsigned LIB_STD_CALL startEncodingServer(const char* xmlConfigFileName);

LIB_EXPORT unsigned LIB_STD_CALL startSmartBridgeStreaming ( const char* sessionID,
                                                             double ntpPos, 
														     const char* url, 
														     const char* address, 
														     unsigned short port,
                                                             const char* rtpInfo,
                                                             long tcpSocketHandle
                                                           );

LIB_EXPORT void LIB_STD_CALL retransmitStream ( const char* srcAddress,
                                                const char* srcIxface, 
                                                const char* dstAddress, 
                                                const char* fwdAddress, 
                                                bool emulateLack = false );

LIB_EXPORT long LIB_STD_CALL createRaptorStream(const char* srcAddress, int srcPort, const char* srcIxface,
												const char* dstAddress, int dstPort, const char* dstIxface,
												int repairSymbolCount, int symbolCount);

LIB_EXPORT void LIB_STD_CALL startRaptorEncoding();


LIB_EXPORT int LIB_STD_CALL createStreamingGroup ( const char* address, 
												   const char* name,
												   double nptPos,
												   int scale, 
 												   int lowLevelTransport,
												   int loopCount,
												   int eoplDelay,
												   int eofDelay,
												   bool suspended
												 );
LIB_EXPORT void LIB_STD_CALL resumeStreamingGroup(const char* groupName);

LIB_EXPORT void LIB_STD_CALL setSyncGroupCount ( int count );

LIB_EXPORT int LIB_STD_CALL startStreamingDVD(const char* groupName, int llTransport);

LIB_EXPORT int LIB_STD_CALL startStreamingDVDExt(const char* groupName, int llTransport, 
												 const char* dvdDrive,
												 const char* folder);
LIB_EXPORT double LIB_STD_CALL getGroupNptPos(const char* groupName);

LIB_EXPORT bool LIB_STD_CALL isStreamingGroup(const char* fileName);

LIB_EXPORT uint32_t LIB_STD_CALL addDestination ( const char* session,
												  const char* fileName,
												  const char* ip, uint16_t port,
												  const char* rtpInfo,
												  long tcpSocketHandle,
												  const char* ixface 
												);

LIB_EXPORT int LIB_STD_CALL removeDestination(const char* fileName, long handle);



LIB_EXPORT int LIB_STD_CALL stopStreaming(unsigned streamer);

LIB_EXPORT unsigned LIB_STD_CALL pauseStreaming(unsigned readerID);
LIB_EXPORT unsigned LIB_STD_CALL resumeStreaming(unsigned readerID);

LIB_EXPORT unsigned LIB_STD_CALL changeScale(unsigned readerID, int scale);

LIB_EXPORT int LIB_STD_CALL setPosition(unsigned readerID, double position);

LIB_EXPORT double LIB_STD_CALL getNptPos(unsigned streamer);

LIB_EXPORT int LIB_STD_CALL getRewindSpeed(int requestSpeed);

LIB_EXPORT unsigned LIB_STD_CALL milticastJoin(const char* address, unsigned short port, const char* ixface);

LIB_EXPORT unsigned LIB_STD_CALL createIndexFile(const char* fileName, bool createRewind);

LIB_EXPORT unsigned LIB_STD_CALL createIndexFileAsync(const char* displayName, const char* fileName, bool createRewind);

LIB_EXPORT unsigned LIB_STD_CALL getAssetStatus(const char* fileName);

LIB_EXPORT bool LIB_STD_CALL getEncodedAssetList(char*rezBuffer, int bufferSize);

LIB_EXPORT bool LIB_STD_CALL startAutoIndexing(const std::string& dir, bool createRewind);
LIB_EXPORT bool LIB_STD_CALL stopAutoIndexing();

LIB_EXPORT int LIB_STD_CALL updateIndexes();


LIB_EXPORT long LIB_STD_CALL multicastJoin(const char* assetName, const char* address, int port,
										   bool overWrite, int cycleBufferLen, const char* ixface,
										   bool createRewind, int nBlockSize, int splitByDurationSize);
										   
LIB_EXPORT int LIB_STD_CALL multicastLeave(const char* encoderName);

LIB_EXPORT void LIB_STD_CALL setReadThreadCnt(int readThreadCnt);

LIB_EXPORT void LIB_STD_CALL setReadOnlyIndexes(bool value);

LIB_EXPORT void LIB_STD_CALL initLog(int messageLevel, 
									 const char* logName,
									 const char* logDir,
									 int logRotationPeriod);

LIB_EXPORT void LIB_STD_CALL getAssetDescription ( const char* sessionID, const char* cfileName, int descrType,
												 char* dstBuffer, unsigned len);


LIB_EXPORT void registerEventHandler(EVENT_HANDLER_PROC eventHandler);
LIB_EXPORT int LIB_STD_CALL getAssetInfoFromFile ( const char* fileName, char* dstBuffer, int bufSize );

LIB_EXPORT int64_t LIB_STD_CALL getFileLength(const char* fileName, double fromOffset); // return whole file length for zerro offset, or partial file length from specified offset
LIB_EXPORT int LIB_STD_CALL getTSDuration ( const char* fileName );
LIB_EXPORT int LIB_STD_CALL deleteAsset(const char* fileName);

LIB_EXPORT int LIB_STD_CALL deleteStreamingGroup(const char* groupName);
LIB_EXPORT bool LIB_STD_CALL setRootDir(const char* rootDir);
LIB_EXPORT bool LIB_STD_CALL setM3uRoot(const char* rootDir);
LIB_EXPORT bool LIB_STD_CALL setHttpServerURL(const char* url);
LIB_EXPORT bool LIB_STD_CALL setDefaultRtspTimeout(const int timeout);

LIB_EXPORT void LIB_STD_CALL setTsMtuSize ( const int value );
LIB_EXPORT void LIB_STD_CALL setMaxMIpPacketCount ( const int value );
LIB_EXPORT void LIB_STD_CALL setMaxUIpPacketCount ( const int value );
LIB_EXPORT void LIB_STD_CALL enableBlockSendingBySinglePackets ();

LIB_EXPORT void LIB_STD_CALL enableCacheDemoMode();

LIB_EXPORT void LIB_STD_CALL setYTLogoPos ( uint32_t pos ); 
LIB_EXPORT void LIB_STD_CALL setYTLogoPath ( const std::string& path );

LIB_EXPORT void LIB_STD_CALL setYTCacheParams ( const std::string&  sCacheDirectory, const std::string&  sLogDirectory, uint64_t nTotalCacheDuration );
LIB_EXPORT void LIB_STD_CALL setRTCacheParams ( const std::string&  sCacheDirectory, uint64_t nTotalCacheDuration );

LIB_EXPORT void LIB_STD_CALL getYTVideoInfo ( const std::string&  streamName, uint8_t& existingType, double& fps, bool cleanUp = false );

LIB_EXPORT void LIB_STD_CALL getRTVideoInfo ( const std::string&  streamName, uint8_t& existingType, double& fps, bool cleanUp = false );

LIB_EXPORT void LIB_STD_CALL getYTDomesticHost ( char* dstBuff, uint32_t& nDstBuffSize  );
LIB_EXPORT void LIB_STD_CALL setYTDomesticHost ( const std::string& sDomesticHost );

LIB_EXPORT bool LIB_STD_CALL checkSmartBridgeCache ( const char* streamName );

LIB_EXPORT void LIB_STD_CALL setYTVideoQuality ( const std::string& sQuality );
LIB_EXPORT void LIB_STD_CALL setYTScalingType  ( const std::string& sType );

LIB_EXPORT void LIB_STD_CALL setRTScalingType  ( const std::string& sType );

LIB_EXPORT void LIB_STD_CALL disableYTCache    ();
LIB_EXPORT void LIB_STD_CALL disableYTMp4Transcoding ();

LIB_EXPORT void LIB_STD_CALL disableRTCache    ();

LIB_EXPORT void LIB_STD_CALL setYTVideoSize ( const uint32_t width, const uint32_t height, const bool autoPal );

LIB_EXPORT void LIB_STD_CALL setRTVideoSize ( const uint32_t width, const uint32_t height, const bool autoPal );

LIB_EXPORT void LIB_STD_CALL setMediaInterface ( const std::string& sMediaInterface );

LIB_EXPORT int LIB_STD_CALL getStatisticForPeriod ( int period, 
                                                    double& avgBroadcastBitrate,
                                                    double& maxBroadcastBitrate,
                                                    double& avgRunoffBitrate,
                                                    double& maxRunoffBitrate,
                                                    char* interval,
                                                    int intervalSize 
                                                  );

LIB_EXPORT int LIB_STD_CALL getChecksumForAsset ( const char* fileName, char* buffer, int& size );

std::string getLongFileLocation ( const std::string& fileName );
// indexingType == 0 - no indexing
// indexingType == 1 - positioning only
// indexingType == 2 - full indexing (pos, RB and RF files)
const static int NO_INDEX = 0;
const static int CREATE_REWIND_INDEX = 1;
const static int CREATE_FULL_INDEX = 2;
int LIB_STD_CALL muxTSFile( std::map<int, MuxTrackInfo> trackInfo,
							const std::string& outName,
							int   indexingType, int nBitrate, int nVBVLen, int nFileBlockSize );
LIB_EXPORT AbstractReader* LIB_STD_CALL getReader(const char* streamName);
LIB_EXPORT bool LIB_STD_CALL checkTS ( std::string fileName );

LIB_EXPORT void LIB_STD_CALL enableMaxSpeedMode();
LIB_EXPORT bool LIB_STD_CALL cyclicMulticastEncoder ( std::string name ); // it is a cyclic multicast encoder
LIB_EXPORT int LIB_STD_CALL cyclicMulticastEncoderLen ( std::string name );
LIB_EXPORT int cyclicStreamingFirstFileNum(const char* streamName);
LIB_EXPORT int cyclicStreamingLastFileNum(const char* streamName);

LIB_EXPORT bool LIB_STD_CALL isMulticastEncoder ( std::string name ); // it is any type multicast encoder
LIB_EXPORT void LIB_STD_CALL loadSmartBridge ( const char* configDir );
//LIB_EXPORT double LIB_STD_CALL getCyclicNptPos ( std::string name );
}

#endif
