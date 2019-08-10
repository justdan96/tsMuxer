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
* the program currently only compiles 32-bit executables, even on 64-bit systems, a multi-architecture approach is needed
* consider making static executables for Linux, to make the program more portable
* build for Windows requires Visual Studio 2017 and compiling through GUI - we need to create a Makefile equivalent
* create a multi-platform build pipeline, maybe using dockcross or if that doesn't work creating a custom WinPE image just for compiling on Windows
* both Windows and Linux builds require extra files to be downloaded and manually copied into place in order for the builds to succeed - we need to work on that

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

#### Linux

For these examples we have successfully used Ubuntu 19 64-bit and Debian 10 64-bit. 

First we have to install the pre-requisites. On Debian 10 you have to enable the "buster-backports" repo. Then on Debian or Ubuntu you can run the following to install all required packages:

```
# add 32-bit architecture 
sudo dpkg --add-architecture i386
sudo apt-get update

# download/install dependencies
sudo apt-get install build-essential \
flex \
libelf-dev \
libc6-dev \
binutils-dev \
libdwarf-dev \
libc6-dev-i386 \
g++-multilib \
qt5-qmake \
qtbase5-dev \
qtdeclarative5-dev \
qtmultimedia5-dev \
libqt5multimediawidgets5 \
libqt5multimedia5-plugins \
libqt5multimedia5
libfreetype6-dev \
upx \
zlib1g-dev \
git

# on Ubuntu:
sudo apt-get install checkinstall
sudo apt-get install libfreetype6-dev:i386
sudo apt-get install qt5-default

# on Debian:
sudo apt-get -t buster-backports install checkinstall
sudo apt-get install libfreetype6-dev:i386
sudo apt-get install qt5-default
```

With all the dependencies set up we can now actually compile the code.

Open the folder where the git repo is stored in a terminal and run the following:

```
# build libmediation
cd libmediation
make -j$(nproc)

# compile tsMuxer to ../bin
cd ..
cd tsMuxer
make -j$(nproc)

# generate the tsMuxerGUI makefile
cd ..
cd tsMuxerGUI
qmake

# compile tsMuxerGUI to ../bin
make -j$(nproc)

# create lower case links, then create installable deb package and move it to $HOME
cd ../bin
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

#### Windows

To compile tsMuxer and tsMuxerGUI on Windows you will require Visual Studio 2017 Community Edtion. You can run the installer from [here](https://aka.ms/vs/15/release/vs_buildtools.exe).

When selecting the installer options please specify:

* Visual Studio core editor
* Just-In-Time debugger
* Visual C++ core desktop features
* VC 2017 version ... tools
* C++ profiling tools
* Visual C++ ATL for x86 and x64
* Windows 8.1 SDK and UCRT SDK
* MSBuild Tools
* Visual C++ Build Tools core features
* Visual C++ 2017 Redistributable Update
* Windows 10 SDK
* Visual C++ tools for CMake
* Testing tools core features - Build Tools
* C++/CLI support

Next you will need to set up the Windows build dependencies. At the moment these aren't portable, so you will have to follow these steps exactly or the build will fail. We need to set up Freetype and Zlib as they aren't included in the MSVC libraries. 

You can download a compressed archive of these files [here](https://drive.google.com/file/d/1gZZPxZk6zwU8TV_XiVytJJvN4WvPviCQ/view?usp=sharing) or [here](https://s3.eu.cloud-object-storage.appdomain.cloud/justdan96-public/windows-libfreetype-libz.zip). 

Once you download the package you have to install it, you need to  extract the ZIP directly into the root of the C:\ drive (I know, this needs to be improved!). If the path "C:\windev2\include\" exists you know it is set up correctly.

With all the dependencies set up we can now actually compile the code.

Firstly, to compile tsMuxer open the tsMuxer.sln file in Visual Studio, right click the name of the solution and select "Build". Output files are created in ..\bin.

Next to compile tsMuxerGUI you will require a Qt4 installation that is compatible with Visual C++ 2017. This is not generally available, so must be installed manually - again, this is not portable, so you will have to follow these steps exactly or the build will fail.

You can download a compressed archive of a Qt 4.8.7 installation compiled for MSVC++ 2017 32-bit from [here](https://drive.google.com/file/d/1ugv5x-ZCDPlIwUkvFJooki4GxRo1eBBn/view?usp=sharing) or [here](https://s3.eu.cloud-object-storage.appdomain.cloud/justdan96-public/qt-4.8.7-vs2017-32.zip). 

Once you download the package you have to install it, you need to  extract the ZIP directly into the root of the C:\ drive (I know, this needs to be improved!). If the path "C:\Qt\qt-4.8.7-vs2017-32\bin\" exists you know it is set up correctly.

To compile tsMuxerGUI you need to open the tsMuxerGUI folder in a command prompt and then run the following commands:

```
set PATH=C:\Qt\qt-4.8.7-vs2017-32\bin;%PATH%
"C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
qmake
nmake
```

You will find the following files will be created in ..\bin:

* QtCore4.dll
* QtGui4.dll
* tsMuxerGUI.exe
* tsMuxerGUI.exe.manifest
* tsMuxerGUI.pdb

## Financing

We are not currently accepting any kind of donations and we do not have a bounty program.

## Versioning

Version numbering follows the [Semantic versioning](http://semver.org/) approach.

## License

We’re using the Apache 2.0 license for simplicity and flexibility. You are free to use it in your own project.

## Credits

* Roman Vasilenko - for creating tsMuxer
