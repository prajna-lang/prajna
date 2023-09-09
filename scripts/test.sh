#! /bin/bash
set -e

build_dir=build_$1

if [ "$(uname)" == "Darwin" ]; then
# Mac OS X 操作系统
    ./$build_dir/bin/prajna_compiler_tests $@
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ];then
# GNU/Linux操作系统
    ./$build_dir/bin/prajna_compiler_tests $@
else
# Windows NT操作系统
    ./$build_dir/bin/Release/prajna_compiler_tests $@
fi