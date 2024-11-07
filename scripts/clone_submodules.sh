#! /bin/bash
set -e


# 下载子模块
git submodule update --init --recursive $@ .
git submodule update --init --recursive $@ xeus_prajna/third_party

