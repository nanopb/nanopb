#!/usr/bin/env bash

sudo apt-get -y update
sudo apt-get -y install python3-protobuf protobuf-compiler #scons splint valgrind
cd examples/simple
make
