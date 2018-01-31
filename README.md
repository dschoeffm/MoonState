# MoonState

This should (at some point) be a high-performance DTLS state-machine.

## Build Instructions

In order to build MoonState you need the following programs and libraries:
* cmake
* make
* gcc / clang
* g++ / clang++
* dpdk (see cmake options)
* libnuma

MoonState uses CMake as a build system.

The most simple build command is the following:
```
mkdir build
cd build
cmake ..
make -j
```

You may want or need to give cmake more options:
```
DPDK_INCLUDE_DIR=<path-to-DPDK-headers>
DPDK_LIB_DIR=<path-to-DPDK-libs>
WITH_PCAP=x // If this is defined, pcap is needed
```
