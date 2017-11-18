#!/bin/sh
git submodule init
git submodule update doxygen-bootstrapped
doxygen 1> /dev/null
