#! /bin/bash

set -e

threads=4
if [ "$(uname)" == "Darwin" ]; then
# Mac OS X 操作系统
    threads=$(sysctl -n hw.physicalcpu)
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ];then
    threads=$(nproc)
# GNU/Linux操作系统
elif [ "$(expr substr $(uname -s) 1 10)" == "MINGW32_NT" ];then
# Windows NT操作系统
    threads=8
fi

build_dir=build_$1
cd $build_dir
make  -j $threads $2
cd ..
