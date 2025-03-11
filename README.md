![o,age](https://i.postimg.cc/zXrvRN2s/Untitled.png)

Automatically builds xrdp with intel va-api hardware acceleration support and sets up a working VM environment.
Intended for use with [i915-sriov-dkms](https://github.com/strongtz/i915-sriov-dkms) in libvirt/qemu virtual machines, but also usable otherwise.
 
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
bash xrdp_vaapi.sh | tee build.log
```
