MODDIR="${0%/*}"

OVERLAYDIR="/data/adb/overlay"
OVERLAYMNT="/mnt/overlay_system"

mv -fT /cache/overlayfs.log /cache/overlayfs.log.bak
rm -rf /cache/overlayfs.log
echo "--- Start debugging log ---" >/cache/overlayfs.log

mkdir -p "$OVERLAYMNT"
mkdir -p "$OVERLAYDIR"

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

if [ -d "$OVERLAYDIR" ]; then
    mount --bind "$OVERLAYDIR" "$OVERLAYMNT"
elif [ -f "$OVERLAYDIR" ]; then
    loop_setup /data/adb/overlay
    [ -z "$LOOPDEV" ] || mount -o rw -t ext4 "$LOOPDEV" "$OVERLAYMNT"
else
    echo "unable to mount writeable dir" >>/cache/overlayfs.log
    exit
fi
# overlay_system <writeable-dir> <magisk-mirror>
chmod 777 "$MODDIR/overlayfs_system"

MAGISKTMP="$(magisk --path)"

if [ -z "$MAGISKTMP" ]; then
    # KernelSU
    "$MODDIR/overlayfs_system" "$OVERLAYMNT" | tee -a /cache/overlayfs.log
else
    "$MODDIR/overlayfs_system" "$OVERLAYMNT" "$MAGISKTMP/.magisk/mirror" | tee -a /cache/overlayfs.log
fi
    
umount -l "$OVERLAYMNT"
rmdir "$OVERLAYMNT"

echo "--- Mountinfo ---" >>/cache/overlayfs.log
cat /proc/mounts >>/cache/overlayfs.log
