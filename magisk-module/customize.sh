SKIPUNZIP=1

loop_setup() {
  unset LOOPDEV
  local LOOP
  local MINORX=1
  [ -e /dev/block/loop1 ] && MINORX=$(stat -Lc '%T' /dev/block/loop1)
  local NUM=0
  while [ $NUM -lt 64 ]; do
    LOOP=/dev/block/loop$NUM
    [ -e $LOOP ] || mknod $LOOP b 7 $((NUM * MINORX))
    if losetup $LOOP "$1" 2>/dev/null; then
      LOOPDEV=$LOOP
      break
    fi
    NUM=$((NUM + 1))
  done
}


if ! $BOOTMODE; then
    abort "! Install from Recovery is not supported"
fi

ABI="$(getprop ro.product.cpu.abi)"

# Fix ABI detection
if [ "$ABI" == "armeabi-v7a" ]; then
  ABI32=armeabi-v7a
elif [ "$ABI" == "arm64" ]; then
  ABI32=armeabi-v7a
elif [ "$ABI" == "x86" ]; then
  ABI32=x86
elif [ "$ABI" == "x64" ] || [ "$ABI" == "x86_64" ]; then
  ABI=x86_64
  ABI32=x86
fi

unzip -oj "$ZIPFILE" "libs/$ABI/overlayfs_system" -d "$TMPDIR" 1>&2
chmod 777 "$TMPDIR/overlayfs_system"

if ! $TMPDIR/overlayfs_system --test; then
    ui_print "! Kernel doesn't support overlayfs, are you sure?"
    abort
fi

unzip -o "$ZIPFILE" module.prop -d "$MODPATH" 1>&2

if [ ! -f "/data/adb/overlay" ]; then
    rm -rf "/data/adb/overlay"
    ui_print "- Create 2GB ext4 loop image..."
    dd if=/dev/zero of=/data/adb/overlay bs=1024 count=2000000
    /system/bin/mkfs.ext4 /data/adb/overlay
fi

unzip -oj "$ZIPFILE" post-fs-data.sh service.sh util_functions.sh mode.sh mount.sh "libs/$ABI/overlayfs_system" "libs/$ABI/busybox" -d "$MODPATH" 1>&2

chmod 777 "$MODPATH/overlayfs_system" "$MODPATH/busybox"
