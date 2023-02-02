SKIPUNZIP=1

if [ "$BOOTMODE" ] && [ "$KSU" ]; then
    ui_print "- Installing from KernelSU app"
    ui_print "- KernelSU version: $KSU_KERNEL_VER_CODE (kernel) + $KSU_VER_CODE (ksud)"
    ui_print "- Please note that KernelSU modules mount will make"
    ui_print "  your system partitions unable to mount as rw"
    ui_print "- If you are using KernelSU, "
    ui_print "  please unmount all ksu overlayfs"
    ui_print "  when you want to modify system partitions"
elif [ "$BOOTMODE" ] && [ "$MAGISK_VER_CODE" ]; then
    ui_print "- Installing from Magisk app"
else
    ui_print "*********************************************************"
    ui_print "! Install from recovery is not supported"
    ui_print "! Please install from KernelSU or Magisk app"
    abort    "*********************************************************"
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

unzip -oj "$ZIPFILE" post-fs-data.sh \
                     service.sh \
                     util_functions.sh \
                     mode.sh \
                     mount.sh \
                     uninstall.sh \
                     module.prop \
                     "libs/$ABI/overlayfs_system" \
                     -d "$MODPATH"
unzip -oj "$ZIPFILE" util_functions.sh  -d "/data/adb/modules/${MODPATH##*/}"

ui_print "- Setup module"

chmod 777 "$MODPATH/overlayfs_system"

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
    # ensure that uppderdir does not override my binary
    rm -rf "$randdir/upper/system/bin/overlayfs_system" \
           "$randdir/upper/system/bin/magic_remount_rw" \
           "$randdir/upper/system/bin/magic_remount_ro"
    umount -l "$randdir"
    return $result_mnt
}

create_ext4_image() {
    dd if=/dev/zero of="$1" bs=1024 count=100
    /system/bin/mkfs.ext4 "$1" && return 0
    return 1
}

if [ ! -f "/data/adb/overlay" ] || ! test_mount_image; then
    rm -rf "/data/adb/overlay"
    ui_print "- Setup 2GB ext4 image at /data/adb/overlay"
    ui_print "  Please wait..."
    if ! create_ext4_image "/data/adb/overlay" || ! resize_img "/data/adb/overlay" 2000M || ! test_mount_image; then
        rm -rf /data/adb/overlay
        abort "! Setup ext4 image failed, abort"
    fi
fi

mkdir -p "$MODPATH/system/bin"
chcon -R u:object_r:system_file:s0 "$MODPATH/system"
chmod -R 755 "$MODPATH/system"

cp -af "$MODPATH/overlayfs_system" "$MODPATH/system/bin"
ln -s "./overlayfs_system" "$MODPATH/system/bin/magic_remount_rw"
ln -s "./overlayfs_system" "$MODPATH/system/bin/magic_remount_ro"

if [ "$BOOTMODE" ] && [ -z "$KSU" ]; then
    . "$MODPATH/util_functions.sh"
    support_overlayfs && rm -rf "$MODPATH/system"
fi

ui_print
