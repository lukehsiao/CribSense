# Setup Guide

This is the step-by-step guide on building the cribsense software on Ubuntu.

## Prerequisites
This software depends on Qt5 and OpenCV. Install these by running

```sh
sudo apt-get install qtbase5-dev qtdeclarative5-dev libqt5webkit5-dev libsqlite3-dev
sudo apt-get install qt5-default qttools5-dev-tools
```

To install [OpenCV](http://docs.opencv.org/2.4/doc/tutorials/introduction/linux_install/linux_install.html), run:
```
sudo apt-get install build-essential
sudo apt-get install cmake git libgtk2.0-dev pkg-config libavcodec-dev libavformat-dev libswscale-dev
sudo apt-get install libopencv-dev
```

## Build
To build the software, navigate to the root of the directory and run

```sh
qmake src/Magnifier.pro
make
```

To execute the program:

```
./Magnifier
```
