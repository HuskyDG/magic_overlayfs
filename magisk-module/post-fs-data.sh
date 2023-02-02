MODDIR="${0%/*}"
mkdir -p /mnt/overlay_system
mount --bind "/data/adb/overlay" /mnt/overlay_system
# overlay_system <writeable-dir> <mirror>
chmod 777 "$MODDIR/overlayfs_system"
"$MODDIR/overlayfs_system" /mnt/overlay_system "$(magisk --path)/.magisk/mirror"
umount -l /mnt/overlay_system
rmdir /mnt/overlay_system