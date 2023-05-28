MODDIR="${0%/*}"

set -o standalone

export MAGISKTMP="$(magisk --path)"

chmod 777 "$MODDIR/overlayfs_system"

OVERLAYDIR="/data/adb/overlay"
OVERLAYMNT="/mnt/overlay"
MODULEMNT="/mnt/loop"

if [ ! -e "/mnt/vendor/system" ]; then
    OVERLAYMNT="/mnt/vendor/system"
fi


mv -fT /cache/overlayfs.log /cache/overlayfs.log.bak
rm -rf /cache/overlayfs.log
echo "--- Start debugging log ---" >/cache/overlayfs.log
echo "init mount namespace: $(readlink /proc/1/ns/mnt)" >>/cache/overlayfs.log
echo "current mount namespace: $(readlink /proc/self/ns/mnt)" >>/cache/overlayfs.log

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

num=0

for i in /data/adb/modules/*; do
    [ ! -e "$i" ] && break;
    module_name="$(basename "$i")"
    if [ ! -e "$i/disable" ] && [ ! -e "$i/remove" ]; then
        if [ -f "$i/overlay.img" ]; then
            loop_setup "$i/overlay.img"
            if [ ! -z "$LOOPDEV" ]; then
                echo "mount overlayfs for module: $module_name" >>/cache/overlayfs.log
                mkdir -p "$MODULEMNT/$num"
                mount -o rw -t ext4 "$LOOPDEV" "$MODULEMNT/$num"
            fi
            num="$((num+1))"
        fi
        if [ "$KSU" == "true" ]; then
            mkdir -p "$MODULEMNT/$num"
            mount --bind "$i" "$MODULEMNT/$num"
            num="$((num+1))"
        fi
    fi
done

OVERLAYLIST=""

for i in "$MODULEMNT"/*; do
    [ ! -e "$i" ] && break;
    if [ -d "$i" ] && [ ! -L "$i" ] && "$MODDIR/overlayfs_system" --test --check-ext4 "$i"; then
        OVERLAYLIST="$i:$OVERLAYLIST"
    fi
done

mkdir -p "$OVERLAYMNT/upper"
rm -rf "$OVERLAYMNT/worker"
mkdir -p "$OVERLAYMNT/worker"

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


umount -l "$OVERLAYMNT"
rmdir "$OVERLAYMNT"
umount -l "$MODULEMNT"
rmdir "$MODULEMNT"

rm -rf /dev/.overlayfs_service_unblock
echo "--- Mountinfo (post-fs-data) ---" >>/cache/overlayfs.log
cat /proc/mounts >>/cache/overlayfs.log
(
    # block until /dev/.overlayfs_service_unblock
    while [ ! -e "/dev/.overlayfs_service_unblock" ]; do
        sleep 1
    done
    rm -rf /dev/.overlayfs_service_unblock

    echo "--- Mountinfo (late_start) ---" >>/cache/overlayfs.log
    cat /proc/mounts >>/cache/overlayfs.log
) &

