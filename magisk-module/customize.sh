SKIPUNZIP=1
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
    if ! /system/bin/mkfs.ext4 /data/adb/overlay; then
        rm -rf /data/adb/overlay
        abort "! Setup ext4 image failed"
    fi
fi

ui_print "- Extract files"

unzip -oj "$ZIPFILE" post-fs-data.sh service.sh util_functions.sh mode.sh mount.sh uninstall.sh "libs/$ABI/overlayfs_system" "libs/$ABI/busybox" -d "$MODPATH"

ui_print "- Setup module"

chmod 777 "$MODPATH/overlayfs_system" "$MODPATH/busybox"

ui_print

ui_print "* OverlayFS is locked as read-only as default"
ui_print "* Modify mode.sh to change mode of OverlayFS"
ui_print "* OverlayFS upper loop is /dev/block/overlayfs_loop" 
ui_print "* On Magisk, OverlayFS upper loop are mounted at"
ui_print "*  \$(magisk --path)/overlayfs_mnt"

ui_print
