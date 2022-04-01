# tsMuxer : Usage Instructions

## GUI

The simplest thing to do is to use the tsMuxerGUI. A screenshot of that can be seen below:

![tsMuxerGUI_Screenshot](https://user-images.githubusercontent.com/122434/148913872-b7af9caa-c2ca-4892-8853-06fd6329a15a.png)

## Command Line

Alternatively you can use tsMuxer via the command-line. 

Examples:
```
    tsMuxeR <media file name>
    tsMuxeR <meta file name> <out file/dir name>
```

tsMuxeR can be run in track detection mode or muxing mode. If tsMuxeR is run with only one argument, then the program displays track information required to construct a meta file. When running with two arguments, tsMuxeR starts the muxing or demuxing process.

The output of the program is encoded in UTF-8, which means that non-ASCII characters will not show up properly in the Windows console by default. If you want to see the output properly, run `chcp 65001` before running tsMuxeR.

## Meta file format
File MUST have the .meta extension. This file defines files you want to multiplex. The first line of a meta file contains additional parameters that apply to all tracks. In this case the first line should begin with the word MUXOPT.

### Encoding
The file should be encoded with UTF-8. However, since older versions of the GUI saved the file in the "active code page" encoding on Windows, it is used as a fallback on this platform. In the very rare event of the program not being able to open some files referenced by a meta file saved with an older GUI version, please convert it to UTF-8 manually by opening it in Notepad and selecting `ANSI` as the encoding, and then saving it via "Save As", but this time selecting `UTF-8` as the encoding.

### Syntax

The following lines form a list of tracks and their parameters.  The format is as follows: `<code name>,   <file name>,   <parameters>`. Parameters are separated with commas, with each parameter consisting of a name and a value, separated with an equals sign.
Example of META file:
```
MUXOPT --blu-ray
V_MPEG4/ISO/AVC, D:/media/test/stream.h264, fps=25
A_AC3, D:/media/test/stream.ac3, timeshift=-10000ms
```
In this example one AC3 audio stream and one H264 video stream are multiplexed into BD disc. The input file name can reference an elementary stream or a track located inside a container.

Supported input containers:
* TS/M2TS/MTS
* EVO/VOB/MPG/MPEG
* MKV
* MOV/MP4
* MPLS (Blu-ray media play list file)

Names of codecs in the meta file:

Meta File Code    | Description 
---               | --- 
V_MPEGI/ISO/VVC   | H.266/VVC 
V_MPEGH/ISO/HEVC  | H.265/HEVC 
V_MPEG4/ISO/AVC   | H.264/AVC 
V_MPEG4/ISO/MVC   | H.264/MVC 
V_MS/VFW/WVC1     | VC1 
V_MPEG-2          | MPEG2 
A_AC3             | AC3/AC3+/TRUE-HD 
A_AAC             | AAC 
A_DTS             | DTS/DTS-Express/DTS-HD 
A_MP3             | MPEG audio layer 1/2/3 
A_LPCM            | raw pcm data or PCM WAV file 
S_HDMV/PGS        | Presentation graphic stream (BD subtitle format) 
S_TEXT/UTF8       | SRT subtitle format.  Encoding MUST be  UTF-8/UTF-16/UTF-32 

Each track may have additional parameters. Track parameters do not have dashes. If a parameter's value consists of several words, it must be enclosed in quotes.

Common additional parameters for any type of track:

Parameter         | Description 
---               | --- 
track             | track number if input file is a container.
lang              | track language. MUST contain exactly 3 letters. 

Additional parameters for audio tracks:

Parameter         | Description 
---               | --- 
timeshift         | Shift audio track by the given number of milliseconds. Can be negative. 
down-to-dts       | Available only for DTS-HD tracks. Filter out HD part. 
down-to-ac3       | Available only for TRUE-HD tracks. Filter out HD part. 
secondary         | Mux as secondary audio.  Available for DD+ and DTS-Express. 
default           | Mark this track as the default when muxing to Blu-ray.

Additional parameters for video tracks:

Parameter         | Description 
---               | --- 
fps               | The number of frames per second. If not defined, the value is auto detected if available in the source stream. If not, it defaults to 23.976. 
delPulldown       | Remove pulldown from the track, if it exists. If the pulldown is present, the FPS value is changed from 30 to 24. 
ar                | Override video aspect ratio. 16:9, 4:3 e.t.c. 

Additional parameters for H.264 video tracks:

Parameter         | Description 
---               | --- 
level             | Overwrite the level in the H264 stream. Do note that this option only updates the headers and does not reencode the stream, which may not meet the requirements for a lower  level. 
insertSEI         | If the original stream does not contain SEI picture timing, SEI buffering period or VUI parameters, add this data to the stream. This option is recommended for BD muxing. 
forceSEI          | Add SEI picture timing, buffering period and VUI parameters to the stream and rebuild this data if it already exists. 
contSPS           | If the original video doesn't contain repetitive SPS/PPS, then SPS/PPS will be added to the stream before each key frame. This option is recommended for BD muxing. 
subTrack          | Used for combined AVC/MVC tracks only. TsMuxeR always demultiplexes such tracks to separate AVC and MVC streams. Setting this to 1 sets the reference to the AVC part, while 2 sets it to the MVC part. 
secondary         | Mux as secondary video (PIP). 
pipCorner         | Corner for PIP video. Allowed values: "TopLeft","TopRight", "BottomRight", "BottomLeft". 
pipHOffset        | PIP window horizontal offset from the corner in pixels. 
pipVOffset        | PIP window vertical offset from the corner in pixels. 
pipScale          | PIP window scale factor. Allowed values: "1", "1/2", "1/4", "1.5", "fullScreen". 
pipLumma          | Allow the PIP window to be transparent. Transparent colors are lumma colors in range [0..pipLumma].

Additional parameters for PG and SRT tracks:

Parameter         | Description 
---               | ---
video-width       | The width of the video in pixels. 
video-height      | The height of the video in pixels. 
default           | Mark this track as the default when muxing to Blu-ray. Allowed values are `all` which causes all subtitles to be shown, and `forced` which shows only elements marked as "forced" in the subtitle stream.
fps               | Video fps. It is recommended to define this parameter in order to enable more careful timing processing. 
3d-plane          | Defines the number of the '3D offset track' which is placed inside the MVC track. Each message has an individual 3D offset. This information is stored inside 3D offset track.

Additional parameters for SRT tracks:

Parameter         | Description 
---               | --- 
font-name         | Font name to render. 
font-color        | Font color, defined as a hexadecimal or decimal number. 24-bit long numbers (for instance 0xFF00FF) define RGB components, while 32-bit long ones (for instance 0x80FF00FF) define ARGB components. 
font-size         | Font size in pixels. 
font-italic       | Italic display text. 
font-bold         | Bold display text. 
font-underline    | Underlined text. 
font-strikeout    | Strikethrough text. 
bottom-offset     | Distance from the lower edge while displaying text. 
font-border       | Outline width. 
fadein-time       | Time in ms for smooth subtitle appearance. 
fadeout-time      | Time in ms for smooth subtitle disappearance. 
line-spacing      | Interval between subtitle lines. Default value is 1.0.

Currently tsMuxer only supports fonts in TTF format. It also will only load fonts from `/usr/share/fonts/` on Linux and `/Library/Fonts/` on Mac. As such our recommendation is to use font "FreeSans" on Linux and "OpenSans" on Mac.

tsMuxeR supports additional tags inside SRT tracks.  The syntax  and parameters coincide with HTML: `<b>, <i>, <u>, <strike>, <font>`. Default relative font size (used in these tags) is 3.  For example:
```
<b><font size=5 color="deepskyblue" name="Arial"><u>Test</u>
<font size= 4 color="#806040">colored</font>text</font>
</b>
```

Global additional parameters are placed in the first line of the META file, which must begin with the MUXOPT token. All parameters in this group start with two dashes:

Parameter           | Description 
---                 | --- 
--pcr-on-video-pid  | Do not allocate a separate PID for PCR and use the existing video PID. 
--new-audio-pes     | Use bytes 0xfd instead of 0xbd for AC3, True-HD, DTS and DTS-HD. Activated automatically for BD muxing. 
--vbr               | Use variable bitrate.
--minbitrate        | Sets the lower limit of the VBR bitrate. If the stream has a smaller bitrate, NULL packets will be inserted to compensate. 
--maxbitrate        | The upper limit of the vbr bitrate.
--cbr               | Muxing mode with a fixed bitrate. --vbr and --cbr must not be used together. 
--vbv-len           | The  length  of the  virtual  buffer  in milliseconds.  The default value  is 500.  Typically, this  option  is used together with --cbr. The parameter is similar to  the value of  vbv-buffer-size  in  the  x264  codec,  but  defined in milliseconds instead of kbit. 
--no-asyncio        | Do not  create  a separate thread  for writing. This option also disables the FILE_FLAG_NO_BUFFERING flag on Windows when writing. This option is deprecated. 
--auto-chapters     | Insert a chapter every <n> minutes. Used only in BD/AVCHD mode. 
--custom-chapters   | A semicolon delimited list of hh:mm:ss.zzz strings, representing the chapters' start times. 
--demux             | Run in demux mode : the selected audio and video tracks are stored as separate files. The output name must be a folder name. All selected effects (such as changing the level of a H264 stream) are processed. When demuxing, certain types of tracks are always changed : - Subtitles in a Presentation Graphic Stream are converted into sup format. - PCM audio is saved as WAV files. 
--blu-ray           | Mux as a BD disc. If the output file name is a folder, a Blu-Ray folder structure is created inside that folder. SSIF files for BD3D discs are not created in this case. If the output name has an .iso extension, then the disc is created directly as an image file. 
--blu-ray-v3        | As above - except mux to UHD BD discs. If you're using the GUI, this will be automatically set if one of the streams is HEVC.
--avchd             | Mux to AVCHD disc.
--cut-start         | Trim the beginning of the file. The value should be followed by the time unit : "ms" (milliseconds), "s" (seconds) or "min" (minutes). 
--cut-end           | Trim the end of the file. Same rules as --cut-start apply. 
--split-duration    | Split the output into several files, with each of them being <n> seconds long. 
--split-size        | Split the output into several files, with each of them having a given maximum size. KB, KiB, MB, MiB, GB and GiB are accepted as size units. 
--right-eye         | Use base video stream for right eye. Used for 3DBD only.
--start-time        | Timestamp of the first video frame. May be defined as 45Khz clock (just a number) or as time in hh:mm:ss.zzz format
--mplsOffset        | The number of the first MPLS file. Used for BD disc mode.
--m2tsOffset        | The number of the first M2TS file. Used for BD disc mode.
--insertBlankPL     | Add an additional short playlist.  Used for cropped video muxed to BD disc.
--blankOffset       | Blank playlist number.
--label             | Disk label when muxing to ISO.
--extra-iso-space   | Allocate extra space in 64K units for ISO metadata (file and directory names). Normally, tsMuxeR allocates this space automatically, but if split condition generates a lot of small files, it may be required to define extra space.
--constant-iso-hdr  | Generates an ISO header that does not depend on the program version or the current time. Normally, the ISO header's "application ID", "implementation ID", and "volume ID" fields are set to strings containing the program version and/or a random number, while the access/modification/creation times of the files in the image are set to the current time. This option disables this behaviour by filling these fields with hardcoded values and setting the file times to the equivalent of `Wed 1 Jul 20:00:00 UTC 2020` in the local timezone. Using this option is not recommended for normal usage, as it is meant only for testing ISO output validity.
