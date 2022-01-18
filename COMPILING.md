# Compiling

The following sections outline how to build tsMuxer and tsMuxerGUI for your chosen platform.

## Docker (All Platforms)

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

## Linux

For these examples we have successfully used Ubuntu 19 64-bit and Debian 10 64-bit. 

First we have to install the pre-requisites. On Debian 10 you have to enable the "buster-backports" repo. Then on Debian or Ubuntu you can run the following to install all required packages for your chosen platform:

Common:
```
sudo apt-get update
sudo apt-get install build-essential g++-multilib ninja-build cmake

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
libqt5multimedia5 \
qttools5-dev \
qttools5-dev-tools
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

## Windows (MXE on Linux)

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

## Windows (Msys2)

To compile tsMuxer and tsMuxerGUI on Windows with Msys2, you must download and install [Msys2](https://www.msys2.org/). Once you have Msys2 fully configured, open an Msys2 prompt and run the following commands, depending on which build you require:

Common:
```
pacman -Syu
pacman -Sy --needed base-devel \
flex \
zlib-devel \
git
```

Or just run:
```
./rebuild_msys2.sh
```

Close the Msys2 prompt and then open either a Mingw32 or a Mingw64 prompt, depending on whether you want to build for 32 or 64 bit.
```
pacman -Sy --needed $MINGW_PACKAGE_PREFIX-toolchain \
$MINGW_PACKAGE_PREFIX-cmake \
$MINGW_PACKAGE_PREFIX-freetype \
$MINGW_PACKAGE_PREFIX-zlib \
$MINGW_PACKAGE_PREFIX-ninja
```

If you intend to build the GUI as well you need to also install these, depending on your platform (please note Qt5 takes up a LOT of disk space!):
```
pacman -Sy --needed $MINGW_PACKAGE_PREFIX-qt5-static
```

Before we compile anything we have to alter a file to work around [this bug](https://bugreports.qt.io/browse/QTBUG-76660). Run the following commands to fix that:
```
echo 'load(win32/windows_vulkan_sdk)' > $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
echo 'QMAKE_LIBS_VULKAN       =' >> $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
```

Download tsMuxer repo and browse to the it location by run:
```
cd ~
git clone https://github.com/justdan96/tsMuxer.git
cd tsMuxer
```
Compile tsMuxer by run:
```
./rebuild_linux.sh
```

This will create in tsMuxer/bin statically compiled versions of tsMuxer - so no external DLL files are required.

Or just run:
```
./rebuild_msys2.sh
```

This will create in tsMuxer/bin statically compiled versions of tsMuxer and tsMuxerGUI - so no external DLL files are required.

## MacOS (osxcross on Linux)

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

## MacOS (Native)

To use compile tsMuxer on Mac natively we must first use Homebrew to install some dependencies:

```
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)" < /dev/null 2> /dev/null
brew install freetype
brew install zlib
```

Next we need to install Qt. Please note that Qt through Homebrew has issues with `macdeployqt` so is *not* supported. To ensure it is not installed at all run the commands below:

```
brew uninstall qt
```

We will use `aqtinstall` to download and install the offical Qt for Mac package. Qt 6.2.2 officially supports Apple silicon so this version is recommended. To install Qt6 for Mac we will need to install `pip`, use that to install `aqtinstall`, use `aqtinstall` to download the latest version of Qt for Mac before finally copying the installation and enabling it to be used system-wide.

```
# install pip
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
python3 get-pip.py
# install aqtinstall and download Qt 6.2.2
pip install aqtinstall
aqt install-qt mac desktop 6.2.2 -m qtmultimedia
# install Qt to /opt/qt
sudo mkdir /opt/qt
sudo cp -r ./6.2.2/macos/* /opt/qt/
# make Qt bin folder available in PATH
echo 'export PATH=/opt/qt/bin:$PATH' >> $HOME/.zprofile
. $HOME/.zprofile
# cleanup temporary files
rm -f get-pip.py
rm -rf ./6.2.2
```

With all of those requirements met we can now compile the programs. Simply run `./build_macos_native.sh` from the repository folder. Upon completion the executables will be available in the ./build/bin folder.
