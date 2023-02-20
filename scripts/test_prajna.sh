set -e

build_dir=build_$1

./$build_dir/bin/prajna exe tests/program/main.prajna
./$build_dir/bin/prajna exe tests/program/hello_world.prajnascript
