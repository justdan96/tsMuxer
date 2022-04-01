# tsMuxer : Installation Instructions

The following executables are created to be portable, so you can just save and extract the compressed package for your platform. 

Nightly builds are created by GitHub Actions and stored as [releases](https://github.com/justdan96/tsMuxer/releases). Please visit the releases page in order to download the release you're interested in.

## Windows

The ZIP file for Windows can just be unzipped and the executables can be used straight away - there are no dependencies.

## Linux

The ZIP file for Linux can just be unzipped and the executables can be used straight away - there are no dependencies.

The following are packages created specifically for each distribution. To update, please uninstall the package and then install it again.

### Mageia Cauldron
```
dnf config-manager --add-repo https://download.opensuse.org/repositories/home:/justdan96/Mageia_Cauldron/home:justdan96.repo
```

### openSUSE Leap 15.2
```
zypper ar -r https://download.opensuse.org/repositories/home:/justdan96/openSUSE_Leap_15.2/home:justdan96.repo
```

### openSUSE Tumbleweed
```
zypper ar -r https://download.opensuse.org/repositories/home:/justdan96/openSUSE_Tumbleweed/home:justdan96.repo
```

### Fedora 31
```
dnf config-manager --add-repo https://download.opensuse.org/repositories/home:/justdan96/Fedora_31/home:justdan96.repo
dnf config-manager --set-enabled home_justdan96
```

### Debian 10
```
wget -qO - https://download.opensuse.org/repositories/home:/justdan96/Debian_debbuild_10/Release.key | sudo apt-key add -
echo "deb https://download.opensuse.org/repositories/home:/justdan96/Debian_debbuild_10  ./" | sudo tee -a /etc/apt/sources.list > /dev/null
```

### Ubuntu 18.04 - 20.10
```
wget -qO - https://download.opensuse.org/repositories/home:/justdan96/Ubuntu_debbuild_$(lsb_release -r -s)/Release.key | sudo apt-key add -
echo "deb https://download.opensuse.org/repositories/home:/justdan96/Ubuntu_debbuild_$(lsb_release -r -s)  ./" | sudo tee -a /etc/apt/sources.list > /dev/null
```

## MacOS

The ZIP file for MacOS can just be unzipped, as the executables should be relocatable. 

If you receive missing dependency errors you may need to install a couple of dependencies, using the commands below in the Terminal:
```
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)" < /dev/null 2> /dev/null
brew install freetype
brew install zlib
```
