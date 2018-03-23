# Building on Ubuntu 14.04/16.04/17.10 or Debian stretch

These instructions where tested on Debian stretch and Ubuntu 16.04/17.10 (amd64) but should basically
work on every Debian based system with systemd.

Some basic packages are required for building any of the component.

```console
sudo apt-get install git build-essential cmake
```
To not to interfere with any package of the system the installation is done to
/opt/ogon. To not require any root privileges for the installation the directory
should be created beforehand and given the build user the ownership:
```console
sudo mkdir /opt/ogon
sudo chown ${USER}:${USER} /opt/ogon
```
Once the build is complete it is recommended to change the ownership of the directory
back to root:
```console
sudo chown root:root -R /opt/ogon
```

## FreeRDP

ogon compiles against FreeRDP 2.0, so it's likely that you will have to compile it (except if you
use nightly packages, or if your distribution ships very recent versions of FreeRDP).

```console
sudo apt-get install xsltproc libssl-dev libx11-dev libxext-dev libxinerama-dev libxcursor-dev \
libxdamage-dev libxv-dev libxkbfile-dev libasound2-dev libcups2-dev libxml2 libxml2-dev \
libxrandr-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libxi-dev libgstreamer-plugins-base1.0-dev
```

```console
# for stretch and Ubuntu 16.04 and later
sudo apt-get install libsystemd-dev 
```

```console
git clone https://github.com/ogon-project/freerdp-ogon
cd freerdp-ogon
cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_SERVER=ON -DCMAKE_INSTALL_PREFIX=/opt/ogon -DCMAKE_PREFIX_PATH=/opt/ogon/ -DWITH_GSTREAMER_1_0=ON  .
make -j $(grep -c '^processor' /proc/cpuinfo) install
cd ..
```

For Ubuntu 17.10+ disable GStreamer support with -DWITH_GSTREAMER_1_0=OFF

## ogon

This set contains the RDP listener and the session manager.

### Dependencies

Install some required packages:

```console
sudo apt-get install libprotobuf-dev libprotoc-dev protobuf-compiler protobuf-c-compiler \
	libpam0g-dev libboost-dev libdbus-1-dev automake libpam-systemd ca-certificates \
	libprotobuf-c0-dev ssl-cert
```

### Building

Then you can build:

```console
git clone https://github.com/ogon-project/ogon.git
cd ogon
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/opt/ogon -DCMAKE_PREFIX_PATH=/opt/ogon/ -DWITH_OPENH264=on .
make install #this needs to be single threaded because of the integrated thrift build
```

If you wish to use the H264 encoding, you may execute these commands to download
the openH264 library, and accept the license:

```console
sudo apt-get install curl unzip
./misc/ogon-get-openh264-codec
```

Then these commands will give you a functional installation (but without backends).
The start scripts for systemd, configuration for pam and dbus need to be installed
directly into the system:

```console
cp misc/config.ini.debian /opt/ogon/etc/ogon/config.ini
sudo ln -s /opt/ogon/etc/dbus-1/system.d/ogon.SessionManager.conf /etc/dbus-1/system.d/
sudo service dbus reload
sudo cp misc/pam.d/ogon.debian /etc/pam.d/ogon
```

```console
# Debian stretch/Ubuntu 16.04+
sudo cp misc/ogon-rdp-server.service /etc/systemd/system
sudo cp misc/ogon-session-manager.service /etc/systemd/system
sudo systemctl daemon-reload
```

```console
# Ubuntu 14.04
sudo cp misc/ogon-rdp-server.init /etc/init.d/ogon-rdp-server
sudo cp misc/ogon-session-manager.init /etc/init.d/ogon-session-manager
```

```console
cd ..
mkdir -p /opt/ogon/var/run
```

Then let's start ogon:

```console
# Debian stretch/Ubuntu 16.04+
sudo systemctl start ogon-session-manager
sudo systemctl start ogon-rdp-server
```

```console
# Ubuntu 14.04
sudo /etc/init.d/ogon-session-manager start
sudo /etc/init.d/ogon-rdp-server start
```


## ogon-apps

This set contains common utilities that are used by content providers.

### Dependencies

This is Qt5 based, so let's install the corresponding packages:

```console
sudo apt-get install qtbase5-dev qt5-default qttools5-dev qttools5-dev-tools
```

### Building

```console
git clone https://github.com/ogon-project/ogon-apps.git
cd ogon-apps
cmake -DCMAKE_PREFIX_PATH=/opt/ogon/ -DCMAKE_INSTALL_PREFIX=/opt/ogon .
make -j $(grep -c '^processor' /proc/cpuinfo) install
cd ..
```

## Platform Qt and the Qt greeter

ogon comes with a Qt5 platform plugin that can be used to display any Qt5 application
over RDP. The ogon login screen (aka the greeter) is an application that uses
the ogon Qt5 platform plugin to enter username and password.

### Dependencies

Install the required dependencies, with EGL and rendernodes activated at boot, you will be able
to have QML applications working.

```console
sudo apt-get install libxkbcommon-dev libfontconfig1-dev libmtdev-dev libudev-dev libegl1-mesa-dev qt5-qmake qtbase5-private-dev
```

### Build

```console
git clone https://github.com/ogon/ogon-platform-qt.git
cd ogon-platform-qt
qtchooser -qt=qt5 -run-tool=qmake ADDITIONAL_RPATHS=/opt/ogon/lib/:/opt/ogon/lib/x86_64-linux-gnu/pkgconfig/ PREFIX=/opt/ogon
```

```console
export PKG_CONFIG_PATH=/opt/ogon/lib/pkgconfig/:/opt/ogon/lib/x86_64-linux-gnu/pkgconfig/
make && make install
cd ..
```

As Qt searches for platform plugins in the system folders a link needs to be created:

```console
sudo ln -s /opt/ogon/lib/qt5/plugins/platforms/libogon.so /usr/lib/x86_64-linux-gnu/qt5/plugins/platforms/libogon.so
```

## greeter

The greeter is the application that is shown to the user when he authenticates.

```console
git clone https://github.com/ogon-project/ogon-greeter-qt.git
cd ogon-greeter-qt
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/opt/ogon -DCMAKE_PREFIX_PATH=/opt/ogon/ .
make -j $(grep -c '^processor' /proc/cpuinfo) install
cd ..
```

## ogon-x-backend

Here we will compile a modified xorg server that is able to act as a ogon content provider.
With this you can have a X desktop rendered through ogon.

### Dependencies

Compiling xorg requires quite a lot of packages:

```console
sudo apt-get install autoconf automake xutils-dev libtool libpixman-1-dev x11proto-bigreqs-dev x11proto-composite-dev x11proto-dri3-dev x11proto-present-dev x11proto-resource-dev x11proto-scrnsaver-dev x11proto-fonts-dev  x11proto-xf86dri-dev x11proto-xcmisc-dev x11proto-record-dev xfonts-utils 
```

```
# Ubuntu 14.04
sudo apt-get install libxfont-dev
```
```
# Debian stretch/ Ubuntu 16.04+
sudo apt-get install libxfont1-dev
```


In case you are trying to build under Debian Jessie you will need to compile a more recent version of xtrans

```console
git clone git://anongit.freedesktop.org/xorg/lib/libxtrans -b xtrans-1.3.5
cd libxtrans && mkdir -p output && cd output
../autogen.sh --prefix=/opt/ogon
make && sudo make install
cd ../..
```

### Building

```console
git clone https://github.com/ogon-project/xserver-ogon.git
cd xserver-ogon
NOCONFIGURE=10 ./autogen.sh
PKG_CONFIG_PATH=/opt/ogon/lib/pkgconfig/:/opt/ogon/share/pkgconfig:/opt/ogon/lib/x86_64-linux-gnu/pkgconfig/ ./configure --disable-xfree86-utils --disable-linux-acpi --disable-linux-apm --disable-xorg --disable-xvfb --disable-xquartz --disable-standalone-xpbproxy --disable-xwin --disable-glamor --disable-kdrive --disable-xephyr --disable-xfake --disable-xfbdev --disable-kdrive-kbd --disable-kdrive-mouse --disable-kdrive-evdev --with-vendor-web="http://www.ogon-project.com" --disable-xquartz --disable-xnest --disable-xorg  --prefix=/opt/ogon/ --enable-xogon --disable-xwayland --with-xkb-output=/usr/share/X11/xkb/compiled --with-xkb-path=/usr/share/X11/xkb --with-xkb-bin-directory=/usr/bin/ LDFLAGS="-Wl,-rpath=/opt/ogon/lib:/opt/ogon/lib/x86_64-linux-gnu/"
make  -j $(grep -c '^processor' /proc/cpuinfo)
cd hw/xogon/
make install
cd ../../../
```
## Channels
### ogon-channels - drive and clipboard
#### Dependencies
```console
sudo apt-get install libfuse-dev

```
#### Build
```console
git clone https://github.com/ogon-project/ogon-channels
cd ogon-channels
cmake -DCMAKE_PREFIX_PATH=/opt/ogon -DCMAKE_INSTALL_PREFIX=/opt/ogon .
make -j $(grep -c '^processor' /proc/cpuinfo) install
cd ..
```
### Audio - with pulseaudio
#### Dependencies
```console
sudo apt-get install libsm-dev libxtst-dev libx11-xcb-dev intltool autopoint libltdl-dev libcap-dev libsm-dev libjson-c-dev libsndfile1-dev intltool
```

#### Build
```console
git clone https://github.com/ogon-project/pulseaudio-ogon
cd pulseaudio-ogon
NOCONFIGURE=YES ./bootstrap.sh
PKG_CONFIG_PATH=/opt/ogon/lib/pkgconfig/:/opt/ogon/share/pkgconfig:/opt/ogon/lib/x86_64-linux-gnu/pkgconfig/ ./configure -disable-oss-output --enable-oss-wrapper --disable-alsa --disable-jack --disable-xen --disable-tests --disable-udev --enable-ogon --prefix=/opt/ogon --disable-glib2 --disable-avahi --disable-ipv6 --disable-openssl --enable-x11 --disable-systemd-journal --disable-systemd-daemon LDFLAGS="-Wl,-rpath=/opt/ogon/lib:/opt/ogon/lib/x86_64-linux-gnu/"
make -j $(grep -c '^processor' /proc/cpuinfo) install
cd ..
```
## Test and run

At this point the rdp-server and session-manager should be started and you should be able to connect with any RDP client.
In case you use a firewall make sure port 3389 isn't blocked.

In the example configurations the channels (clipboard, drive redirection, and sound) are not started.
For testing you can start them manually - have a look to misc/startvc.sh.example.
To start them automatically add the option "module_xsession_startvc_string" 
with your start script as parameter to your config.ini in /opt/ogon/etc/ogon/.