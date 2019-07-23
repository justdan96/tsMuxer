## tsMuxeR 2.6.15
- Fixed mkv parser a bit. I've got unparsed file example

## tsMuxeR 2.6.13
- update SEI correction: do not correct SPS/PPS if stream contains different PPS with same pps_id

## tsMuxeR 2.6.12
- several minor bugs fixed

## tsMuxeR 2.6.11
- fixed saving UI settings to a registry. Also, if file tsMuxerGUI.ini found, UI will switch settings to an ini file instead of registry
  (you can create empty ini file at the beginning).
- UI: change control for cut start/end time
- fixed SEI processing for 'force' mode ( it doesn't work correctly for some movies)
- fixed bug in the wav demuxer (first audio frame has mixed up channels)
- fixed timings for PG streams. Timings was inaccurate for amount of several ms (for some movies only, it depended of the first PTS of the file)

## tsMuxeR 2.6.9
- inserting SEI did not work for some H.264 stream at all
- add more correction for VUI parameters if option insert SEI is active (it helps to open some H.264 streams in the Scenarins
  and solve PS3 problem for some sources)
- fixed channels for 7.1 and 7.0 wav files
- fixed combined H.264 streams read from Elementary Stream
- BD Bitrate control improved a little bit

## tsMuxeR 2.6.4
- Add secondary video support
- fixed mp4 files with MPEG-DASH
- fixed SEI again
- fixed DTS-ES recognition
- fixed font renderer (a little bit wrong text position)
- several minor improvments and bug fixes

## tsMuxeR 2.5.7
- fixed bug with SEI messages for some movie
- fixed problem with some movies where problem occured during processing several last video frames
- several minor bug fixes

## tsMuxeR 2.5.5
- add HEVC video codec support
- UI improvment: Save settings for General tab, Subtitles tab and last output folder
- Fixed file duration detection for ssif and some m2ts files
- Fixed bug if mux playlist and several sup files (it is a very olg bug, but it became much more often since 2.4.x)
- Several minor bug fixes

## tsMuxeR 2.4.0
- Add secondary audio support for bluray muxing. Due to standart It is allowed only for DTS-Express and DD+ codecs.
- Filter out H.264 filler packets
- UI improvment: option for MPLS offset can be entered either as time or as 45Khz clock value
- UI improvment: UI displays opened file duration
- UI improvment: chapter list correctly updated if join several files. Also joining for MPLS is enabled.
- Add help if run tsMuxeR without parameters
- Fixed muxing for 96Khz TRUE-HD tracks
- PCM inside VOB was anonced before, but actually did not work. Fixed.
- UI fix: if open MPLS, then close, track list is not cleared. It is broken in previous build only.
- Subtitles renderer fixed (broken in previous build only after in/out effects)

## tsMuxeR 2.3.2
- Support PG subtitles inside MKV
- Support MKV tracks with zlib compression
- Support 3D MP4 and MOV files (combined AVC+MVC stream)
- Add option 'line spacing' to subtitles renderer
- Add fade in/out effect to subtitles renderer
- Fixed ability to drag&drop files directly to tsMuxerGUI shurtcut (it worked before in version 10.6)
- Fixed splitting operation if no video track present
- bug fixed: tsMuxeR can't create output directory for UNC path (for instance \\.\Volume{E5FB13D8-5096-11E3-B9C4-005056C00008}\folder1\test.ts)
- bug fixed: message "file already exist" appeared if open several files from a folder with '(' in the name

## tsMuxeR 2.2.3
- Add support for DTS-HD elementary stream with extra DTSHD headers
- Add support for mkv with 'Header Stripping' compression
- Add 3D MKV support
- Add PCM inside MKV support
- Add PCM inside VOB support
- Fixed option 'bind to video fps' for subtitles
- Improved font renderer quality
- Fixed file splitting option (it was disabled since v.1.11.x because of was not implemented for ISO and 3D-blurays)
- Several minor bug fixes

## tsMuxeR 2.1.8
- Fixed join files problem with True-HD track
- introduce MAC build

## tsMuxeR 2.1.6
- Add support for combined AVC+MVC streams
- Output file size slightly reduced
- Fixed bug if mux AVC+MVC tracks to m2ts file. Some 3d m2ts movies did not play on Samsung Smart TV
- Fixed minor bug in a SSIF interleaving for some movies
- introduce Linux build

## tsMuxeR 2.1.4
- Same problem fixed again. Sometimes tsMuxeR get access to file with wrong name during mpls processing.

## tsMuxeR 2.1.3
- Previous version introduce a new bug. Sometime tsMuxeR showed error message "file not found". Fixed.

## tsMuxeR 2.1.2(b);
- fixed bug in MVC stream recognition. MVC from Intel Media Encoder now work.
- SSIF files is not required any more if you open 3D MPLS file
- Add Stereo subtitles basic support. If source PG stream has stereo format, same stereo PG stream will be created in a output file
- Add tag <force> (or <f>) to srt parser. This tag force to show subtitle message. For instance:

	1
	00:00:10,440 --> 00:00:20,375
	<force>	
	<b>Senator</b>, we're making
	our final approach into Coruscant.

## tsMuxeR 2.0.8:
- fixed subtitles bug: "3d-plane" option was inaccessible for many disks

## tsMuxeR 2.0.7:
 improvments:
- add control for select/unselect all tracks at once
 bug fixes:
- extract ac3 core from e-ac3 track fixed
- fixed option --m2tsOffset (was broken in version 2.x.x)
- fixed 'bufer overflow' error message if simultaneously mux several m2ts files and one of them has PSG tracks only
- fixed problem with too long file names in demux mode for large mpls files

## tsMuxeR 2.0.6:
- bug fixed: removing overlapped frames for HD audio fixed

## tsMuxeR 2.0.5:
- add direct ISO output

## tsMuxeR 1.12.10:
- fixed H.264 stream parser. Same fix as in previous version but more careful
- fixed subtitles color selection in UI

## tsMuxeR 1.12.10:
- fixed H.264 stream parser. It cause video distortion for some movies.
- add DTS-express support. Is not fully complete yet, tsMuxeR doesn't produce subpath for secondary audio

## tsMuxeR 1.12.9:
- fixed file join for mov/mp4
- fixed bug in SEI unit processing (if enable options 'insert picture timing'). Bug may cause video distortion.
- fixed distortion for VC1 codec if join several files
- seamless audio fixed. Extra audio frame correctly removed.

## tsMuxeR 1.12.6:
- fixed 3d subtitles. Add ability to select 3D offset plane for subtitles
- add new parameter '--start-time'. This parameter define time for first video frame in output file. This parameter is filled automatically (too keep same input time) if open MPLS file.
- several more minor fixes in transport stream to improve Blu-ray compatibility
- fixed E-AC3 codec

## tsMuxeR 1.12.3:
- fixed problem with ssif muxing
- add addition check for 'insert picture timing' parameter. For MVC depended view used same value as for primary video stream
- add new parameter to GUI and tsMuxeR core: 'right-eye'. Parameter is used for 3D blurays only. If parameter is set then MPEG-4 MVC Base view video used for Right eye.
This parameter filled automatically in GUI if open MPLS file.

## tsMuxeR 1.12.2:
- add 3d bluray support. Bluray muxing activated automatically if MVC substream appears in input tracks.
To reduce HDD space, tsMuxeR doesn't produce ssif file, only a couple of .m2ts files. ssif files can be 
creted on the fly in DVD fab using "create mini iso" menu item.
- add ability to mux to ssif file directly. It is not supported in GUI, but you can provide .ssif file extension
- fixed bugs in SEI message processing and add MVC sei message support
- fixed several bugs in the Transport Stream to improve compatibility with Blu-ray standart.

## tsMuxeR 1.11.6:
- fixed bug in SSIF file demuxing. It cause a problem for subtitles tracks.

## tsMuxeR 1.11.5:
- added SSIF files support for blu-ray play lists (MPLS)

## tsMuxeR 1.11.4:
- detect language for audio/subtitle tracks fixed for SSIF files (it's work if ssif file is opened from Blu-ray disk structure)

## tsMuxeR 1.11.3:
- bug fixed in MVC parsing

## tsMuxeR 1.11.0:
- add support of SSIF files and MVC codec (3d Blu-ray compatibility)
