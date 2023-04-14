MODDIR="${0%/*}"
rm -rf /data/adb/overlay.img.xz
cp -af "$MODDIR/overlay.xz" /data/adb/overlay.img.xz
exec nsenter --mount=/proc/1/ns/mnt "$MODDIR/busybox" sh "$MODDIR/mount.sh"