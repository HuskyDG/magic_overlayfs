touch "${0%/*}/disable"
touch /dev/.overlayfs_service_unblock

. "${0%/*}/mode.sh"

while [ "$(getprop sys.boot_completed)" != 1 ]; do sleep 1; done
rm -rf "${0%/*}/disable"