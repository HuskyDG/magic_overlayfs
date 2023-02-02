#!/usr/bin/env bash

set -euo pipefail

build_mode="${1:-release}"

cd "$(dirname "$0")"

plugin() {
rm -rf "$1"
git clone "$2" "$1"
}
#      <DIRECTORY>                       <PLUG-IN LINK>
plugin ./native/jni/libcxx               http://github.com/huskydg/libcxx || exit 1
plugin ./native/jni/external/selinux     https://github.com/topjohnwu/selinux || exit 1
plugin ./native/jni/external/pcre        https://android.googlesource.com/platform/external/pcre || exit 1

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