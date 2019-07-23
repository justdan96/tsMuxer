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
* create a multi-platform build pipeline, maybe using dockcross

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

To build the project you will need a machine for your chosen platform. In this example we will use Linux, specifically Ubuntu 19 64-bit. 

First we have to install the pre-requisites. On Ubuntu you can run the following to install all required packages:

```
sudo apt-get install libfreetype6-dev \
build-essential \
flex \
libelf-dev \
libc6-dev \
binutils-dev \
libdwarf-dev \
libc6-dev-i386 \
g++-multilib \
upx \
qt4-qmake \
libqt4-dev
```

Unfortunately on Ubuntu this isn't enough. We need to install libfreetype and it's dependencies as 32-bit libraries. You can download a compressed archive of these files [here](https://dropapk.com/6308nkz3zpej) or [here](https://drive.google.com/file/d/1pvQoYIvwRlH2DPYQNTrBvFhF6E54QUpE/view?usp=sharing). 

Once you download the package you have to install it, the easiest thing is to extract the tar directly into the correct folder as root (I know, this needs to be improved!). Assuming you downloaded the tar to /home/me/Downloads:

```
cd /lib32
sudo tar --strip-components=1 -xvf /home/me/Downloads/ubuntu-libfreetype-lib32.tar.gz lib32
```

With all the dependencies set up we can now actually compile the code.

Open the folder where the git repo is stored in a terminal and run the following:

```
# compile tsMuxer to ../bin
cd tsMuxer
make

# generate the tsMuxerGUI makefile
cd ..
cd tsMuxerGUI
qmake

# compile tsMuxerGUI to ../bin
make

# UPX compress the executables, then create a release package as tsMuxeR.tar.gz from the install folder
cd ..
cd tsMuxer
make install
```

## Financing

We are not currently accepting any kind of donations and we do not have a bounty program.

## Versioning

Version numbering follows the [Semantic versioning](http://semver.org/) approach.

## License

We’re using the Apache 2.0 license for simplicity and flexibility. You are free to use it in your own project.

## Credits

* Roman Vasilenko - for creating tsMuxer
