touch "${0%/*}/disable"
touch /dev/.overlayfs_service_unblock

. "${0%/*}/mode.sh"

if [ "$DO_UNMOUNT_KSU" == "true" ]; then
    "${0%/*}/overlayfs_system" --unmount-ksu
fi

while [ "$(getprop sys.boot_completed)" != 1 ]; do sleep 1; done
rm -rf "${0%/*}/disable"