# tsMuxer

## Vision

This project is for tsMuxer - a transport stream muxer for remuxing/muxing elementary streams. This is very useful for transcoding and this project is used in other products such as Universal Media Server.

EVO/VOB/MPG, MKV/MKA, MP4/MOV, TS, M2TS to TS to M2TS.

Supported video codecs H.264/AVC, H.265/HEVC, VC-1, MPEG2. 
Supported audio codecs AAC, AC3 / E-AC3(DD+), DTS/ DTS-HD. 

Some of the major features include:

* Ability to set muxing fps manually and automatically
* Ability to change level for H.264 streams
* Ability to shift a sound tracks
* Ability to extract DTS core from DTS-HD
* Ability to join files
* Output/Author to compliant Blu-ray Disc or AVCHD
* Blu-ray 3D support

## Ethics

This project operates under the W3C's
[Code of Ethics and Professional Conduct](https://www.w3.org/Consortium/cepc):

> W3C is a growing and global community where participants choose to work
> together, and in that process experience differences in language, location,
> nationality, and experience. In such a diverse environment, misunderstandings
> and disagreements happen, which in most cases can be resolved informally. In
> rare cases, however, behavior can intimidate, harass, or otherwise disrupt one
> or more people in the community, which W3C will not tolerate.
>
> A Code of Ethics and Professional Conduct is useful to define accepted and
> acceptable behaviors and to promote high standards of professional
> practice. It also provides a benchmark for self evaluation and acts as a
> vehicle for better identity of the organization.

We hope that our community group act according to these guidelines, and that
participants hold each other to these high standards. If you have any questions
or are worried that the code isn't being followed, please contact the owner of the repository.


## Language

tsMuxer is written in C++. It can be compiled for Windows, Linux and Mac. 

## History

This project was created by Roman Vasilenko, with the last public release 20th January 2014. It was open sourced on 23rd July 2019, to aid the future development.

## Installation

All executable are created to be portable, so you can just save and extract the compressed package for your platform. 

Nightly builds are created in Bintray, use the link below to go directly to the latest nightly build:
[ ![Download](https://api.bintray.com/packages/justdan96/tsMuxer/tsMuxerGUI-Nightly/images/download.svg) ](https://bintray.com/justdan96/tsMuxer/tsMuxerGUI-Nightly/_latestVersion)

Or browse to the version you want via [this link](https://bintray.com/justdan96/tsMuxer/tsMuxerGUI-Nightly), clicking the link for the version you want.
![bintray_version_list](https://user-images.githubusercontent.com/503722/70852149-f3f37300-1e95-11ea-8fb4-c9a82d698448.png)

Then when you are on the page for your chosen version, click on the "Files" tab:
![bintray_file_link](https://user-images.githubusercontent.com/503722/70852150-f48c0980-1e95-11ea-8bb8-76168fb0c2cb.png)

Finally select the ZIP file for your platform - Linux, Mac, Windows 32-bit or Windows 64-bit:
![bintray_file_list](https://user-images.githubusercontent.com/503722/70852151-f48c0980-1e95-11ea-8914-7d525b614bf3.png)

### Windows

The ZIP file for Windows can just be unzipped and the executables can be used straight away - there are no dependencies.

### Linux

The ZIP file for Linux can just be unzipped and the executables can be used straight away - there are no dependencies.

### MacOS

This ZIP file for MacOS can just be unzipped and the executables can be used after installing a couple of dependencies. To install those run the commands below in the Terminal:
```
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)" < /dev/null 2> /dev/null
brew install freetype
brew install zlib
```

## Usage

### GUI

The simplest thing to do is to use the tsMuxerGUI. A screenshot of that can be seen below:

![tsMuxerGUI_Screenshot](https://user-images.githubusercontent.com/503722/69820496-209e5e00-11f9-11ea-81b2-8c130b6e0e89.png)

### Command Line

Alternatively you can use tsMuxer via the command-line. 

Examples:
```
    tsMuxeR <media file name>
    tsMuxeR <meta file name> <out file/dir name>
```

tsMuxeR can be run in track detection mode or muxing mode. If run tsMuxeR  with only  one argument  then tsMuxeR  display  input track information  required to construct  meta  file.  If run tsMuxeR  with two arguments tsMuxeR start muxing or demuxing process.

### Meta file format
File MUST has extension .meta.  This file  define files you want to  multiplex. First line of meta file contain additional parameters that apply to all tracks. In this case the line should begin with the word MUXOPT.

Following lines indicate a list of tracks  and their parameters.  The format is as follows:   `<code name>,   <file name>,   <parameters>`   Parameters are comma separated. Each parameter indicates the name and value.
Example of META file:
```
MUXOPT --blu-ray
V_MPEG4/ISO/AVC, D:/media/test/stream.h264, fps=25
A_AC3, D:/media/test/stream.ac3, timeshift=-10000ms
```
In this example one AC3 audio stream and one H264 video stream are  multiplexed to BD disk.  Input file name can reference to elementary stream or track inside container.

Supported input containers:
* TS/M2TS/MTS
* EVO/VOB/MPG/MPEG
* MKV
* MOV/MP4
* MPLS (Blu-ray media play list file)

Names of codecs in the meta file:

Meta File Code    | Description 
---               | --- 
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

Each track may has addition parameters.  Track parameters do not  have dash. If parameter value has several words, parameter must be enclosed in quotes.

Common additional parameters for any type of track:

Parameter         | Description 
---               | --- 
track             | track number if input file is container. 
lang              | track language. MUST contains exact 3 letters. 

Additional parameters for audio tracks:

Parameter         | Description 
---               | --- 
timeshift         | Shift audio track to future (positive value) or to past. Measured at milliseconds. 
down-to-dts       | Available only for DTS-HD tracks. Filter out HD part. 
down-to-ac3       | Available only for TRUE-HD tracks. Filter out HD part. 
secondary         | Mux as secondary audio.  Available for DD+ and DTS-Express. 

Additional parameters for video tracks:

Parameter         | Description 
---               | --- 
fps               | Video fps. If not defined, default value auto detected from a source stream if present. If not, default value 23.976. 
delPulldown       | Remove pulldown from the track if exists.  This option lead to fps change from 30 to 24 if pulldown exists. 
ar                | Override video aspect ratio. 16:9, 4:3 e.t.c. 

Additional parameters for H.264 video tracks:

Parameter         | Description 
---               | --- 
level             | Overwrite  level in the H264 stream.  Note:  option  update headers only. The H264 stream may not meet the requirements of a lower level. 
insertSEI         | If original   stream  does not contain  SEI picture timing, SEI buffering period or VUI parameters,  then add this data to the stream. This option is recommended for BD muxing. 
forceSEI          | Add SEI picture timing, buffering period and VUI parameters to the stream. Rebuild data If data already exist. 
contSPS           | If original video doesn't contain  repetitive SPS/PPS  then SPS/PPS will be added to the stream before  each key frame. This option is recommended for BD muxing. 
subTrack          | Used  for  combined  AVC/MVC  tracks  only.  TsMuxeR always demultiplex  such  tracks to separate  AVC and MVC streams. This parameter defined reference to AVC part(if value=1) or to MVC part (if value=2). 
secondary         | Mux as secondary video (PIP). 
pipCorner         | Corner for PIP video. Allowed values: "TopLeft","TopRight", "BottomRight", "BottomLeft". 
pipHOffset        | PIP window horizontal offset from the corner in pixels. 
pipVOffset        | PIP window vertical offset from the corner in pixels. 
pipScale          | PIP window scale factor. Allowed values: "1", "1/2", "1/4", "1.5", "fullScreen". 
pipLumma          | Allow PIP window to be transparent. Transparent colors  are lumma colors in range `[0..pipLumma]`. 

Additional parameters for PG and SRT tracks:

Parameter         | Description 
---               | --- 
video-width       | The width of the video in pixels. 
video-height      | The height of the video in pixels. 
fps               | Video fps.  Recommended  to  define this parameter for more carefully timing processing. 
3d-plane          | Parameter  defines  number  of  the '3D offset track' which placed inside MVC track.  Each message has individual 3D offset. This information stored inside 3D offset track. 

Additional parameters for SRT tracks:

Parameter         | Description 
---               | --- 
font-name         | Font name to render. 
font-color        | Font color. Color can be defined in hexadecimal or  decimal format. If color 24 bit long  (for instance 0xFF00FF)  it's  define RGB components.  IF color 32 bit long  (for instance 0x80FF00FF) it's define ARGB components. 
font-size         | Font size in pixels. 
font-italic       | Italic display text. 
font-bold         | Bold display text. 
font-underline    | Underlined text. 
font-strikeout    | Strikethrough text. 
bottom-offset     | Distance from the lower edge while displaying text. 
font-border       | Outline width. 
fadein-time       | Time in ms for smooth subtitle appearance. 
fadeout-time      | Time in ms for smooth subtitle disappearance. 
line-spacing      | Interval between lines. Default value 1.0. 

tsMuxeR  supports  addition  tag inside  SRT track.  The syntax  and parameters coincide with HTML: `<b>, <i>, <u>, <strike>, <font>`. Default relative font size (used in these tags) - 3.  For example:
```
<b><font size=5 color="deepskyblue" name="Arial"><u>Test</u>
<font size= 4 color="#806040">colored</font>text</font>
</b>
```

Global additional parameters placed in the first line of the META file (MUXOPT). All parameters in this group start with two dashes:

Parameter           | Description 
---                 | --- 
--pcr-on-video-pid  | Do not allocate separate PID for PCR, use an existing video PID.
--new-audio-pes     | Use bytes 0xfd instead of 0xbd for AC3, True-HD, DTS and DTS-HD. Parameter is auto activated for BD muxing.
--vbr               | Use variable bitrate.
--minbitrate        | Sets the lower limit of the vbr bitrate.  If the stream has a  smaller bitrate  then NULL  packets will be inserted  to hold the limit.
--maxbitrate        | The upper limit of the vbr bitrate.
--cbr               | Muxing mode  with a fixed bitrate.  Options --vbr and --cbr should not be used together.
--vbv-len           | The  length  of the  virtual  buffer  in milliseconds.  The default value  is 500.  Typically, this  option  is used in together with --cbr. The parameter is similar to the value of  vbv-buffer-size  in  the  x264  coder,  but  defined in milliseconds instead of kbit.
--no-asyncio        | Do not  create  a separate thread  for writing.  Also, this option  disable  flag  FILE_FLAG_NO_BUFFERING  for writing. Deprecated option.
--auto-chapters     | Number.  Insert a chapter every <nn> minutes. Used only for BD/AVCHD mode.
--custom-chapters   | A semicolon delimited list of string in format hh:mm:ss.zzz
--demux             | In this mode selected audio  and video tracks are stored as separate files instead of muxing. utput name must be folder name.  All selected  effects  (such as change  of level for h264) are processed. When demux,  certain types  of tracks always get changed on storing into a file: - Subtitles in a Presentation Graphic Stream  are converted into sup format. - PCM audio are saved as WAV files.
--blu-ray           | Mux to BD disks. If output file name is folder,  bluray disk is created as folder on HDD.  For BD3D disks ssif files are not  created at  this  case.  If output file name  has .iso extension, then BD disk is created as image file.
--blu-ray-v3        | As above - except mux to UHD BD disks.
--avchd             | Mux to AVCHD disk.
--cut-start         | Trim the beginning of the file.  Value should be  completed with  "ms"  (the number of milliseconds),  "s" (seconds) or "min" (minutes).
--cut-end           | Trim  the end of the file.  Value should be  completed with "ms" (the number of milliseconds), "s" (seconds) or "min" (minutes).
--split-duration    | Split output to several files.The time specified in seconds
--split-size        | Split  output to several files.  Values  should be  written using one of the following postfix: Kb,kib, mb,mib, gb,gib.
--right-eye         | Use base video stream for right eye. Used for 3DBD only.
--start-time        | Timestamp of the first video frame. May be defined as 45Khz clock (just a number) or as time in format hh:mm:ss.zzz
--mplsOffset        | The number of the first MPLS file.  Used for  BD disk mode.
--m2tsOffset        | The number of the first M2TS file.  Used for  BD disk mode.
--insertBlankPL     | Add extra  short playlist.  Used for cropped video muxed to BD disk.
--blankOffset       | Blank playlist number.
--label             | Disk label for muxing to ISO file.
--extra-iso-space   | Allocate extra space  in 64K units  for ISO  disk  metadata (file and directory names). Normally, tsMuxeR allocate this space automatically. But if split condition generates a lot of small files, extra ISO space may be required to define.


## Todo

The following is a list of changes that will need to be made to the original source code and project in general:

* the program doesn't support MPEG-4 ASP, even though MPEG-4 ASP is defined in the TS specification
* no Opus audio support
* [several](https://forum.doom9.org/showthread.php?p=1880216#post1880216) [muxing](https://forum.doom9.org/showthread.php?p=1881372#post1881372) [bugs](https://forum.doom9.org/showthread.php?p=1881509#post1881509) when muxing a HEVC/UHD stream - results in an out-of-sync stream
* has issues with 24-bit DTS Express
* issues with the 3D plane lists when there are mismatches between the MPLS and M2TS

## Contributing

We’re really happy to accept contributions from the community, that’s the main reason why we open-sourced it! There are many ways to contribute, even if you’re not a technical person.

We’re using the infamous [simplified Github workflow](http://scottchacon.com/2011/08/31/github-flow.html) to accept modifications (even internally), basically you’ll have to:

* create an issue related to the problem you want to fix (good for traceability and cross-reference)
* fork the repository
* create a branch (optionally with the reference to the issue in the name)
* hack hack hack
* commit incrementally with readable and detailed commit messages
* submit a pull-request against the master branch of this repository

We’ll take care of tagging your issue with the appropriated labels and answer within a week (hopefully less!) to the problem you encounter.

If you’re not familiar with open-source workflows or our set of technologies, do not hesitate to ask for help! We can mentor you or propose good first bugs (as labeled in our issues). Also welcome to add your name to Credits section of this document.

## Submitting Bugs

You can report issues directly on Github, that would be a really useful contribution given that we lack some user testing on the project. Please document as much as possible the steps to reproduce your problem (even better with screenshots).

## Building

For full details on building tsMuxer for your platform please see the document on [COMPILING](COMPILING.md).

## Testing

In absence of a full test suite currently we are creating some basic tests that can be performed. 

The first test is a simple MKV to M2TS test with the first 2 minutes of Big Buck Bunny. 

You need the file bbb-2mins.mkv and to either use the M2TS option in tsMuxerGUI or use the following meta file:

```
MUXOPT --no-pcr-on-video-pid --new-audio-pes --vbr  --vbv-len=500
V_MPEG-2, "bbb-2mins.mkv", track=1, lang=und
A_AC3, "bbb-2mins.mkv", track=2, lang=und
```

The MD5 sums of the original file and the output file should be:
```
a239a724cba1381a5956b50cb8b46754  bbb-2mins.mkv
62342b056d37e44fae4e48161d6208bf  bbb-2mins-from-linux-meta.m2ts
```

We will have also done the same with the test file [Life Untouched](https://uhdsample.com/6-life-untouched-4k-demo-60fps-hdr10.html). Results of the MD5 sums for the original and output file are below:
```
f2db8f6647f4f2a0b2417aed296fee73  Life Untouched 4K Demo.mp4
aba9ee3a3211cd09ba4833a610faff22  Life Untouched 4K Demo.m2ts
```

We need more sample files with 3D and multiple subtitle tracks if possible so if you have any ways of testing these files (particularly in relation to the bugs in the TODO section) please let us know?

## Financing

We are not currently accepting any kind of donations and we do not have a bounty program.

## Versioning

Version numbering follows the [Semantic versioning](http://semver.org/) approach.

## License

We’re using the Apache 2.0 license for simplicity and flexibility. You are free to use it in your own project.

## Credits

* Roman Vasilenko - for creating tsMuxer
