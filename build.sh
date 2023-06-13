#!/usr/bin/env bash

set -euo pipefail

build_mode="${1:-release}"

cd "$(dirname "$0")"

ANDROID_NDK_HOME=./android-ndk-r23b
export PATH=${PATH}:${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin

rm -rf out
mkdir -p out
cp -af magisk-module out
cp -af README.md out/magisk-module
mkdir -p out/magisk-module/libs

( cat << EOF
arm64-v8a aarch64-linux-android31-clang++
armeabi-v7a armv7a-linux-androideabi31-clang++
x86 i686-linux-android31-clang++
x86_64 x86_64-linux-android31-clang++
EOF
) | while read line; do
    ARCH="$(echo $line | awk '{ print $1 }')"
    CXX="$(echo $line | awk '{ print $2 }')"
    if [ ! -z "$ARCH" ]; then
        mkdir "out/magisk-module/libs/${ARCH}"
        ${CXX} \
    native/jni/main.cpp \
    native/jni/logging.cpp native/jni/utils.cpp native/jni/mountinfo.cpp \
    -static \
    -std=c++17 \
    -o "out/magisk-module/libs/${ARCH}/overlayfs_system"
    fi
done

zip -r9 out/magisk-module-release.zip out/magisk-module