#include <algorithm>
#include <iostream>
#include "metaDemuxer.h"
#include "tsMuxer.h"
#include "singleFileMuxer.h"
#include <time/time.h>
#include <fs/directory.h>
#include <fs/textfile.h>
#include <vector>
#include "utf8Converter.h"
#include "convertUTF.h"
#include "textSubtitles.h"
#include "mpegStreamReader.h"
#include "math.h"
#include "blank_patterns.h"
#include "muxerManager.h"
#include "psgStreamReader.h"
#include "iso_writer.h"
#include "blurayHelper.h"

using namespace std;

BufferedReaderManager readManager(2, DEFAULT_FILE_BLOCK_SIZE, DEFAULT_FILE_BLOCK_SIZE + MAX_AV_PACKET_SIZE, DEFAULT_FILE_BLOCK_SIZE / 2);
TSMuxerFactory tsMuxerFactory;
SingleFileMuxerFactory singleFileMuxerFactory;

static const char EXCEPTION_ERR_MSG[] = ". It does not have to be! Please contact application support team for more information.";

const char* APP_VERSION = "2.6.15";

//const static uint32_t BLACK_PL_NUM = 1900;
//const static uint32_t BLACK_FILE_NUM = 1900;

#define LTRACE2(level, msg) \
{\
	{ \
		if (level <= LT_WARN) cerr << msg;\
		else if (level == LT_INFO) cout << msg;\
        if (level <= LT_INFO)\
        sLastMsg = true;\
	}\
}
DiskType checkBluRayMux(const char* metaFileName, int& autoChapterLen, vector<double>& customChaptersList, 
                        int& firstMplsOffset, int& firstM2tsOffset, bool& insertBlankPL, int& blankNum, bool& stereoMode,
                        std::string& isoDiskLabel)
{
	autoChapterLen = 0;
    stereoMode = false;
	TextFile file(metaFileName, File::ofRead);
	string str;
	file.readLine(str);
    DiskType result = DT_NONE;
	while(str.length() > 0) 
	{
		if (strStartWith(str, "MUXOPT")) 
		{
			vector<string> params = splitQuotedStr(str.c_str(), ' ');
			for (unsigned i = 0; i < params.size(); i++) {
				vector<string> paramPair = splitStr(trimStr(params[i]).c_str(), '=');
				if (paramPair.size() == 0)
					continue;
				if (paramPair[0] == "--auto-chapters")
					autoChapterLen = strToInt32(paramPair[1].c_str())*60;
				else if (paramPair[0] == "--custom-chapters" && paramPair.size() > 1) {
					vector<string> chapList = splitStr(paramPair[1].c_str(),';');
					for (unsigned k = 0; k < chapList.size(); k++)
						customChaptersList.push_back(timeToFloat(chapList[k]));
				}
				else if (paramPair[0] == "--mplsOffset")
				{
					firstMplsOffset = strToInt32(paramPair[1].c_str());
					if (firstMplsOffset > 1999) 
						THROW(ERR_COMMON, "Too large m2ts offset " << firstMplsOffset);
				}
				else if (paramPair[0] == "--blankOffset")
				{
					blankNum = strToInt32(paramPair[1].c_str());
					if (blankNum > 1999) 
						THROW(ERR_COMMON, "Too large black playlist offset " << blankNum);
				}
				else if (paramPair[0] == "--m2tsOffset")
				{
					firstM2tsOffset = strToInt32(paramPair[1].c_str());
					if (firstM2tsOffset > 99999) 
						THROW(ERR_COMMON, "Too large m2ts offset " << firstM2tsOffset);
				}
				else if (paramPair[0] == "--insertBlankPL")
					insertBlankPL = true;
                else if (paramPair[0] == "--label")
                {
                    isoDiskLabel = paramPair[1];
                }
			}

			if (str.find("--blu-ray") != string::npos)
				result =  DT_BLURAY;
			else if (str.find("--avchd") != string::npos)
				result =  DT_AVCHD;
			else
				result =  DT_NONE;
		}
        else if (strStartWith(str, "V_MPEG4/ISO/MVC"))
        {
            stereoMode = true;
        }
            
		file.readLine(str);
	}
	return result;
}

void detectStreamReader(const char* fileName, MPLSParser* mplsParser, bool isSubMode)
{
	DetectStreamRez streamInfo = METADemuxer::DetectStreamReader(readManager, fileName, mplsParser == 0);
	vector<CheckStreamRez>& streams = streamInfo.streams;

    std::vector<MPLSStreamInfo> pgStreams3D;
    if (mplsParser && mplsParser->isDependStreamExist)
        pgStreams3D = mplsParser->getPgStreams();

	for (unsigned i = 0; i < streams.size(); i++)
	{
        if (streams[i].trackID != 0) {
            if (i > 0)
                LTRACE(LT_INFO, 2, "");
			LTRACE(LT_INFO, 2, "Track ID:    " << streams[i].trackID);
        }
		if (streams[i].codecInfo.codecID) 
        {
            if (mplsParser) 
            {
                MPLSStreamInfo streamInfo = mplsParser->getStreamByPID(streams[i].trackID);
                if (streamInfo.streamPID) {
                    if (streamInfo.isSecondary)
                        streams[i].isSecondary = true;
                }
                else {
                    if (!(streams[i].codecInfo.codecID == CODEC_V_MPEG4_H264_DEP && mplsParser->isDependStreamExist))
                        streams[i].unused = true;
                }
            }

            string postfix;
            if (isSubMode && streams[i].codecInfo.codecID == CODEC_S_PGS)
			    postfix = " (depended view)";
            LTRACE(LT_INFO, 2, "Stream type: " << streams[i].codecInfo.displayName << postfix);
            if (streams[i].isSecondary)
                LTRACE(LT_INFO, 2, "Secondary: 1");
            if (streams[i].unused)
                LTRACE(LT_INFO, 2, "Unselected: 1");

			LTRACE(LT_INFO, 2, "Stream ID:   " << streams[i].codecInfo.programName);
            std::string descr = streams[i].streamDescr;
            if (streams[i].codecInfo.codecID == CODEC_S_PGS)
            {
                // PG stream
                int pgTrackNum = streams[i].trackID - 0x1200;
                if (pgTrackNum >= 0 && pgTrackNum < pgStreams3D.size()) 
                {
                    if (pgStreams3D[pgTrackNum].offsetId != 0xff) {
                        descr += "   3d-plane: ";
                        descr += int32ToStr(pgStreams3D[pgTrackNum].offsetId);
                    }
                    else {
                        descr += "   3d-plane: zero";
                    }
                    if (pgStreams3D[pgTrackNum].isSSPG) {
                        descr += "   (stereo, right=";
                        descr += (pgStreams3D[pgTrackNum].rightEye->type == 2 ? "dep-view " : "");
                        descr += int32ToStr(pgStreams3D[pgTrackNum].rightEye->streamPID);
                        descr += ", left=";
                        descr += (pgStreams3D[pgTrackNum].leftEye->type == 2 ? "dep-view " : "");
                        descr += int32ToStr(pgStreams3D[pgTrackNum].leftEye->streamPID);
                        descr += ")";
                    }
                }
            }
			LTRACE(LT_INFO, 2, "Stream info: " << descr);
			LTRACE(LT_INFO, 2, "Stream lang: " << streams[i].lang);
			if (streams[i].delay)
				LTRACE(LT_INFO, 2, "Stream delay: " << streams[i].delay);
            if (streams[i].multiSubStream)
                LTRACE(LT_INFO, 2, "subTrack: " << (streams[i].codecInfo.codecID == CODEC_V_MPEG4_H264_DEP ? 1 : 2));
		}
		else
			LTRACE(LT_INFO, 2, "Can't detect stream type");
	}

    AVChapters& chapters = streamInfo.chapters;
    if (chapters.size() > 0 || streamInfo.fileDurationNano > 0)
        LTRACE(LT_INFO, 2, "");
    if (streamInfo.fileDurationNano)
        LTRACE(LT_INFO, 2, "Duration: " << floatToTime(streamInfo.fileDurationNano/1e9));
	for (int j = 0; j < chapters.size(); j++) 
	{
		uint64_t time = chapters[j].start;
		if (j % 5 == 0) {
			LTRACE(LT_INFO, 2, "");
			LTRACE2(LT_INFO, "Marks: ");
		}
		LTRACE2(LT_INFO, floatToTime(time/1e9) << " ");
	}
    if (chapters.size() > 0 || streamInfo.fileDurationNano > 0)
		LTRACE(LT_INFO, 2, "");
}

string getBlurayStreamDir(const string& mplsName)
{
	string dirName = extractFileDir(mplsName);
	dirName = toNativeSeparators(dirName);
	int tmp = dirName.substr(0,dirName.size()-1).find_last_of(getDirSeparator());
	if (tmp != string::npos) {
		dirName = dirName.substr(0, tmp+1);
		if (strEndWith(dirName, string("BACKUP") + getDirSeparator()))
		{
			tmp = dirName.substr(0,dirName.size()-1).find_last_of(getDirSeparator());
			if (tmp == string::npos)
				return "";
			dirName = dirName.substr(0, tmp+1);
		}
		return dirName + string("STREAM") + getDirSeparator();
	}
	else
		return "";
}

void muxBlankPL(const string& appDir, BlurayHelper& blurayHelper, const PIDListMap& pidList, DiskType dt, int blankNum)
{
	int videoWidth = 1920;
	int videoHeight = 1080;
	double fps = 23.976;
	for (PIDListMap::const_iterator itr = pidList.begin(); itr != pidList.end(); ++itr)
	{
		const PMTStreamInfo& streamInfo = itr->second;
		const MPEGStreamReader* streamReader = dynamic_cast<const MPEGStreamReader*> (streamInfo.m_codecReader);
		if (streamReader) 
		{
			videoWidth = streamReader->getStreamWidth();
			videoHeight = streamReader->getStreamHeight();
			fps = streamReader->getFPS();
			break;
		}
	}
	uint8_t* pattern;
	int patternSize;
	bool isNtsc = videoWidth <= 854 && videoHeight <= 480 && (fabs(25-fps) >= 0.5 && fabs(50-fps) >= 0.5);
	bool isPal = videoWidth <= 1024 && videoHeight <= 576 && (fabs(25-fps) < 0.5 || fabs(50-fps) < 0.5);
	if (isNtsc) {
		pattern = pattern_ntsc;
		patternSize = sizeof(pattern_ntsc);
	}
	else if (isPal) {
		pattern = pattern_pal;
		patternSize = sizeof(pattern_pal);
	}   
	else if (videoWidth >= 1300) {
		pattern = pattern_1920;
		patternSize = sizeof(pattern_1920);
	}
	else {
		pattern = pattern_1280;
		patternSize = sizeof(pattern_1280);
	}
	string tmpFileName = appDir + string("blank_") + int64ToStr(mtime::clockGetTimeEx()) + string(".264");
	File file;
	if (!file.open(tmpFileName.c_str(), File::ofWrite))
		THROW(ERR_COMMON, "can't create file " << tmpFileName);
	for (int i = 0; i < 3; ++i)
	{
		if (file.write(pattern, patternSize) != patternSize) {
			deleteFile(tmpFileName);
			THROW(ERR_COMMON, "can't write data to file " << tmpFileName);
		}
	}
	file.close();
	map<string, string> videoParams;
	videoParams["insertSEI"];
	videoParams["fps"] = "23.976";
	{
		MuxerManager muxerManager(readManager, tsMuxerFactory);
		muxerManager.parseMuxOpt("MUXOPT --no-pcr-on-video-pid --vbr --avchd --vbv-len=500");
		muxerManager.addStream("V_MPEG4/ISO/AVC", tmpFileName, videoParams);
		string dstFile = blurayHelper.m2tsFileName(blankNum);
		muxerManager.doMux(dstFile.c_str(), &blurayHelper);

        TSMuxer* tsMuxer = dynamic_cast<TSMuxer*> (muxerManager.getMainMuxer());

        blurayHelper.createMPLSFile(tsMuxer, 0, 0, vector<double>(), dt, blankNum, false);
		blurayHelper.createCLPIFile(tsMuxer, blankNum, true);
	}
	deleteFile(tmpFileName);
}

void doTrancatedFile(const char* fileName, int64_t offset)
{
    File f;
    File outFile;

    f.open(fileName, File::ofRead);
    std::string outName = std::string(fileName) + std::string(".back");
    outFile.open(outName.c_str(), File::ofWrite);

    uint8_t buffer[1024*64];
    f.seek(offset);
    int readed = f.read(buffer, sizeof(buffer));
    while (readed > 0)
    {
        outFile.write(buffer, readed);
        readed = f.read(buffer, sizeof(buffer));
    }
}

void showHelp()
{
    const char help[] = "\n\
tsMuxeR is  simple  program to  mux video to  TS/M2TS files or create BD disks.\n\
tsMuxeR does not use external filters (codecs).\n\
\n\
Examples:\n\
    tsMuxeR <media file name>\n\
    tsMuxeR <meta file name> <out file/dir name>\n\
\n\
tsMuxeR can be run in track detection mode or muxing mode. If run tsMuxeR  with\n\
only  one argument  then tsMuxeR  display  input track information  required to\n\
construct  meta  file.  If run tsMuxeR  with two arguments tsMuxeR start muxing\n\
or demuxing process.\n\
\n\
Meta file format:\n\
File MUST has extension .meta.  This file  define files you want to  multiplex.\n\
First line of meta file contain additional parameters that apply to all tracks.\n\
In this case the line should begin with the word MUXOPT.\n\
\n\
Following lines indicate a list of tracks  and their parameters.  The format is\n\
as follows:   <code name>,   <file name>,   <parameters>   Parameters are comma\n\
separated. Each parameter indicates the name and value.\n\
Example of META file:\n\
\n\
MUXOPT --blu-ray\n\
V_MPEG4/ISO/AVC, D:/media/test/stream.h264, fps=25\n\
A_AC3, D:/media/test/stream.ac3, timeshift=-10000ms\n\
\n\
In this example one AC3 audio stream and one H264 video stream are  multiplexed\n\
to BD disk.  Input file name can reference to elementary stream or track inside\n\
container.\n\
\n\
Supported input containers:\n\
- TS/M2TS/MTS\n\
- EVO/VOB/MPG/MPEG\n\
- MKV\n\
- MOV/MP4\n\
- MPLS (Blu-ray media play list file)\n\
\n\
Names of codecs in the meta file:\n\
- V_MPEGH/ISO/HEVC  H.265/HEVC\n\
- V_MPEG4/ISO/AVC   H.264/AVC\n\
- V_MPEG4/ISO/MVC   H.264/MVC\n\
- V_MS/VFW/WVC1     VC1\n\
- V_MPEG-2          MPEG2\n\
- A_AC3             AC3/AC3+/TRUE-HD\n\
- A_AAC             AAC\n\
- A_DTS             DTS/DTS-Express/DTS-HD\n\
- A_MP3             MPEG audio layer 1/2/3\n\
- A_LPCM            raw pcm data or PCM WAV file\n\
- S_HDMV/PGS        Presentation graphic stream (BD subtitle format)\n\
- S_TEXT/UTF8       SRT subtitle format.  Encoding MUST be  UTF-8/UTF-16/UTF-32\n\
\n\
Each track may has addition parameters.  Track parameters do not  have dash. If\n\
parameter value has several words, parameter must be enclosed in quotes.\n\
\n\
Common additional parameters for any type of track:\n\
- track             track number if input file is container.\n\
- lang              track language. MUST contains exact 3 letters.\n\
\n\
Additional parameters for audio tracks:\n\
- timeshift         Shift audio track to future (positive value) or to past.\n\
                    Measured at milliseconds.\n\
- down-to-dts       Available only for DTS-HD tracks. Filter out HD part.\n\
- down-to-ac3       Available only for TRUE-HD tracks. Filter out HD part.\n\
- secondary         Mux as secondary audio.  Available for DD+ and DTS-Express.\n\
\n\
Additional parameters for video tracks:\n\
- fps               Video fps. If not defined, default value auto detected from\n\
                    a source stream if present. If not, default value 23.976.\n\
- delPulldown       Remove pulldown from the track if exists.  This option lead\n\
                    to fps change from 30 to 24 if pulldown exists.\n\
- ar                Override video aspect ratio. 16:9, 4:3 e.t.c.\n\
\n\
Additional parameters for H.264 video tracks:\n\
- level             Overwrite  level in the H264 stream.  Note:  option  update\n\
                    headers only. The H264 stream may not meet the requirements\n\
                    of a lower level.\n\
- insertSEI         If original   stream  does not contain  SEI picture timing,\n\
                    SEI buffering period or VUI parameters,  then add this data\n\
                    to the stream. This option is recommended for BD muxing.\n\
- forceSEI          Add SEI picture timing, buffering period and VUI parameters\n\
                    to the stream. Rebuild data If data already exist. \n\
- contSPS           If original video doesn't contain  repetitive SPS/PPS  then\n\
                    SPS/PPS will be added to the stream before  each key frame.\n\
                    This option is recommended for BD muxing.\n\
- subTrack          Used  for  combined  AVC/MVC  tracks  only.  TsMuxeR always\n\
                    demultiplex  such  tracks to separate  AVC and MVC streams.\n\
                    This parameter defined reference to AVC part(if value=1) or\n\
                    or to MVC part (if value=2).\n\
- secondary         Mux as secondary video (PIP).\n\
- pipCorner         Corner for PIP video. Allowed values: \"TopLeft\",\"TopRight\",\n\
                    \"BottomRight\", \"BottomLeft\". \n\
- pipHOffset        PIP window horizontal offset from the corner in pixels.\n\
- pipVOffset        PIP window vertical offset from the corner in pixels.\n\
- pipScale          PIP window scale factor. Allowed values: \"1\", \"1/2\", \"1/4\",\n\
                    \"1.5\", \"fullScreen\".\n\
- pipLumma          Allow PIP window to be transparent. Transparent colors  are\n\
                    lumma colors in range [0..pipLumma]. \n\
\n\
Additional parameters for PG and SRT tracks:\n\
\n\
- video-width       The width of the video in pixels.\n\
- video-height      The height of the video in pixels.\n\
- fps               Video fps.  Recommended  to  define this parameter for more\n\
                    carefully timing processing.\n\
- 3d-plane          Parameter  defines  number  of  the '3D offset track' which\n\
                    placed  inside  MVC  track.  Each message has individual 3D\n\
                    offset. This information  stored  inside 3D offset track.\n\
\n\
Additional parameters for SRT tracks:\n\
\n\
- font-name         Font name to render.\n\
- font-color        Font color. Color can be defined in hexadecimal or  decimal\n\
                    format. If color 24 bit long  (for instance 0xFF00FF)  it's\n\
                    define RGB components.  IF color 32 bit long  (for instance\n\
                    0x80FF00FF) it's define ARGB components.\n\
- font-size         Font size in pixels.\n\
- font-italic       Italic display text.\n\
- font-bold         Bold display text.\n\
- font-underline    Underlined text.\n\
- font-strikeout    Strikethrough text.\n\
- bottom-offset     Distance from the lower edge while displaying text.\n\
- font-border       Outline width.\n\
- fadein-time       Time in ms for smooth subtitle appearance.\n\
- fadeout-time      Time in ms for smooth subtitle disappearance.\n\
- line-spacing      Interval between lines. Default value 1.0.\n\
\n\
tsMuxeR  supports  addition  tag inside  SRT track.  The syntax  and parameters\n\
coincide with HTML: <b>, <i>, <u>, <strike>, <font>. Default relative font size\n\
(used in these tags) - 3.  For example:\n\
\n\
<b><font size=5 color=\"deepskyblue\" name=\"Arial\"><u>Test</u>\n\
<font size= 4 color=\"#806040\">colored</font>text</font>\n\
</b>\n\
\n\
Global addition parameters placed in the first line of the META file  (MUXOPT).\n\
All parameters in this group started with two dashes:\n\
\n\
--pcr-on-video-pid  Do not allocate separate PID for PCR, use an existing video\n\
                    PID.\n\
--new-audio-pes     Use bytes 0xfd instead of 0xbd for AC3, True-HD, DTS and\n\
                    DTS-HD. Parameter is auto activated for BD muxing.\n\
--vbr               Use variable bitrate.\n\
--minbitrate        Sets the lower limit of the vbr bitrate.  If the stream has\n\
                    a  smaller bitrate  then NULL  packets will be inserted  to\n\
                    hold the limit.\n\
--maxbitrate        The upper limit of the vbr bitrate.\n\
--cbr               Muxing mode  with a fixed bitrate.  Options --vbr and --cbr\n\
                    should not be used together.\n\
--vbv-len           The  length  of the  virtual  buffer  in milliseconds.  The\n\
                    default value  is 500.  Typically, this  option  is used in\n\
                    together with --cbr. The parameter is similar to  the value\n\
                    of  vbv-buffer-size  in  the  x264  coder,  but  defined in\n\
                    milliseconds instead of kbit.\n\
--no-asyncio        Do not  create  a separate thread  for writing.  Also, this\n\
                    option  disable  flag  FILE_FLAG_NO_BUFFERING  for writing.\n\
                    Deprecated option.\n\
--auto-chapters     Number.  Insert a chapter every <nn> minutes. Used only for\n\
                    BD/AVCHD mode.\n\
--custom-chapters   A semicolon delimited list of string in format hh:mm:ss.zzz\n\
--demux             In this mode selected audio  and video tracks are stored as\n\
                    separate files instead of muxing. utput name must be folder\n\
                    name.  All selected  effects  (such as change  of level for\n\
                    h264) are processed.  When demux,  certain types  of tracks\n\
                    always get changed on storing into a file:\n\
                    - Subtitles in a Presentation Graphic Stream  are converted\n\
                      into sup format.\n\
                    - PCM audio are saved as WAV files.\n\
--blu-ray           Mux to BD diks. If output file name is folder,  bluray disk\n\
                    is created as folder on HDD.  For BD3D disks ssif files are\n\
                    not  created at  this  case.  If output file name  has .iso\n\
                    extension, then BD disk is created as image file.\n\
--avchd             Mux to AVCHD disk.\n\
--cut-start         Trim the beginning of the file.  Value should be  completed\n\
                    with  \"ms\"  (the number of milliseconds),  \"s\" (seconds) or\n\
                    \"min\" (minutes).\n\
--cut-end           Trim  the end of the file.  Value should be  completed with\n\
                    \"ms\" (the number of milliseconds), \"s\" (seconds) or \"min\"\n\
                    (minutes).\n\
--split-duration    Split output to several files.The time specified in seconds\n\
--split-size        Split  output to several files.  Values  should be  written\n\
                    using one of the following postfix: Kb,kib, mb,mib, gb,gib.\n\
--right-eye         Use base video stream for right eye. Used for 3DBD only.\n\
--start-time        Timestamp of the first video frame. May be defined as 45Khz\n\
                    clock (just a number) or as time in format hh:mm:ss.zzz\n\
--mplsOffset        The number of the first MPLS file.  Used for  BD disk mode.\n\
--m2tsOffset        The number of the first M2TS file.  Used for  BD disk mode.\n\
--insertBlankPL     Add extra  short playlist.  Used for cropped video muxed to\n\
                    BD disk.\n\
--blankOffset       Blank playlist number.\n\
--label             Disk label for muxing to ISO file.\n\
--extra-iso-space   Allocate extra space  in 64K units  for ISO  disk  metadata\n\
                    (file and directory names). Normally, tsMuxeR allocate this\n\
                    space automatically. But if split condition generates a lot\n\
                    of small files, extra ISO space may be required to define.\n\
";
    LTRACE(LT_INFO, 2, help);
}

int main(int argc, char** argv)
{	
	LTRACE(LT_INFO, 2, "tsMuxeR version " << APP_VERSION << ". github.com/justdan96/tsMuxer");
	int firstMplsOffset = 0;    
	int firstM2tsOffset = 0;
	int blankNum = 1900;
	bool insertBlankPL = false;
	//createBluRayDirs("c:/workshop/");

    //MPLSParser parser;
    //parser.parse("d:/hdtv/SHERLOCK_HOLMES/BDMV/PLAYLIST/00100.mpls");

	//CLPIParser parser;
	//parser.parse("h:/BDMV/CLIPINF/00000.clpi");
    //parser.parse("d:/workshop/test_orig_disk2/BDMV/CLIPINF/00003.clpi");

    //uint8_t moBuffer[1024];
    //MovieObject mo;
    //mo.parse("h:/BDMV/MovieObject.bdmv");
    //int moLen = mo.compose(moBuffer, sizeof(moBuffer));
    //file.write(moBuffer, moLen);


	try {
		if (argc == 2) 
		{
			string str = argv[1];
			string fileExt = extractFileExt(str);
			fileExt = strToLowerCase(fileExt);
			if (fileExt == "mpls" || fileExt == "mpl")
			{
				bool shortExt = fileExt == "mpl";
				MPLSParser mplsParser;
				mplsParser.parse(argv[1]);
				string streamDir = getBlurayStreamDir(argv[1]);
                std::string mediaExt = shortExt ? ".MTS" : ".m2ts";
                std::string ssifExt = shortExt ? ".SIF" : ".ssif";
                bool mode3D = mplsParser.isDependStreamExist;
                bool switchToSsif = false;
				if (mplsParser.m_playItems.size() > 0)
				{
					MPLSPlayItem& item = mplsParser.m_playItems[0];
                    string itemName = streamDir + item.fileName + mediaExt;
                    if (fileExists(itemName)) 
                    {
                        if (mode3D && !mplsParser.m_mvcFiles.empty())
                        {
                            string subItemName = streamDir + mplsParser.m_mvcFiles[0] + mediaExt;
                            if (fileExists(subItemName))
                                detectStreamReader(subItemName.c_str(), &mplsParser, true);
                            else
                                switchToSsif = true;
                        }
                    }
                    else  {
                        switchToSsif = true;
                    }
                    if (switchToSsif)
                    {
                        string ssifName = streamDir + string("SSIF") + getDirSeparator() + item.fileName + ssifExt;
                        if (fileExists(ssifName))
                            itemName = ssifName; // if m2ts file absent then swith to ssif
                    }
                    detectStreamReader(itemName.c_str(), &mplsParser, false);
				}
				
				int markIndex = 0;
				int64_t prevFileOffset = 0;
				for (int i = 0; i < mplsParser.m_playItems.size(); i++) 
                {
					MPLSPlayItem& item = mplsParser.m_playItems[i];

                    string itemName;
                    if (mode3D)
                        itemName = streamDir + string("SSIF") + getDirSeparator() + item.fileName + ".ssif";
                    else
                        itemName = streamDir + item.fileName + mediaExt; // 2d mode

                    LTRACE(LT_INFO, 2, "");

#ifdef WIN32
					char buffer[1024*16];
					CharToOemA(itemName.c_str(), buffer);
					LTRACE(LT_INFO, 2, "File #" << strPadLeft(int32ToStr(i),5,'0') << " name=" << buffer);
#else
					LTRACE(LT_INFO, 2, "File #" << strPadLeft(int32ToStr(i),5,'0') << " name=" << itemName);
#endif
					LTRACE(LT_INFO, 2, "Duration: " << floatToTime((mplsParser.m_playItems[i].OUT_time - mplsParser.m_playItems[i].IN_time)/ (double) 45000.0));
                    if (mplsParser.isDependStreamExist) {
                        if (mplsParser.mvc_base_view_r) {
                            LTRACE(LT_INFO, 2, "Base view: right-eye");
                        }
                        else {
                            LTRACE(LT_INFO, 2, "Base view: left-eye");
                        }
                    }
                    if (!mplsParser.m_playItems.empty())
                        LTRACE(LT_INFO, 2, "start-time: " << mplsParser.m_playItems[0].IN_time);
					int marksPerFile = 0;
					for (;markIndex < mplsParser.m_marks.size(); markIndex++) 
					{
						PlayListMark& curMark = mplsParser.m_marks[markIndex];
						if (curMark.m_playItemID > i)
							break;
						uint64_t time = curMark.m_markTime - mplsParser.m_playItems[i].IN_time + prevFileOffset;
						if (marksPerFile % 5 == 0) {
							if (marksPerFile > 0)
								LTRACE(LT_INFO, 2, "");
							LTRACE2(LT_INFO, "Marks: ");
						}
						marksPerFile++;
						LTRACE2(LT_INFO, floatToTime(time/45000.0) << " ");
					}
					if (marksPerFile > 0)
						LTRACE(LT_INFO, 2, "");
					prevFileOffset += mplsParser.m_playItems[i].OUT_time - mplsParser.m_playItems[i].IN_time;
				}
			}
			else
				detectStreamReader(argv[1], 0, false);
			cout << endl;
			return 0;
		}
		if (argc != 3) {
            /*
			LTRACE(LT_INFO, 2, "Usage: ");
			LTRACE(LT_INFO, 2, "For start muxing: " << "tsMuxeR <meta file name> <out file/dir name>");
			LTRACE(LT_INFO, 2, "For detect stream params: " << "tsMuxeR <media file name>");
			LTRACE(LT_INFO, 2, "For more information about meta file see readme.txt");
			cout << endl;
            */
            showHelp();
			return -1;
		}
		string fileExt = extractFileExt(argv[2]);
		fileExt = strToUpperCase(fileExt);
		uint64_t startTime = mtime::clockGetTimeEx();

		int autoChapterLen = 0;
		vector<double> customChapterList;
        bool stereoMode = false;
        string isoDiskLabel;
		DiskType dt = checkBluRayMux(argv[1], autoChapterLen, customChapterList, firstMplsOffset, firstM2tsOffset, insertBlankPL, blankNum, stereoMode, isoDiskLabel);
        std::string fileExt2 = unquoteStr(fileExt);
		bool muxMode = fileExt2 == "M2TS" || fileExt2 == "TS" || fileExt2 == "SSIF" || fileExt2 == "ISO" || dt != DT_NONE;

		if (muxMode)
		{
            BlurayHelper blurayHelper;

			MuxerManager muxerManager(readManager, tsMuxerFactory);
            muxerManager.setAllowStereoMux(fileExt2 == "SSIF" || dt != DT_NONE);
			muxerManager.openMetaFile(argv[1]);
			string dstFile = unquoteStr(argv[2]);

			if (dt != DT_NONE)
			{
                if (!blurayHelper.open(dstFile, dt, muxerManager.totalSize(), muxerManager.getExtraISOBlocks()))
                    throw runtime_error(string("Can't create output file ") + dstFile);
                blurayHelper.setVolumeLabel(isoDiskLabel);
				blurayHelper.createBluRayDirs();
				blurayHelper.writeBluRayFiles(insertBlankPL, firstMplsOffset, blankNum, stereoMode);
				dstFile = blurayHelper.m2tsFileName(firstM2tsOffset);
			}
			if (muxerManager.getTrackCnt() == 0)
				THROW(ERR_COMMON, "No tracks selected");
            muxerManager.doMux(dstFile, dt != DT_NONE ? &blurayHelper : 0);
			if (dt != DT_NONE) 
			{
                TSMuxer* mainMuxer = dynamic_cast<TSMuxer*> (muxerManager.getMainMuxer());
                TSMuxer* subMuxer = dynamic_cast<TSMuxer*> (muxerManager.getSubMuxer());

                if (mainMuxer)
				    blurayHelper.createCLPIFile(mainMuxer, mainMuxer->getFirstFileNum(), true);
                if (subMuxer) {
                    blurayHelper.createCLPIFile(subMuxer, subMuxer->getFirstFileNum(), false);

                    IsoWriter* IsoWriter = blurayHelper.isoWriter();
                    if (IsoWriter) {
                        for (int i = 0; i < mainMuxer->splitFileCnt(); ++i) {
                            string file1 = mainMuxer->getFileNameByIdx(i);
                            string file2 = subMuxer->getFileNameByIdx(i);
                            int ssifNum = strToInt32(extractFileName(file1));
                            if (!file1.empty() && !file2.empty())
                                IsoWriter->createInterleavedFile(file1, file2, blurayHelper.ssifFileName(ssifNum));
                        }
                    }
                }

                for (int i = 0; i < customChapterList.size(); ++i)
                    customChapterList[i] -= (double)muxerManager.getCutStart()/1e9;
                //createMPLSFile(dstDir, mainMuxer->getPidList(), *(mainMuxer->getFirstPts().begin()), *(mainMuxer->getLastPts().rbegin()),
                //    autoChapterLen, customChapterList, dt, firstMplsOffset, firstM2tsOffset);

                // allign last PTS between main and sub muxers

                if (subMuxer)
                    mainMuxer->alignPTS(subMuxer);

                blurayHelper.createMPLSFile(mainMuxer, subMuxer, autoChapterLen, customChapterList, dt, firstMplsOffset, muxerManager.isMvcBaseViewR());


				if (insertBlankPL && mainMuxer && !subMuxer) {
					LTRACE(LT_INFO, 2, "Adding blank play list");
					muxBlankPL(extractFileDir(argv[0]), blurayHelper, mainMuxer->getPidList(), dt, blankNum);
				}
			}

			LTRACE(LT_INFO, 2, "Mux successful complete");
		}
		else {
			MuxerManager sMuxer(readManager, singleFileMuxerFactory);
			sMuxer.openMetaFile(argv[1]);
			if (sMuxer.getTrackCnt() == 0)
				THROW(ERR_COMMON, "No tracks selected");
      	    createDir(unquoteStr(argv[2]), true);
			sMuxer.doMux(unquoteStr(argv[2]), 0);
			LTRACE(LT_INFO, 2, "Demux complete.");
		}
		uint64_t endTime = mtime::clockGetTimeEx();
		int seconds = (double)(endTime - startTime)/1e6 + 0.5;
		int minutes = seconds/60;
		if (muxMode) {
			LTRACE2(LT_INFO, "Muxing time: ");
		}
		else
			LTRACE2(LT_INFO, "Demuxing time: ");
		if (minutes > 0) {
			LTRACE2(LT_INFO, minutes << " min ");
			seconds -= minutes*60;
		}
		LTRACE(LT_INFO, 2, seconds << " sec");

        return 0;
	} catch(runtime_error& e) {
		if (argc == 2)
			LTRACE2(LT_ERROR, "Error: ");
#ifdef WIN32
		char buffer[1024*16];
		CharToOemA(e.what(), buffer);
		LTRACE(LT_ERROR, 2, buffer);
#else
		LTRACE2(LT_ERROR, e.what());
#endif
		return -1;
	} catch(VodCoreException& e) {
		if (argc == 2)
			LTRACE2(LT_ERROR, "Error: ");
#ifdef WIN32
		char buffer[1024*16];
		CharToOemA(e.m_errStr.c_str(), buffer);
		LTRACE(LT_ERROR, 2, buffer);
#else
		LTRACE(LT_ERROR, 2, e.m_errStr.c_str());
#endif
		return -2;
	} catch(BitStreamException& e) {
		if (argc == 2)
			LTRACE2(LT_ERROR, "Error: ");
		LTRACE(LT_ERROR, 2, "Bitstream exception " << e.what() << EXCEPTION_ERR_MSG);
		return -3;
	}
	catch (...) 
	{
		if (argc == 2)
			LTRACE2(LT_ERROR, "Error: ");
		LTRACE(LT_ERROR, 2, "Unknnown exception" << EXCEPTION_ERR_MSG);
		return -4;
	}
}
