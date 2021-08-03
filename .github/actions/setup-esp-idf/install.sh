#!/usr/bin/env bash

version="$1"

sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    git wget flex bison gperf python3 python3-pip python3-setuptools cmake \
    ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

mkdir ~/esp
cd ~/esp

git clone -b "$version" --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/esp-idf
./install.sh 2>&1 | tee ~/esp-idf-install.log
