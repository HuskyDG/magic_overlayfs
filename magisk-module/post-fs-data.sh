MODDIR="${0%/*}"

chmod 777 "$MODDIR/overlayfs_system"

OVERLAYDIR="/data/adb/overlay"
OVERLAYMNT="/mnt/overlay_system"
MODULEMNT="/mnt/overlay_modules"

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

if [ -d "$OVERLAYDIR" ]; then
    mount --bind "$OVERLAYDIR" "$OVERLAYMNT"
elif [ -f "$OVERLAYDIR" ]; then
    loop_setup /data/adb/overlay
    [ -z "$LOOPDEV" ] || mount -o rw -t ext4 "$LOOPDEV" "$OVERLAYMNT"
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

rm -rf "$OVERLAYMNT/.module_work"
mkdir -p "$OVERLAYMNT/.module_work"

if [ ! -z "$OVERLAYLIST" ]; then
    OVERLAYLIST="${OVERLAYLIST::-1}"
    echo "mount overlayfs list: [$OVERLAYLIST]" >>/cache/overlayfs.log
    mount -t overlay -o lowerdir="$OVERLAYLIST",upperdir="$OVERLAYMNT/upper",workdir="$OVERLAYMNT/.module_work" overlay "$OVERLAYMNT/upper"
fi

# overlay_system <writeable-dir> <magisk-mirror>

MAGISKTMP="$(magisk --path)"

if [ -z "$MAGISKTMP" ]; then
    # KernelSU
    "$MODDIR/overlayfs_system" "$OVERLAYMNT" | tee -a /cache/overlayfs.log
else
    "$MODDIR/overlayfs_system" "$OVERLAYMNT" "$MAGISKTMP/.magisk/mirror" | tee -a /cache/overlayfs.log
fi
    
umount -l "$OVERLAYMNT"
umount -l "$MODULEMNT"
rmdir "$OVERLAYMNT"
rmdir "$MODULEMNT"

echo "--- Mountinfo ---" >>/cache/overlayfs.log
cat /proc/mounts >>/cache/overlayfs.log
