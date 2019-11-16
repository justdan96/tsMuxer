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

## Todo

The following is a list of changes that will need to be made to the original source code and project in general:

* swapping custom includes from libmediation to their standard library equivalents
* the code uses my_htonl, my_ntohll, etc - these can be swapped for the standard library versions
* the program doesn't support MPEG-4 ASP, even though MPEG-4 ASP is defined in the TS specification
* no Opus audio support
* [several](https://forum.doom9.org/showthread.php?p=1880216#post1880216) [muxing](https://forum.doom9.org/showthread.php?p=1881372#post1881372) [bugs](https://forum.doom9.org/showthread.php?p=1881509#post1881509) when muxing a HEVC/UHD stream - results in an out-of-sync stream
* has issues with 24-bit DTS Express
* issues with the 3D plane lists when there are mismatches between the MPLS and M2TS
* a [bug](https://forum.doom9.org/showpost.php?p=1888650&postcount=74) with unsynchronised AV in HEVC files because the coded picture buffer is not large enough
* a [bug](https://forum.doom9.org/showthread.php?p=1880216#post1880216) with UHD HEVC where the [wrong stream type is used](https://forum.doom9.org/showpost.php?p=1888746&postcount=75) 

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

### Submitting bugs

You can report issues directly on Github, that would be a really useful contribution given that we lack some user testing on the project. Please document as much as possible the steps to reproduce your problem (even better with screenshots).

### Building

#### Docker (All Platforms)

You can use our [Docker container](https://github.com/justdan96/tsmuxer_build) to build tsMuxer for your chosen platform. To build the GUI you will need to follow the instructions specifically for your platform.

To create the builds using the Docker container, follow the steps below:

1. Pull `justdan96/tsmuxer_build` from the Docker repository:
```
docker pull justdan96/tsmuxer_build
```

Or build `justdan96/tsmuxer_build` from source:
```
git clone https://github.com/justdan96/tsmuxer_build.git
cd tsmuxer_build
docker build -t justdan96/tsmuxer_build .
```

2. Browse to the tsMuxer repository and run one of the following commands:

*Linux*
```
docker run -it --rm -v $(pwd):/workdir -w="/workdir" justdan96/tsmuxer_build bash -c ". rebuild_linux_docker.sh"
```

*Windows*
```
docker run -it --rm -v $(pwd):/workdir -w="/workdir" justdan96/tsmuxer_build bash -c ". rebuild_mxe_docker.sh"
```

*OSX*
```
docker run -it --rm -v $(pwd):/workdir -w="/workdir" justdan96/tsmuxer_build bash -c ". rebuild_osxcross_docker.sh"
```

The executable binary will be saved to the "\bin" folder.

#### Linux

For these examples we have successfully used Ubuntu 19 64-bit and Debian 10 64-bit. 

First we have to install the pre-requisites. On Debian 10 you have to enable the "buster-backports" repo. Then on Debian or Ubuntu you can run the following to install all required packages for your chosen platform:

Common:
```
sudo apt-get update
sudo apt-get install build-essential \
g++-multilib
ninja

# on Ubuntu:
sudo apt-get install checkinstall

# on Debian:
sudo apt-get -t buster-backports install checkinstall
```

32-bit:
```
# add 32-bit architecture 
sudo dpkg --add-architecture i386
sudo apt-get update

# download/install dependencies
sudo apt-get install libc6-dev-i386 \
sudo apt-get install libfreetype6-dev:i386
```

64-bit:
```
sudo apt-get update
sudo apt-get install libc6-dev \
libfreetype6-dev \
zlib1g-dev \
```

If you also intend to build the GUI then you require:

64-bit:
```
sudo apt-get install qt5-default \
qt5-qmake \
qtbase5-dev \
qtdeclarative5-dev \
qtmultimedia5-dev \
libqt5multimediawidgets5 \
libqt5multimedia5-plugins \
libqt5multimedia5
```

With all the dependencies set up we can now actually compile the code.

Open the folder where the git repo is stored in a terminal and run the following to build just the command-line program:

```
# build the project
./rebuild_linux.sh
```

Or run the following to build the GUI as well:

```
# build the project
./rebuild_linux_with_gui.sh
```

Next run the below to create a DEB file:
```
# create lower case links, then create installable deb package and move it to $HOME
cd ../bin
cp ../build/tsMuxer/tsmuxer .
cp ../build/tsMuxerGUI/tsmuxergui .
ln -s tsMuxeR tsmuxer
ln -s tsMuxerGUI tsmuxergui
sudo checkinstall \
    --pkgname=tsmuxer \
    --pkgversion="1:$(./tsmuxer | \
                      grep "tsMuxeR version" | \
                      cut -f3 -d " " | \
                      sed 's/.$//')-git-$(\
                      git rev-parse --short HEAD)-$(\
                      date --rfc-3339=date | sed 's/-//g')" \
    --backup=no \
    --deldoc=yes \
    --delspec=yes \
    --deldesc=yes \
    --strip=yes \
    --stripso=yes \
    --addso=yes \
    --fstrans=no \
    --default cp -R * /usr/bin && \
mv *.deb ~/
cd $HOME
```

#### Windows (MXE on Linux)

To compile tsMuxer and tsMuxerGUI for Windows using MXE on Linux you must follow the steps below on Ubuntu:

```
# setup pre-reqs
sudo apt-get install -y software-properties-common
sudo apt-get install -y apt-transport-https
sudo apt-get install -y checkinstall
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys C6BF758A33A3A276

# add MXE repo
sudo add-apt-repository -y 'deb https://mirror.mxe.cc/repos/apt stretch main'
sudo apt-get update

# install necessary MXE components for building tsmuxer
sudo apt-get install -y mxe-x86-64-w64-mingw32.static-zlib
sudo apt-get install -y mxe-x86-64-w64-mingw32.static-harfbuzz
sudo apt-get install -y mxe-x86-64-w64-mingw32.static-freetype
sudo apt-get install -y mxe-x86-64-w64-mingw32.static-cmake
sudo apt-get install -y mxe-x86-64-w64-mingw32.static-ccache
sudo apt-get install -y mxe-x86-64-w64-mingw32.static-autotools
sudo apt-get install -y mxe-x86-64-pc-linux-gnu-autotools
sudo apt-get install -y mxe-x86-64-pc-linux-gnu-ccache
sudo apt-get install -y mxe-x86-64-pc-linux-gnu-cc
sudo apt-get install -y mxe-x86-64-pc-linux-gnu-cmake

# manually fix some weird symlinks
sudo rm /usr/lib/mxe/usr/x86_64-pc-linux-gnu/bin/x86_64-w64-mingw32.static-g++
sudo rm /usr/lib/mxe/usr/x86_64-pc-linux-gnu/bin/x86_64-w64-mingw32.static-gcc
sudo ln -s /usr/lib/mxe/usr/bin/x86_64-w64-mingw32.static-g++ /usr/lib/mxe/usr/x86_64-pc-linux-gnu/bin/x86_64-w64-mingw32.static-g++
sudo ln -s /usr/lib/mxe/usr/bin/x86_64-w64-mingw32.static-gcc /usr/lib/mxe/usr/x86_64-pc-linux-gnu/bin/x86_64-w64-mingw32.static-gcc
sudo rm /usr/lib/mxe/usr/x86_64-pc-linux-gnu/bin/g++
sudo rm /usr/lib/mxe/usr/x86_64-pc-linux-gnu/bin/gcc
sudo ln -s /usr/lib/mxe/usr/bin/x86_64-w64-mingw32.static-g++ /usr/lib/mxe/usr/x86_64-pc-linux-gnu/bin/g++
sudo ln -s /usr/lib/mxe/usr/bin/x86_64-w64-mingw32.static-gcc /usr/lib/mxe/usr/x86_64-pc-linux-gnu/bin/gcc
```

If you want to compile the GUI as well you also need to install the below (please note Qt5 takes up a LOT of disk space!):
```
sudo apt-get install -y mxe-x86-64-w64-mingw32.static-qt5
```

With all the dependencies set up we can now actually compile the code.

Open the folder where the git repo is stored in a terminal and run the following to build just the command-line program:

```
# build the project
./rebuild_mxe.sh
```

Or run the following to build the GUI as well:

```
# build the project
./rebuild_mxe_with_gui.sh
```

#### Windows (Msys2)

To compile tsMuxer and tsMuxerGUI on Windows with Msys2, you must download and install [Msys2](https://www.msys2.org/). Once you have Msys2 fully configured, open an Msys2 prompt and run the following commands, depending on which build you require:

Common:
```
pacman -Syu
pacman -Sy --needed base-devel \
flex \
zlib-devel
```

32-bit:
```
pacman -Sy --needed mingw-w64-i686-toolchain \
mingw-w64-i686-cmake \
mingw-w64-i686-freetype \
mingw-w64-i686-zlib
```

64-bit:
```
pacman -Sy --needed mingw-w64-x86_64-toolchain \
mingw-w64-x86_64-cmake \
mingw-w64-x86_64-freetype \
mingw-w64-x86_64-zlib
```

If you intend to build the GUI as well you need to also install these, depending on your platform (please note Qt5 takes up a LOT of disk space!):

32-bit:
```
pacman -Sy --needed mingw-w64-i686-qt5-static
```

64-bit:
```
pacman -Sy --needed mingw-w64-x86_64-qt5-static
```

Close the Msys2 prompt and then open either a Mingw32 or a Mingw64 prompt, depending on whether you want to build for 32 or 64 bit. Before we compile anything we have to alter a file to work around [this bug](https://bugreports.qt.io/browse/QTBUG-76660). Run the following commands to fix that:

```
echo 'load(win32/windows_vulkan_sdk)' > $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
echo 'QMAKE_LIBS_VULKAN       =' >> $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
```

With that fixed, browse to the location of the tsMuxer repo and then run the following commands:

```
mkdir build
cd build
cmake ../ -G Ninja
cmake . --build
cp tsMuxer/tsmuxer.exe tsMuxerGUI/tsMuxeR.exe
```

This will create statically compiled versions of tsMuxer and tsMuxerGUI - so no external DLL files are required.

#### MacOS (osxcross on Linux)

To use osxcross on Ubuntu to compile for OSX, first run the following commands:

```
# setup pre-reqs
sudo apt-get install -y clang
sudo apt-get install -y patch lzma-dev libxml2-dev libssl-dev python curl
```

Next you have a choice - compile osxcross from source or use a prepared package. To compile osxcross from source:

```
# set up a new osxcross installation
cd /tmp
git clone https://github.com/tpoechtrager/osxcross.git
cd osxcross
curl -sLo tarballs/MacOSX10.10.sdk.tar.xz "https://s3.eu.cloud-object-storage.appdomain.cloud/justdan96-public/MacOSX10.10.sdk.tar.xz"

export SDK_VERSION="10.10"
export OSX_VERSION_MIN="10.6"
UNATTENDED=1 ./build.sh

rm -rf "target/SDK/MacOSX10.10.sdk/usr/share/man"

# copy to permanent home as root
sudo su -
cp -r target /usr/lib/osxcross                                                                                    
cp -r tools /usr/lib/osxcross/ 
exit

# remove temporary folder
cd ..
rm -rf osxcross
```

To install osxcross from a pre-compiled package for Ubuntu 19 x86_64:

```
# download the package to /tmp
curl -sLo /tmp/osxcross-6acb50-20191025.tgz "https://s3.eu.cloud-object-storage.appdomain.cloud/justdan96-public/osxcross-6acb50-20191025.tgz"

# extract to correct location
sudo su -
cd /tmp
mkdir /usr/lib/osxcross
tar -xzf osxcross-6acb50-20191025.tgz --strip-components=1 -C /usr/lib/osxcross
exit

# remove tar file
rm -f osxcross-6acb50-20191025.tgz
```

Now to setup the tsMuxer build dependencies for osxcross:

```
# install freetype and zlib in root session with osxcross-macports, dependencies for tsMuxer build
sudo su -
export MACOSX_DEPLOYMENT_TARGET=10.10
export PATH=/usr/lib/osxcross/bin:/usr/lib/osxcross/tools:$PATH
/usr/lib/osxcross/bin/osxcross-conf
/usr/lib/osxcross/bin/osxcross-macports
/usr/lib/osxcross/bin/osxcross-macports install freetype
/usr/lib/osxcross/bin/osxcross-macports install zlib
exit
```

With all the dependencies set up we can now actually compile the code.

Open the folder where the git repo is stored in a terminal and run the following to build just the command-line program:

```
# build the project
./rebuild_osxcross.sh
```

Or run the following to build the GUI as well:

```
# build the project
./rebuild_osxcross_with_gui.sh
```

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
