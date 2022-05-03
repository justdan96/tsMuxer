# tsMuxer

## Vision

This project is for tsMuxer - a transport stream muxer for remuxing/muxing elementary streams. This is very useful for transcoding and this project is used in other products such as Universal Media Server.

EVO/VOB/MPG, MKV/MKA, MP4/MOV, TS, M2TS to TS to M2TS.

Supported video codecs H.264/AVC, H.265/HEVC, H.266/VVC (Alpha release), VC-1, MPEG2. 
Supported audio codecs AAC, AC3 / E-AC3(DD+), DTS/ DTS-HD - please note TrueHD must have the AC3 core intact.

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

Please see [INSTALLATION.md](docs/INSTALLATION.md) for installation instructions.

## Usage

Please see [USAGE.md](docs/USAGE.md) for usage instructions.

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

For full details on building tsMuxer for your platform please see the document on [COMPILING](docs/COMPILING.md).

## Testing

The very rough and incomplete testing document is available at [TESTING.md](docs/TESTING.md).

## Financing

We are not currently accepting any kind of donations and we do not have a bounty program.

### Sponsorships

The project is part of the [MacStadium Open Source Program](https://www.macstadium.com/opensource) to create native Apple Silicon executables for Mac OS.

![MacStadiumOpenSource](https://uploads-ssl.webflow.com/5ac3c046c82724970fc60918/5c019d917bba312af7553b49_MacStadium-developerlogo.png)

## Versioning

Version numbering follows the [Semantic versioning](http://semver.org/) approach.

## License

We’re using the Apache 2.0 license for simplicity and flexibility. You are free to use it in your own project.

## Credits

**Original Author**
Roman Vasilenko (physic)

**Contributors**
* Daniel Bryant (justdan96)
* Daniel Kozar (xavery)
* Jean Christophe De Ryck (jcdr428)
* Stephen Hutchinson (qyot27)
* Koka Abakum (abakum)
* Alexey Shidlovsky (alexls74)
* Lonely Crane (lonecrane)
* Markus Feist (markusfeist)

<sub><sup>For sake of brevity I am including anyone who has merged a pull request!</sup></sub>
