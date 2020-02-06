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

tsMuxeR can be run in track detection mode or muxing mode. If tsMuxeR is run with only one argument, then the program displays track information required to construct a meta file. When running with two arguments, tsMuxeR starts the muxing or demuxing process.

### Meta file format
File MUST have the .meta extension. This file defines files you want to multiplex. The first line of a meta file contains additional parameters that apply to all tracks. In this case the first line should begin with the word MUXOPT.

#### Encoding
The file should be encoded with UTF-8. However, since older versions of the GUI saved the file in the "active code page" encoding on Windows, it is used as a fallback on this platform. In the very rare event of the program not being able to open some files referenced by a meta file saved with an older GUI version, please convert it to UTF-8 manually by opening it in Notepad and selecting `ANSI` as the encoding, and then saving it via "Save As", but this time selecting `UTF-8` as the encoding.

#### Syntax

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
pipLumma          | Allow the PIP window to be transparent. Transparent colors are lumma colors in range [0..pipLumma].  Additional parameters for PG and SRT tracks:

Parameter         | Description 
---               | --- 
video-width       | The width of the video in pixels. 
video-height      | The height of the video in pixels. 
fps               | Video fps. It is recommended to define this parameter in order to enable more careful timing processing. 
3d-plane          | Defines the number of the '3D offset track' which is placed inside the MVC track. Each message has an individual 3D offset. This information is stored inside 3D offset track. Additional parameters for SRT tracks:

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
--blu-ray-v3        | As above - except mux to UHD BD discs.
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

All pull requests must pass code style checks which are executed with `clang-format` version 9. Therefore, it is advised to install an appropriate commit hook (for example [this one](https://github.com/barisione/clang-format-hooks)) to your local repository in order to commit properly formatted code right away.

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
