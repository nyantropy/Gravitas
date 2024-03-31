#!/bin/bash

mkdir release

cd src/decode_video
make

cd ../encode_video
make

cd ../transcode
make
