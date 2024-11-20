#! /bin/bash
set -e

build_dir=build_$1
cd $build_dir

threads=8
if [ "$(uname)" == "Darwin" ]; then
# Mac OS X 操作系统
    threads=$(sysctl -n hw.physicalcpu)
    ninja -j $threads
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ];then
# GNU/Linux操作系统
    threads=$(nproc)
    ninja -j $threads
else # Windows NT操作系统
    ninja -j $threads
fi
