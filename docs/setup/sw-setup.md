# Software Setup Guide

This is the step-by-step guide on building the cribsense software on Ubuntu.

## Prerequisites

This software depends on autoconf, OpenCV and libcanberra. Install these by running

```sh
sudo apt-get install git build-essential autoconf libopencv-dev libcanberra-dev
```

## Build

To build the software, navigate to the root of the directory and run

```sh
./autogen.sh --prefix=/usr --sysconfdir=/etc --disable-debug
make
sudo make install
sudo systemctl daemon-reload
```

To start the program:

    sudo systemctl start cribsense

To start the program automatically at every boot:

    sudo systemctl enable cribsense

## Software Architecture

TODO: Describe the software system (state machines, design decisions etc).
