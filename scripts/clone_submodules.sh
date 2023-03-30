#! /bin/bash
set -e


# 下载子模块
git submodule init .
git submodule update  $@

# 下载boost的子模块
cd third_party/boost
git submodule init .
git submodule update $@
