#!/bin/bash

## This script builds the latest freerdp2 tag available (Debian Trixie)

BUILD_DIR=${PWD}/build
CUSTOM_VER=""
REPO="FreeRDP/FreeRDP"
GIT_URL="https://github.com/${REPO}/releases/latest"
BRANCH=$(curl -Ls -o /dev/null -w %{url_effective} ${GIT_URL} | sed -e 's@.*/@@')
BRANCH="master"

mkdir -p "$BUILD_DIR"
rm -rf "$BUILD_DIR/*"

[ $(grep -Pc "deb\s.*(trixie|bookworm)\s.*(non-free|non-free-firmware)" /etc/apt/sources.list) -ge 2 ] || \
sudo /bin/bash -c 'cat >> /etc/apt/sources.list << EOF
deb https://deb.debian.org/debian trixie main contrib non-free non-free-firmware
deb https://deb.debian.org/debian bookworm main contrib non-free non-free-firmware
EOF'

sudo apt-get update && sudo apt-get install -y -t trixie build-essential ccache cdbs clang clang-format \
cmake cmake-curses-gui debhelper docbook-xsl dpkg-dev gcc git git-core libasound2-dev libavcodec-dev libavutil-dev \
libcairo2-dev libcjson-dev libcups2-dev libfaac-dev libfaad-dev libfuse3-dev libfuse-dev libgsm1-dev libgstreamer1.0-dev \
libicu-dev libjson-c-dev libkrb5-dev libmp3lame-dev libopenh264-dev libopus-dev libpam0g-dev libpcsclite-dev libpkcs11-helper1-dev \
libpkcs11-helper-dev libpulse-dev libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl3-dev libsdl3-ttf-dev libsoxr-dev libswresample-dev \
libswscale-dev libsystemd-dev liburiparser-dev libusb-1.0-0-dev libusb-dev libwayland-dev libwebkit2gtk-4.0-dev libx11-dev libxcursor-dev libssl-dev \
libxdamage-dev libxext-dev libxfixes-dev libxi-dev libxinerama-dev libxkbfile-dev libxml2-dev libxrandr-dev libxrender-dev libxtst-dev libxv-dev ninja-build \
ocl-icd-opencl-dev opencl-c-headers pkg-config uuid-dev xmlto xsltproc llvm-dev

git clone --depth 1 https://github.com/freerdp/freerdp.git --branch ${BRANCH} "$BUILD_DIR/freerdp"

cd "$BUILD_DIR"

#git fetch origin pull/10191/head:vaapi-updates
#git checkout vaapi-updates

cmake -GNinja \
    -B freerdp-build \
    -S freerdp \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DWITH_SERVER=ON \
    -DWITH_SAMPLE=ON \
    -DWITH_PLATFORM_SERVER=OFF \
    -DUSE_UNWIND=OFF \
    -DWITH_FFMPEG=ON \
    -DWITH_OPENH264=ON \
    -DWITH_WEBVIEW=OFF \
    -DWITH_SWSCALE=OFF \
    -DWITH_VERBOSE_WINPR_ASSERT=OFF \
    -DWITH_X11=yes \
    -DWITH_VAAPI=on

cmake --build freerdp-build

sudo cmake --install freerdp-build
