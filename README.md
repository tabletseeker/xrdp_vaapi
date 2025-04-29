![o,age](https://i.postimg.cc/2ys32VSp/Untitled.png)

Automatically builds xrdp (remote desktop server) with intel va-api hardware acceleration support and sets up a working VM environment for Debian/Ubuntu. \
Intended for use with [i915-sriov-dkms](https://github.com/strongtz/i915-sriov-dkms) in libvirt/qemu virtual machines, but also applicable otherwise.
 
## Usage
* Clone xrdp_vaapi repo:
```
git clone https://github.com/tabletseeker/xrdp_vaapi -b main
```
* Enter directory:
```
cd xrdp_vaapi
```
* Execute:
```
bash xrdp_vaapi.sh
```

### Arguments & Env Variables
|  xrdp_vaapi.sh                                              | Description                                          | Value                                                                                      
| ---------------------------------------------------- | ------------------------------------------------ | ------------------------------------------------|
| DRIVER_NAME         | LIBVA_DRIVER_NAME Env Variable                 | default: iHD |
| BUILD_DIR         |  Build source directory              | default: $PWD |
| --sriov \| -s      |  Build and install i915_sriov_dkms      | default: false |


|  buildyami.sh                                             | Description                                          | Value                                                                                          
| ---------------------------------------------------- | ------------------------------------------------ | ------------------------------------------------|
| --prefix          | Build install directory       | default: /usr/local
| --disable-x11      | Build with X11 disabled  		      | default: off |
| --latest      | Automatically find the latest versions | default: off |
* If `--latest` is not used, the version numbers stated in `xrdp_vaapi/yami/omatic/buildyami.sh` lines 243 - 247 will apply.


### Sources
Debian/Ubuntu users must ensure a complete `/etc/apt/sources.list` for all packages to be installed successfully.
- Debian
```
deb https://deb.debian.org/debian bookworm main contrib non-free non-free-firmware
deb-src https://deb.debian.org/debian bookworm main contrib non-free non-free-firmware
deb https://deb.debian.org/debian trixie main contrib non-free non-free-firmware
deb-src https://deb.debian.org/debian trixie main contrib non-free non-free-firmware
```
- Ubuntu
```
deb http://de.archive.ubuntu.com/ubuntu noble main restricted universe multiverse
deb-src http://de.archive.ubuntu.com/ubuntu noble main restricted universe multiverse
```
