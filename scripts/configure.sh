#! /bin/bash
set -e

build_configure=$1
shift 1

CXX=clang++ CC=clang cmake . \
-B build_${build_configure} \
-DCMAKE_BUILD_TYPE=${build_configure} \
-DCMAKE_INSTALL_PREFIX=build_${build_configure}/install \
-DBOOST_INCLUDE_LIBRARIES="algorithm;variant;optional;fusion;spirit;multiprecision;process;dll" \
-DLLVM_INCLUDE_TESTS=OFF \
-DLLVM_INCLUDE_BENCHMARKS=OFF \
-DLLVM_OPTIMIZED_TABLEGEN=ON \
-DLLVM_ENABLE_PROJECTS="compiler-rt" \
-DLLVM_ENABLE_ABI_BREAKING_CHECKS=OFF -DLLVM_ENABLE_RTTI=ON -DBUILD_SHARED_LIBS=OFF \
$@
