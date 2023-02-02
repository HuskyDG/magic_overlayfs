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

if ! (echo "$MAGISK_VER" | grep -q "\-delta"); then
    abort "! Please install latest Magisk Delta"
fi

if [ "$MAGISK_VER_CODE" -lt 25206 ]; then
    abort "! Please install Magisk Delta 25206+"
fi

MAGISKTMP="$(magisk --path)"
if [ ! -d "$MAGISKTMP/.magisk/mirror/early-mount" ]; then
    abort "! Unable to find early-mount.d"
fi

mkdir -p "$MAGISKTMP/.magisk/mirror/early-mount.d/initrc.d"

unzip -o "$ZIPFILE" module.prop -d "$MODPATH" 1>&2

CURRENTDIR="$(readlink "$MAGISKTMP/.magisk/mirror/early-mount" | sed "s/^.//g")"

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
    unzip -oj "$ZIPFILE" "libs/$ABI/overlayfs_system" overlayfs_mount.rc -d "$MAGISKTMP/.magisk/mirror/early-mount/initrc.d" 1>&2
    ui_print "- To uninstall overlayfs, remove these files:"
    ui_print "  /data/adb/overlay"
    ui_print "  $CURRENTDIR/initrc.d/overlay_system"
    ui_print "  $CURRENTDIR/initrc.d/overlay_mount.rc"
else
    abort "! Your data partition cannot be used as upperdir"
fi
chmod 777 "$MAGISKTMP/.magisk/mirror/early-mount/initrc.d/overlayfs_system"

ui_print "- This is not a module and will not show in Magisk app"

touch "$MODPATH/remove"
