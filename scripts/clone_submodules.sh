#! /bin/bash
set -e


# 下载子模块
git submodule update --init $@ .

pushd third_party/boost

git submodule update --init $@ tools/cmake

git submodule update --init $@ tools/cmake

pushd libs

git submodule update --init $@ \
algorithm \
align \
any \
array \
asio \
assert \
atomic \
bind \
chrono \
concept_check \
config \
container \
container_hash \
context \
conversion \
core \
coroutine \
date_time \
describe \
detail \
dll \
dynamic_bitset \
endian \
exception \
filesystem \
foreach \
function \
function_types \
functional \
fusion \
integer \
interprocess \
intrusive \
io \
iostreams \
iterator \
lexical_cast \
locale \
move \
mp11 \
mpl \
multiprecision \
numeric \
optional \
phoenix \
pool \
predef \
preprocessor \
process \
proto \
random \
range \
ratio \
regex \
scope \
scope_exit \
smart_ptr \
sort \
spirit \
static_assert \
static_string \
system \
thread \
throw_exception \
timer \
tokenizer \
tuple \
type_index \
type_traits \
typeof \
unordered \
utility \
uuid \
variant \
variant2 \
winapi \
serialization \
multi_index \
property_tree \

popd
popd
