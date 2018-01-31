# MoonState

The intention of MoonState is, to provide a framework for the creation of high
performance state-machines.
Primarily it is intended for the use of network protocols in a high-performance setting.
MoonState is build from the ground up to accommodate the lack of state awareness in
[MoonGen](https://github.com/dschoeffm/MoonGen)
/
[libmoon](https://github.com/dschoeffm/libmoon)
(created by [Paul Emmerich](https://github.com/emmericp))

It's design follows this figure:

![Overview](https://raw.githubusercontent.com/dschoeffm/MoonState/master/doc/overview.svg?sanitize=true)

The processing is done as follows:
1. Receive a packet from MoonGen (via C interface)
1. Hand the packet to an *Identifier*, which uniquely identifies each connection
1. Look up the flow inside a *State Table*
1. Extract the *State* data (a *void ptr* and a *StateID*)
1. Query the *Function Table* for the *StateID*, to retrieve the *Function*
1. Run the *Function* (arguments are the *void ptr*, the packet, and an interface)

The user needs to supply a suitable *Identifier* (a generic 5-Tuple identifier exists)
as well as the IDs of valid states and the *Functions* to run for the given states.

A minimal example of how to use MoonState can be seen in ``tests/simple.cpp``.

## Build Instructions

In order to build MoonState you need the following programs and libraries:
* cmake
* make
* g++ / clang++ (c++14 compliant)
* dpdk (see cmake options)
* libnuma

The simplest build is as follows:
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
