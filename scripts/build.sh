#! /bin/bash
set -e

build_dir=build_$1
cd $build_dir

threads=4
if [ "$(uname)" == "Darwin" ]; then
# Mac OS X 操作系统
    threads=$(sysctl -n hw.physicalcpu)
    make $2 -j $threads
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ];then
# GNU/Linux操作系统
    threads=$(nproc)
    make $2 -j $threads
else
# Windows NT操作系统
    threads=8
    cmake --build . --target install 
fi