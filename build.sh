#!/usr/bin/env bash

set -euo pipefail

build_mode="${1:-release}"

cd "$(dirname "$0")"

rm -rf ./native/jni/libcxx
git clone http://github.com/huskydg/libcxx ./native/jni/libcxx

pushd native
rm -fr libs obj
debug_mode=1
if [[ "$build_mode" == "release" ]]; then
    debug_mode=0
fi
ndk-build -j4 NDK_DEBUG=$debug_mode
popd

rm -rf out
mkdir -p out
cp -af magisk-module out
mv -fT native/libs out/magisk-module/libs
zip -r9 out/magisk-module-release.zip out/magisk-module