MODDIR="${0%/*}"

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

loop_setup /data/adb/overlay.img

mkdir -p /mnt/overlay_system

if [ ! -z "$LOOPDEV" ]; then
    mount  -o rw -t ext4 "$LOOPDEV" /mnt/overlay_system
elif [ -d /data/adb/overlay ]; then
    mount --bind "/data/adb/overlay" /mnt/overlay_system
else
    exit
fi
# overlay_system <writeable-dir> <mirror>
chmod 777 "$MODDIR/overlayfs_system"
"$MODDIR/overlayfs_system" /mnt/overlay_system "$(magisk --path)/.magisk/mirror"
umount -l /mnt/overlay_system
rmdir /mnt/overlay_system