set -e

build_dir=build_$1

./$build_dir/bin/prajna exe examples/hello_world.prajna
./$build_dir/bin/prajna exe examples/hello_world.prajnascript
