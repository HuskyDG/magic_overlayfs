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

ui_print "- Test mounting..."
is_support=false
test_dir="/data/.__$(head -c15 /dev/urandom | base64)"
mkdir -p "$test_dir/lower"
mkdir -p "$test_dir/upper"
mkdir -p "$test_dir/worker"
opts="lowerdir=$test_dir/lower,upperdir=$test_dir/upper,workdir=$test_dir/worker"
mount -t overlay -o "$opts" overlay "$test_dir/lower"
if mount | grep " $test_dir/lower " | grep -q " overlay "; then
    is_support=true
    umount -l "$test_dir/lower"
fi
rm -rf "$test_dir"


if $is_support; then
    ui_print "- Directly use folder in /data"
    // clone dummy
else
    ui_print "! Cannot directly use /data"
    ui_print "- Create 1,5GB (enough?) loop device..."
    rm -rf /data/adb/overlay.img
    dd if=/dev/zero of=/data/adb/overlay.img bs=1000000 count=1500
    /system/bin/mkfs.ext4 /data/adb/overlay.img
    loop_setup /data/adb/overlay.img
    if [ -z "$LOOPDEV" ]; then
        rm -rf /data/adb/overlay.img
        abort "! Setup failed"
    fi
    mkdir /data/adb/overlay
    mount -o rw -t ext4 "$LOOPDEV" /data/adb/overlay
fi

{
    ui_print "- Clone dummy skeletion for /system ..."
    mkdir -p /data/adb/overlay/upper/system
    magisk --clone-attr /system /data/adb/overlay/upper/system
    find /system/ -type d | while read line; do
        mkdir "/data/adb/overlay/upper$line"
        magisk --clone-attr "$line" "/data/adb/overlay/upper$line"
    done
    for part in /vendor /product /system_ext; do
        mountpoint -q $part || continue
        ui_print "- Clone dummy skeleton for $part ..."
        mkdir -p /data/adb/overlay/upper$part
        magisk --clone-attr $part /data/adb/overlay/upper$part
        find $part/ -type d | while read line; do
            mkdir "/data/adb/overlay/upper$line"
            magisk --clone-attr "$line" "/data/adb/overlay/upper$line"
        done
    done
    unzip -oj "$ZIPFILE" "libs/$ABI/overlayfs_system" uninstall.sh post-fs-data.sh -d "$MODPATH" 1>&2
}

if ! $is_support; then
    umount -l /data/adb/overlay
    rmdir /data/adb/overlay
fi

chmod 777 "$MODPATH/overlayfs_system"
