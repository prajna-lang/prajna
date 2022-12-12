#! /bin/bash

set -e

build_dir=build_$1

./$build_dir/bin/prajna_compiler_tests $@
