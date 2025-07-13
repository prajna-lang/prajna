#! /bin/bash
set -e

build_dir=build_$1
cd $build_dir

threads=8
if [ "$(uname)" == "Darwin" ]; then
# Mac OS X 操作系统
    threads=$(sysctl -n hw.physicalcpu)
    cmake --build . --target install -j $threads
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ];then
# GNU/Linux操作系统
    threads=$(nproc)
    cmake --build . --target install -j $threads
else # Windows NT操作系统
    cmake --build . --target install -j $threads
fi
