MODDIR="${0%/*}"
exec nsenter --mount=/proc/1/ns/mnt "$MODDIR/busybox" sh "$MODDIR/mount.sh"