# tsMuxer : Testing

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

We will have also done the same with the test file [Life Untouched](https://4kmedia.org/life-untouched-hdr-uhd-4k-demo). Results of the MD5 sums for the original and output file are below:
```
f2db8f6647f4f2a0b2417aed296fee73  Life Untouched 4K Demo.mp4
aba9ee3a3211cd09ba4833a610faff22  Life Untouched 4K Demo.m2ts
```

We need more sample files with 3D and multiple subtitle tracks if possible so if you have any ways of testing these files (particularly in relation to the bugs in the TODO section) please let us know?
