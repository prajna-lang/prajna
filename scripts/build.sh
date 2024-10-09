#! /bin/bash
set -e

build_dir=build_$1
cd $build_dir

threads=4
if [ "$(uname)" == "Darwin" ]; then
# Mac OS X 操作系统
    threads=$(sysctl -n hw.physicalcpu)
    ninja install -j $threads
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ];then
# GNU/Linux操作系统
    threads=$(nproc)
    ninja install -j $threads
else
# Windows NT操作系统
    threads=8
    cmake --build . --target install  --config Release # windows系统需要自己修改下配置， 外面的测试是不起作用的
fi
