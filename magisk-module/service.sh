touch "${0%/*}/disable"
touch /dev/.overlayfs_service_unblock

while [ "$(getprop sys.boot_completed)" != 1 ]; do sleep 1; done
rm -rf "${0%/*}/disable"