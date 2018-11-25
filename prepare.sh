#!/bin/sh

git submodule init
git submodule update

cd esp-toolchain/
git submodule init
git submodule update

cd tools
./get.py
