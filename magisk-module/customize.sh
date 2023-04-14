SKIPUNZIP=1
if ! $BOOTMODE; then
    abort "! Install from Recovery is not supported"
fi


loop_setup() {
  unset LOOPDEV
  local LOOP
  local MINORX=1
  [ -e /dev/block/loop1 ] && MINORX=$(stat -Lc '%T' /dev/block/loop1)
  local NUM=0
  while [ $NUM -lt 1024 ]; do
    LOOP=/dev/block/loop$NUM
    [ -e $LOOP ] || mknod $LOOP b 7 $((NUM * MINORX))
    if losetup $LOOP "$1" 2>/dev/null; then
      LOOPDEV=$LOOP
      break
    fi
    NUM=$((NUM + 1))
  done
}

randdir="$TMPDIR/.$(head -c21 /dev/urandom | base64)"
mkdir -p "$randdir"

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


ui_print "- Extract files"

unzip -oj "$ZIPFILE" overlay.xz \
                     post-fs-data.sh \
                     service.sh \
                     util_functions.sh \
                     mode.sh \
                     mount.sh \
                     uninstall.sh \
                     module.prop \
                     "libs/$ABI/overlayfs_system" \
                     "libs/$ABI/busybox" \
                     -d "$MODPATH"
rm -rf /data/adb/overlay.xz
unzip -oj "$ZIPFILE" overlay.xz -d "/data/adb"
unzip -oj "$ZIPFILE" util_functions.sh  -d "/data/adb/modules/${MODPATH##*/}"

ui_print "- Setup module"

chmod 777 "$MODPATH/overlayfs_system" "$MODPATH/busybox"

resize_img() {
    e2fsck -pf "$1" || return 1
    if [ "$2" ]; then
        resize2fs "$1" "$2" || return 1
    else
        resize2fs -M "$1" || return 1
    fi
    return 0
}

test_mount_image() {
    loop_setup /data/adb/overlay
    [ -z "$LOOPDEV" ] && return 1
    result_mnt=1
    mount -t ext4 -o rw "$LOOPDEV" "$randdir" && \
    "$MODPATH/overlayfs_system" --test --check-ext4 "$randdir" && result_mnt=0
    umount -l "$randdir"
    return $result_mnt
}
    

if [ ! -f "/data/adb/overlay" ] || ! test_mount_image; then
    rm -rf "/data/adb/overlay"
    ui_print "- Setup 2GB ext4 image at /data/adb/overlay"
    ui_print "  Please wait..."
    if ! xz -kd "/data/adb/overlay.xz" || ! resize_img "/data/adb/overlay" 2000M || ! test_mount_image; then
        rm -rf /data/adb/overlay.xz
        abort "! Setup ext4 image failed, abort"
    fi
fi

mkdir -p "$MODPATH/system/bin"
cp -af "$MODPATH/overlayfs_system" "$MODPATH/system/bin"
ln -s "./overlayfs_system" "$MODPATH/system/bin/magic_remount_rw"
ln -s "./overlayfs_system" "$MODPATH/system/bin/magic_remount_ro"

. "$MODPATH/util_functions.sh"
support_overlayfs && rm -rf "$MODPATH/system"

ui_print

ui_print " IMPORTANT! PLEASE READ!"
ui_print "* OverlayFS is mounted read-only by default"
ui_print "* You can modify mode.sh to change mode of OverlayFS"
ui_print "* OverlayFS upper loop device will be setup at: "
ui_print "*   /dev/block/overlayfs_loop" 
ui_print "* On Magisk, OverlayFS upper loop image will be mounted at:"
ui_print "*   \$(magisk --path)/overlayfs_mnt"

ui_print
