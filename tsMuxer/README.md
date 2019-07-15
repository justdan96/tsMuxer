# tsMuxeR

## How to use tsMuxeR from the command line:

We need to create a file with the extension .meta. This file lists the files you want to multiplex.
In the first line of meta file can be specified additional parameters that apply to all tracks. In this case the line should begin with the word MUXOPT.

The following lines indicate a list of tracks and their parameters.
The format is as follows:

```
<code name>, <file name>, <parameters>
```

Parameters are comma separated. Each parameter indicates the name and value.
Example META file:

```
V_MPEG4/ISO/AVC, D:\media\test\stream.h264, fps=25
A_AC3, D:\media\test\stream.ac3, timeshift=-10000ms
```

In this example one AC3 audio stream and one H264 video stream are multiplexed.


## Additional parameters of audio and video tracks:

* fps - For video and subtitle tracks you can define the fps (see example above). If fps is not specified, it is determined from the stream.

* level - Allows you to overwrite the field level in the H264 stream. For example, you can change the profile High@5.1 to High@4.1.
Note that it only updates the header. The H264 stream may not meet the requirements of a lower level.

* insertSEI - Parameter is used only for H.264 video. When activated, it does the following: if the original video does not contain SEI picture timing and SEI buffering period, then the info is added to the stream. This option is recommended for better compatibility with the Sony Playstation 3.

* contSPS - Parameter is used only for H.264 video. When enabled, and the original video doesn't contain cyclic repetitive elements SPS/PPS (when imported from MKV it can be recorded only one time at the beginning of the file), the SPS/PPS will be added to the stream before each key frame. We recommend that you always enable this option.

Note: The video player Dune HD can not decode a repeated SPS component in x264 streams, perhaps this is an error in the current firmware.

* delPulldown - For video streams. Deletes pulldown flags of the track. Attention! When using delPulldown usually a new value fps is required, different from values in the stream. For example, if the stream has fps=29.97, after setting delPulldown you have to input fps=23.976.

* timeshift - For audio and subtitle tracks supported. Setting of timeshift may be more or less than zero.
Values in milliseconds (ms at the end) or in seconds (s at the end). This option allows audio track in time to move forward (positive value) or backward.

* down-to-dts - Available only to DTS-HD tracks. Makes conversion DTS-HD into standard DTS.

* down-to-ac3 - Is only available for TRUE-HD tracks with AC3 inside the nucleus (usually written on Blu-ray discs).

* track - Starting with version 0.9.96 a reference to the track behind other containers. In this case, you must indicate the track number inside the container.

* mplsFile - A reference to the MPLS file, where the media file is the current track. This information allows a more accurate file joining of the append operation. The option is only available for tracks that are inside M2TS files. The value indicates the MPLS file number.
For example: mplsFile=00048
The file is searched on media in subdirectories ./../PLAYLIST and ./../BACKUP/PLAYLIST

When you combine a large number of files of a Blu-ray Disc (10 and more), then this option is recommended. If you do not enable this option and the Blu-ray disc uses a playlist with connection_condition = 5, at every junction will accumulate desync of audio/subtitle to video about 17ms (from 0 to the length of the audio frame of current track).
This option is automatically filled in the GUI when you open a MPLS file.


## Settings for text subtitles SRT:

* video-width - The width of the video in pixels
* video-height - The height of the video in pixels
* fps - Frames per second of video
* bottom-offset - Depart from the lower edge while displaying text.
* font-name - The name of the font used in quotes
* font-color - Font color, for example, 0x00FFFFFF. Color can be defined in hexadecimal or decimal form.
* font-size - The font size
* font-italic - Italic display text
* font-bold - Bold display text
* font-underline - Underlined text
* font-strikeout - Crossed text

In SRT subs also supported by the following text tags, the syntax and parameters which coincide with HTML:
`<b>, <i>, <u>, <strike>, <font>`. The font size default 3 (size of font-size in dpi).

For example:

```
<b><font size=5 color="deepskyblue" name="Arial"><u>Test</u>
<font size= 4 color="#806040">colored</font>text</font>
</b>
```
Supported containers:
* TS/M2TS/MTS
* EVO / VOB / MPG
* MKV
* MPLS (Blu-ray media play list file)

For a track number, run: tsMuxer <container file name>.
To start multiplexing open Windows Terminal, Far or another file manager and type:

```
tsMuxer <meta name of the file> <TS file name>
```

Names of codecs in the meta file:
* V_MPEG4/ISO/AVC - H264
* V_MS/VFW/WVC1 - VC1
* V_MPEG-2 - MPEG2
* A_AC3 - DD (AC3) / DD (E-AC3) / True HD (True HD only tracks with AC3 core inside).
* A_AAC - AAC
* A_DTS - DTS / DTS-HD
* A_MP3 - MPEG audio layer 1/2/3
* A_LPCM - raw pcm data or PCM WAVE file
* S_HDMV / PGS - subtitle format presentation graphic stream.
* S_TEXT/UTF8 - subtitle format SRT. The text file should be in unicode. Any formats: UTF-8, UTF-16 (little-endian, big-endian), UTF-32 (little-endian, big-endian).


## Options of tsMuxeR in cmd line MUXOPT

The parameters of this group force the entire flow as a whole, rather than on a separate track. Options are separated by space.

--pcr-on-video-pid - Do not provide a separate PID for PCR, use an existing video PID.

--new-audio-pes - Use bytes 0xfd instead of 0xbd to track AC3, True-HD, DTS and DTS-HD. This is in line with standard Blu-ray.

--vbr - Use variable bitrate

--minbitrate=xxxx - Sets the lower limit vbr bitrate. If the stream delivers a smaller bit rate then NULL packets will be inserted to hold the limit.

--maxbitrate = xxxx - the upper limit vbr bitrate.

--cbr - Muxing mode with a fixed bitrate. Options --vbr and --cbr should not be used together.

--vbv-len - The length of the virtual buffer in milliseconds. The default value is 500. Typically, this option is used in conjunction with --cbr.
The parameter is similar to the value of vbv-buffer-size in the x264 coder, but is set not in kbit, but in milliseconds (with constant bitrate they can be counted at each other). If you have self-encoded a x264 file with constant bitrate, for more smooth broadcasting to the network you are encouraged to make the same (or less) setting than in the x264. On virtual buffer overflow relevant errors are written in the log.

--bitrate=xxxx - Bitrate for fixed bitrate muxing mode.
Values --maxbitrate, --minbitrate and --bitrate are indicated in kilobits per second. You can use rational numbers separated by a point.
For example: --maxbitrate=19423.432

--no-asyncio - Not to create a separate thread to write output files. This mode only sets the flag FILE_FLAG_NO_BUFFERING. This somewhat reduces the record speed, but allows you to see the amount of output file as you work.

--auto-chapters = nn - Insert a chapter every nn minutes. Only in the blu-ray muxing.

--custom-chapters = <line parameters> - Insert a chapter in the field. Only in the blu-ray muxing. A string of parameters is as follows: hh: mm: ss; hh: mm: ss, etc. Semicolon listed timestamps, semicolon separated list of time stamps a new chapter to define. The line should not contain spaces.

--demux - In this mode selected audio and video tracks are stored as separate files. The processing paths are superimposed. All selected effects (such as change of level for h264) are processed. When demux, certain types of tracks always get changed on storing into a file:
- Subtitles in a Presentation Graphic Stream are converted into format sup
- PCM audio are saved as WAV files. Also automatically splitting into several files, if the size of WAV file exceeds 4Gb.

--cut-start - Trim the beginning of the file. Value should be completed "ms" (the number of milliseconds), "s" (seconds) or "min" (minutes).

--cut-end - Trim the end of the file. Value should be completed "ms" (the number of milliseconds), "s" (seconds) or "min" (minutes)

--split-duration - Cut off in time. The time specified in seconds. The length of each part will always be a little less than imposed values because gap file processes only key frames (or limits of audioframes if there is no video). It is controlled that the next valid gap should not exceed a specified value.

--split-size - To cut output file size. The length of each part will always be a little less than imposed values.
Values should be written using one of the following prefixes:
* Kb - meaning specified in kilobytes (1000 bytes).
* Kib - the value specified in units of 2^10 bytes.
* mb - the value specified in megabytes (1000000 bytes).
* mib - the value specified in units of 2^20 bytes.
* gb - the value stated in gigabytes (1000000000 bytes).
* gib - the value specified in units of 2^30 bytes.

tsMuxer does not use external filters (codecs). 

## Credits

* Roman Vasilenko - for creating this document
* Frank from Doom9 - for translating to English