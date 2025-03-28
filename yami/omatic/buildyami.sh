#!/usr/bin/env bash

sudo -v

# Packages necessary for building the Intel drivers as well as for
# YAMI to initialize properly at runtime.
sudo apt-get -y install autoconf libtool libdrm-dev xorg xorg-dev \
openbox libx11-dev libgl1-mesa-glx libegl1-mesa libegl1-mesa-dev \
libgl1-mesa-dev meson doxygen cmake libx11-xcb-dev libxcb-dri3-dev \
jq wget libx11-xcb-dev libxcb-dri3-dev


git_prep() {

case ${1} in

	*lab*)
	GIT_URL="https://gitlab.freedesktop.org/api/v4/projects/${2}/repository/tags"
	TAG=$(curl -s ${GIT_URL} | jq '.[]' | jq -r '.name' | grep -m1 "${3}" | head -1)
	TAG_URL="https://gitlab.freedesktop.org/api/v4/projects/${2}/repository/archive?sha=${TAG}"
	OUTPUT="${TAG}.tar.gz"
	;;

	*hub*)
	[[ ${1} =~ 'rel' ]] && { TARGET="releases"; REGEX="https.*releases/download.*${3}.*tar.(gz|bz2)"; } || \
	{ TARGET="tags"; REGEX='https.*tarball.*/tags/.*'; EXTENSION=".tar.gz"; }
	GIT_URL="https://api.github.com/repos/${2}/${TARGET}"
	TAG_URL=$(curl -s ${GIT_URL} | sed 's/[()",{}]/ /g; s/ /\n/g' | grep -Pom1 "${REGEX}")
	OUTPUT="${TAG_URL##*/}${EXTENSION}"
	unset EXTENSION
	;;
	
	*clone*)
	
	case ${CUSTOM} in
		
		true)
		GIT_URL="https://api.github.com/repos/${2}/tags"
		BRANCH=$(curl -s ${GIT_URL} | jq '.[]' | jq -r '.name' | grep -Pm1 "${3}")
		;;
		
		false)
		GIT_URL="https://github.com/${2}/releases/latest"
		BRANCH=$(curl -Ls -o /dev/null -w %{url_effective} ${GIT_URL} | sed -e 's@.*/@@')
		;;	
	esac
	
	CLONE_URL="https://github.com/${2}.git"
	git clone ${CLONE_URL} --branch ${BRANCH} ${BRANCH} || { echo "error cloning ${BRANCH}"; exit 1; }
	BUILD_PATH=${BRANCH}
	;;



esac

	[[ ${1} =~ get ]] && { wget ${TAG_URL} -O ${OUTPUT} || { echo "error downloading ${OUTPUT}"; exit 1; };
	BUILD_PATH=${OUTPUT%.tar*};
	mkdir ${BUILD_PATH};
	tar -xf ${OUTPUT} -C ${BUILD_PATH} --strip-components 1; }
	
	BUILD_ARRAY+=(${PWD}/${BUILD_PATH})

}

download_source() {

for i in "${!PACKAGES[@]}"; do

	GIT="${!PACKAGES[i]:0:1}"
	ID="${!PACKAGES[i]:1:1}"
	${CUSTOM} && VERSION="${!PACKAGES[i]:2:1}" || VERSION=""
	
	git_prep "$GIT" "$ID" "$VERSION"

done

echo "Build Paths: ${BUILD_ARRAY[@]}"

}

build_commands() {

case ${1} in

	configuration)
	
	case ${2} in

		meson)
		meson _build -Dprefix=$INSTALL_PATH "${CONFIG}"
		;;

		configure)
		./configure --prefix=$INSTALL_PATH "${CONFIG}"
		;;
		
		cmake)
		mkdir _build
		cd _build
		cmake "${BUILD_ARGS}" "${CONFIG}" ..
		;;

	esac
	
	;;

	build)

	[[ ${SOURCE_NAME} =~ ^libva-[0-9.]+$ ]] && \
	{ echo "" >> config.h; echo "#define va_log_info(buffer)" >> config.h; echo "" >> config.h; }
	
	case ${2} in

		meson)
		ninja -C _build
		;;

		configure)
		make -j"$((`nproc` - 1))"
		;;
		
		cmake)
		make -j"$((`nproc` - 1))"
		;;

	esac

	;;
	
	install)
	
	case ${2} in

		meson)
		cd _build
		sudo meson install
		;;

		configure)
		sudo make install-strip
		;;
		
		cmake)
		sudo make install
		;;

	esac

	;;

esac

}

build_source() {

for i in ${!BUILD_ARRAY[@]}; do

	BUILD_SOURCE="${BUILD_ARRAY[i]}"
	BUILDER="${!PACKAGES[i]:3:1}"
	BUILD_ARGS="${!PACKAGES[i]:4:1}"
	CONFIG="${!PACKAGES[i]:5:1}"
	SOURCE_NAME=${BUILD_SOURCE##*/}
	
	cd ${BUILD_SOURCE}
	
	for x in ${!BUILD_STAGE[@]}; do

		build_commands ${BUILD_STAGE[x]} ${BUILDER}
		error_check ${BUILD_STAGE[x]} ${SOURCE_NAME}
	
	done

done

}

error_check() {

	[ $? -eq 0 ] || {  echo "error during ${1} ${2}"; exit 1; }

}

build_yami() {

cd ${BUILD_DRIVERS_DIR}

git clone https://github.com/intel/libyami.git
cd libyami
git checkout 1.3.2
./autogen.sh
CFLAGS="-O2 -Wall -Wno-array-compare" CXXFLAGS="-O2 -Wall -Wno-array-compare" ./configure --prefix=$INSTALL_PATH --libdir=$LIBRARY_INSTALLATION_DIR $LIBYAMI_CONFIG
error_check "configuration" "libyami"

make clean
make -j"$((`nproc` - 1))"
error_check "make" "libyami"

sudo make install-strip
error_check "install" "libyami"

cd ${YAMI_INF_DIR}
./bootstrap
error_check "bootstrap" "yami_inf"

./configure --prefix=$INSTALL_PATH --libdir=$LIBRARY_INSTALLATION_DIR $LIBYAMI_INF_CONFIG
error_check "configuration" "yami_inf"

make clean
make -j"$((`nproc` - 1))"
error_check "make" "yami_inf"

sudo make install-strip
error_check "install" "yami_inf"

}

BUILD_STAGE=("configuration" "build" "install")
CUSTOM="true"
BUILD_DRIVERS_DIR=${SOURCE_DIR}/drivers_build
YAMI_INF_DIR=${SOURCE_DIR}/yami/omatic/yami_inf
INSTALL_PATH=/usr/local
LIBRARY_INSTALLATION_DIR=$INSTALL_PATH/lib/x86_64-linux-gnu

LIBDRM_CONFIG="-Dlibdir=$LIBRARY_INSTALLATION_DIR -Dradeon=disabled -Damdgpu=disabled -Dnouveau=disabled -Dvmwgfx=disabled"
LIBVA_CONFIG="-Ddriverdir=$LIBRARY_INSTALLATION_DIR -Dlibdir=$LIBRARY_INSTALLATION_DIR -Dwith_x11=yes -Dwith_wayland=no"
LIBVAUTILS_CONFIG="--libdir=$LIBRARY_INSTALLATION_DIR --enable-x11 --disable-wayland"
INTEL_GMMLIB_CONFIG="-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH -DCMAKE_INSTALL_LIBDIR=$LIBRARY_INSTALLATION_DIR"
INTEL_MEDIA_DRIVER_CONFIG="-DLIBVA_DRIVERS_PATH=$LIBRARY_INSTALLATION_DIR/dri -DCMAKE_INSTALL_LIBDIR=$LIBRARY_INSTALLATION_DIR -DCMAKE_PREFIX_PATH=$INSTALL_PATH -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH"
LIBYAMI_CONFIG="--disable-jpegdec --disable-vp8dec --disable-h265dec --enable-capi --enable-x11 --enable-mpeg2dec"
LIBYAMI_INF_CONFIG="--enable-x11"

LIBDRM=("lab|rel|get" "177" "2.4.124" "meson" "" "$LIBDRM_CONFIG")
LIBVA=("hub|rel|get" "intel/libva" "2.22.0" "meson" "" "$LIBVA_CONFIG")
LIBVAUTILS=("hub|rel|get" "intel/libva-utils" "2.22.0" "configure" "" "$LIBVAUTILS_CONFIG")
INTEL_MEDIA=("clone" "intel/media-driver" "25.1.4" "cmake" "libx11-xcb-dev libxcb-dri3-dev" "$INTEL_MEDIA_DRIVER_CONFIG")
INTEL_GMMLIB=("hub|tag|get" "intel/gmmlib" "22.7.0" "cmake" "libx11-xcb-dev libxcb-dri3-dev" "$INTEL_GMMLIB_CONFIG")

PACKAGES=(LIBDRM[@]
LIBVA[@]
LIBVAUTILS[@]
INTEL_GMMLIB[@]
INTEL_MEDIA[@])


for i in "$@"; do

case $i in

	--prefix=*)
	INSTALL_PATH="${i#*=}"
	shift # past argument=value
	;;

	--latest)
	CUSTOM="false"
	shift # past argument=value
	;;

	--disable-x11)
	LIBDRM_CONFIG="-Dlibdir=$LIBRARY_INSTALLATION_DIR -Dradeon=disabled -Damdgpu=disabled -Dnouveau=disabled -Dvmwgfx=disabled"
	LIBVA_CONFIG="-Ddriverdir=$LIBRARY_INSTALLATION_DIR -Dlibdir=$LIBRARY_INSTALLATION_DIR -Dwith_x11=yes -Dwith_wayland=no"
	LIBVAUTILS_CONFIG="--libdir=$LIBRARY_INSTALLATION_DIR --disable-x11 --disable-wayland"
	LIBYAMI_CONFIG="--disable-jpegdec --disable-vp8dec --disable-h265dec --enable-capi --disable-x11 --enable-mpeg2dec"
	LIBYAMI_INF_CONFIG=""
	shift # past argument=value
	;;

esac

done

echo "INSTALL_PATH                = $INSTALL_PATH"
echo "LIBDRM_CONFIG               = $LIBDRM_CONFIG"
echo "LIBVA_CONFIG                = $LIBVA_CONFIG"
echo "LIBVAUTILS_CONFIG           = $LIBVAUTILS_CONFIG"
echo "LIBYAMI_CONFIG              = $LIBYAMI_CONFIG"

export PKG_CONFIG_PATH="$LIBRARY_INSTALLATION_DIR/pkgconfig"
export NOCONFIGURE=1
mkdir -p $BUILD_DRIVERS_DIR
rm -rf ${SOURCE_DIR}/xrdp_build/* ${BUILD_DRIVERS_DIR}/*
cd $BUILD_DRIVERS_DIR

download_source
build_source
build_yami
