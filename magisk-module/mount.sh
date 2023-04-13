MODDIR="${0%/*}"

set -o standalone

export MAGISKTMP="$(magisk --path)"

chmod 777 "$MODDIR/overlayfs_system"

OVERLAYDIR="/data/adb/overlay"
OVERLAYMNT="/mnt/overlay_system"

if [ ! -e "/mnt/vendor/system" ]; then
    OVERLAYMNT="/mnt/vendor/system"
fi

if [ -z "$MAGISKTMP" ];then
    # KernelSU
    MODULEMNT="/mnt/overlay_modules"
else
    MODULEMNT="$MAGISKTMP/overlay_modules"
fi

mv -fT /cache/overlayfs.log /cache/overlayfs.log.bak
rm -rf /cache/overlayfs.log
echo "--- Start debugging log ---" >/cache/overlayfs.log

mkdir -p "$OVERLAYMNT"
mkdir -p "$OVERLAYDIR"
mkdir -p "$MODULEMNT"

mount -t tmpfs tmpfs "$MODULEMNT"

loop_setup() {
  unset LOOPDEV
  local LOOP
  local MINORX=1
  [ -e /dev/block/loop1 ] && MINORX=$(stat -Lc '%T' /dev/block/loop1)
  local NUM=0
  while [ $NUM -lt 2048 ]; do
    LOOP=/dev/block/loop$NUM
    [ -e $LOOP ] || mknod $LOOP b 7 $((NUM * MINORX))
    if losetup $LOOP "$1" 2>/dev/null; then
      LOOPDEV=$LOOP
      break
    fi
    NUM=$((NUM + 1))
  done
}

if [ -f "$OVERLAYDIR" ]; then
    loop_setup /data/adb/overlay
    if [ ! -z "$LOOPDEV" ]; then
        mount -o rw -t ext4 "$LOOPDEV" "$OVERLAYMNT"
        ln "$LOOPDEV" /dev/block/overlayfs_loop
    fi
fi

if ! "$MODDIR/overlayfs_system" --test --check-ext4 "$OVERLAYMNT"; then
    echo "unable to mount writeable dir" >>/cache/overlayfs.log
    exit
fi

for i in /data/adb/modules/*; do
    [ ! -e "$i" ] && break;
    module_name="$(basename "$i")"
    if [ ! -e "$i/disable" ] && [ ! -e "$i/remove" ] && [ -f "$i/overlay.img" ]; then
        loop_setup "$i/overlay.img"
        if [ ! -z "$LOOPDEV" ]; then
            echo "mount overlayfs for module: $module_name" >>/cache/overlayfs.log
            mkdir -p "$MODULEMNT/$module_name"
            mount -o rw -t ext4 "$LOOPDEV" "$MODULEMNT/$module_name"
        fi
    fi
done

OVERLAYLIST=""

for i in "$MODULEMNT"/*; do
    [ ! -e "$i" ] && break;
    if "$MODDIR/overlayfs_system" --test --check-ext4 "$i"; then
        OVERLAYLIST="$i:$OVERLAYLIST"
    fi
done

mkdir -p "$OVERLAYMNT/upper"
mkdir -p "$OVERLAYMNT/worker"

rm -rf "$OVERLAYMNT/master"
mkdir -p "$OVERLAYMNT/master"

if [ ! -z "$OVERLAYLIST" ]; then
    export OVERLAYLIST="${OVERLAYLIST::-1}"
    echo "mount overlayfs list: [$OVERLAYLIST]" >>/cache/overlayfs.log
fi

# overlay_system <writeable-dir>
. "$MODDIR/mode.sh"
"$MODDIR/overlayfs_system" "$OVERLAYMNT" | tee -a /cache/overlayfs.log

if [ ! -z "$MAGISKTMP" ]; then
    mkdir -p "$MAGISKTMP/overlayfs_mnt"
    mount --bind "$OVERLAYMNT" "$MAGISKTMP/overlayfs_mnt"
fi

rm -rf /dev/.overlayfs_service_unblock
echo "--- Mountinfo (post-fs-data) ---" >>/cache/overlayfs.log
cat /proc/mounts >>/cache/overlayfs.log
(
    # block until /dev/.overlayfs_service_unblock
    while [ ! -e "/dev/.overlayfs_service_unblock" ]; do
        sleep 1
    done
    rm -rf /dev/.overlayfs_service_unblock
    if [ -z "$MAGISKTMP" ]; then
        # KernelSU
        umount -l "$MODULEMNT"
        rmdir "$MODULEMNT"
    fi
    umount -l "$OVERLAYMNT"
    rmdir "$OVERLAYMNT"

    echo "--- Mountinfo (late_start) ---" >>/cache/overlayfs.log
    cat /proc/mounts >>/cache/overlayfs.log
) &

