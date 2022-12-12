# set -e

build_configure=$1
shift 1

cmake . \
-B build_${build_configure} -DCMAKE_BUILD_TYPE=${build_configure} -DCMAKE_INSTALL_PREFIX=build_${build_configure}/install \
-DBOOST_INCLUDE_LIBRARIES="algorithm;variant;optional;fusion;spirit;multiprecision" \
-DLLVM_OPTIMIZED_TABLEGEN=ON \
$@
