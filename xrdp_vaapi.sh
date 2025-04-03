#!/usr/bin/env bash

set -e

sudo -v

sudo apt-get update
sudo apt-get dist-upgrade -y
sudo apt-get autoremove -y

sudo usermod $USER -a -G video
sudo usermod $USER -a -G tty
sudo usermod $USER -a -G render
sudo usermod $USER -a -G audio

# XRDP Build Pre-reqs Part 1
sudo apt-get install -y git autoconf libtool pkg-config gcc g++ make libssl-dev libpam0g-dev \
    libjpeg-dev libx11-dev libxfixes-dev libxrandr-dev flex bison libxml2-dev intltool xsltproc \
    xutils-dev python3-libxml2 g++ xutils libfuse-dev libmp3lame-dev nasm libpixman-1-dev \
    xserver-xorg-dev libjson-c-dev libsndfile1-dev libspeex-dev libspeexdsp-dev libpulse-dev \
    libpulse0 autopoint \*turbojpeg\* libfdk-aac-dev libopus-dev libgbm-dev libx264\* \
    libx264-dev build-essential dpkg-dev wget #xfce4 firefox-esr #optional

#XRDP Build Pre-reqs Part 2 (For some reason apt needs this to be separate)
sudo apt-get install -y libepoxy-dev

export SOURCE_DIR=$(find ${PWD%${PWD#/*/}} -type d -name "xrdp_vaapi" | head -1) 
BUILD_DIR=${SOURCE_DIR}/xrdp_build
DRIVER_NAME=iHD
SRIOV=false

case ${@} in
    
    *--sriov*|*-s*)
    SRIOV=true
    ;;
    
esac

echo "Building Intel YAMI and Media Driver"
mkdir -p ${BUILD_DIR}
sudo mkdir -p /usr/local/lib/x86_64-linux-gnu
${SOURCE_DIR}/yami/omatic/buildyami.sh

{ sudo ln -s /usr/local/lib/x86_64-linux-gnu/dri/i965_drv_video.so /usr/local/lib/x86_64-linux-gnu/;
sudo ln -s /usr/local/lib/x86_64-linux-gnu/dri/iHD_drv_video.so /usr/local/lib/x86_64-linux-gnu/;
sudo ln -s /usr/local/lib/x86_64-linux-gnu/dri/ /usr/local/lib/dri; } || true

echo "Building xrdp..."
git clone https://github.com/Nexarian/xrdp.git --branch mainline_merge "$BUILD_DIR/xrdp"
cd "$BUILD_DIR/xrdp"
sed -i 's|/opt/yami|/usr/local|g' ./sesman/Makefile.am
sed -i 's|/opt/yami|/usr/local|g' ./xorgxrdp_helper/xorgxrdp_helper_yami.c
./bootstrap
XRDP_YAMI_CFLAGS="-I/usr/local/include" XRDP_YAMI_LIBS="-I/usr/local/lib" ./configure \
    --enable-fuse --enable-rfxcodec --enable-pixman --enable-mp3lame \
    --enable-sound --enable-opus --enable-fdkaac --enable-x264 --enable-yami --enable-avc444
make -j $((`nproc` - 1)) clean all
sudo make install

echo "Building xorgxrdp..."
git clone https://github.com/Nexarian/xorgxrdp.git --branch mainline_merge "$BUILD_DIR/xorgxrdp"
cd "$BUILD_DIR/xorgxrdp"
# sed -i 's/#define MIN_MS_BETWEEN_FRAMES 40/#define MIN_MS_BETWEEN_FRAMES 16/' module/rdpClientCon.c
./bootstrap
./configure --with-simd --enable-glamor
make -j $((`nproc` - 1)) clean all
sudo make install

#Pulseaudio xrdp module
PULSE_MODULE_VER="0.7"
cd $BUILD_DIR
wget https://github.com/neutrinolabs/pulseaudio-module-xrdp/archive/v$PULSE_MODULE_VER.tar.gz \
  -O pulseaudio-module-xrdp.tar.gz
tar xvzf pulseaudio-module-xrdp.tar.gz
rm pulseaudio-module-xrdp.tar.gz
cd pulseaudio-module-xrdp-$PULSE_MODULE_VER
./scripts/install_pulseaudio_sources_apt.sh -d $BUILD_DIR/pulseaudio.src
./bootstrap
./configure PULSE_DIR=$BUILD_DIR/pulseaudio.src
make
sudo make install


#Allow permission to connect
sudo tee /etc/X11/Xwrapper.config > /dev/null << EOL
# Xwrapper.config (Debian X Window System server wrapper configuration file)
#
# This file was generated by the post-installation script of the
# xserver-xorg-legacy package using values from the debconf database.
#
# See the Xwrapper.config(5) manual page for more information.
#
# This file is automatically updated on upgrades of the xserver-xorg-legacy
# package *only* if it has not been modified since the last upgrade of that
# package.
#
# If you have edited this file but would like it to be automatically updated
# again, run the following command as root:
#   dpkg-reconfigure xserver-xorg-legacy 
needs_root_rights=no
allowed_users=anybody
EOL

#Disable nvidia
sudo sed -i -E 's#param=xrdp/xorg_nvidia.conf#param=xrdp/xorg.conf#' /etc/xrdp/sesman.ini
sudo sed -i 's/XRDP_USE_HELPER=1/XRDP_USE_HELPER=0/' /etc/xrdp/sesman.ini

#Installing i915_sriov_dkms
if ${SRIOV}; then
echo "Installing i915_sriov_dkms kernel module"
SRIOV_CLONE="https://github.com/strongtz/i915-sriov-dkms"
SRIOV_GIT="${SRIOV_CLONE}/releases/latest"
VERSION=$(curl -Ls -o /dev/null -w %{url_effective} ${SRIOV_GIT} | sed -e 's@.*/@@')

sudo apt-get install -y build-* git dkms linux-headers-$(uname -r)
cd ${BUILD_DIR}
git clone ${SRIOV_CLONE} --branch master
sudo dkms add ./i915-sriov-dkms
sudo dkms install i915-sriov-dkms/${VERSION}
sudo sed -i 's|GRUB_CMDLINE_LINUX_DEFAULT=".*"|GRUB_CMDLINE_LINUX_DEFAULT="intel_iommu=on i915.enable_guc=3 module_blacklist=xe"|' /etc/default/grub

fi

echo "Adding udev rule for glamor accellerated session"
sudo /bin/bash -c 'cat > /usr/lib/udev/rules.d/90-xorgxrdp-dri.rules << EOF
# installed by xorgxrdp. allows every user to use glamor accellerated session
SUBSYSTEM=="drm", KERNEL=="renderD*", GROUP="render", MODE="0666"
SUBSYSTEM=="kfd", GROUP="render", MODE="0666"
EOF
'

echo "Enabling VM Boot with -vga none (qemu) | Video=none (libvirt)"
sudo sed -i "s/#GRUB_TERMINAL=console/GRUB_TERMINAL=console/" /etc/default/grub
sudo update-grub
sudo update-initramfs -u

echo "Adding LIBVA_DRIVER_NAME ENV Variable to sesman.ini"
sudo /bin/bash -c "echo LIBVA_DRIVER_NAME=$DRIVER_NAME >> /etc/xrdp/sesman.ini"

echo "Starting the server..."
sudo systemctl enable xrdp
sudo service xrdp start
