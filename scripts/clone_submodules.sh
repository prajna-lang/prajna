#! /bin/bash
set -e


# 下载子模块
git submodule init .
git submodule update --recursive $@
