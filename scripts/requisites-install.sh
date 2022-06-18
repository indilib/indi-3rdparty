#!/bin/bash

set -e

command -v realpath >/dev/null 2>&1 || realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}

SRCS=$(dirname $(realpath $0))/..

# TODO
# libtiff-devel ?

OS=$(uname -s)

case "$OS" in
    Darwin)
        brew install --overwrite \
            git \
            cfitsio libnova libusb curl \
            gsl jpeg fftw \
            ffmpeg libftdi libraw libdc1394 libgphoto2
        ;;
    Linux)
        . /etc/os-release
        case $ID in
            debian|ubuntu|raspbian)
                export DEBIAN_FRONTEND=noninteractive
                $(command -v sudo) apt-get update
                $(command -v sudo) apt-get upgrade -y
                $(command -v sudo) apt-get install -y \
                    git \
                    cmake build-essential zlib1g-dev \
                    libcfitsio-dev libnova-dev libusb-1.0-0-dev libcurl4-gnutls-dev \
                    libgsl-dev libjpeg-dev libfftw3-dev \
                    \
                    libftdi1-dev libavcodec-dev libavdevice-dev libavformat-dev libswscale-dev \
                    libgps-dev libraw-dev libdc1394-22-dev libgphoto2-dev \
                    libboost-dev libboost-regex-dev liblimesuite-dev
                ;;
            fedora)
                $(command -v sudo) dnf upgrade -y
                $(command -v sudo) dnf install -y \
                    git \
                    cmake gcc-c++ zlib-devel \
                    cfitsio-devel libnova-devel libusb-devel libcurl-devel \
                    gsl-devel libjpeg-devel fftw-devel \
                    \
                    https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
                    https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

                $(command -v sudo) dnf install -y \
                    ffmpeg-devel \
                    libftdi-devel \
                    gpsd-devel LibRaw-devel libdc1394-devel libgphoto2-devel \
                    boost-devel

                ;;
            centos)
                # CentOS 8 dont have libnova-devel package
                $(command -v sudo) yum install -y epel-release
                $(command -v sudo) yum upgrade -y
                $(command -v sudo) yum install -y \
                    git \
                    cmake gcc-c++ zlib-devel \
                    cfitsio-devel libnova-devel libusb-devel libcurl-devel \
                    gsl-devel libjpeg-devel fftw-devel
                ;;
            opensuse-tumbleweed)
                # broken git/openssh package
                $(command -v sudo) zypper refresh
                $(command -v sudo) zypper --non-interactive update
                $(command -v sudo) zypper --non-interactive install -y \
                    openssh git \
                    cmake gcc-c++ zlib-devel \
                    cfitsio-devel libnova-devel libusb-devel libcurl-devel \
                    gsl-devel libjpeg-devel fftw-devel libtheora-devel
                ;;
            *)
                echo "Unknown Linux Distribution: $ID"
                cat /etc/os-release
                exit 1
                ;;
        esac
        ;;
    *)
        echo "Unknown System: $OS"
        exit 1
esac

$SRCS/scripts/googletest-build.sh
$SRCS/scripts/googletest-install.sh
