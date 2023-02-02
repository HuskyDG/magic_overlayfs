touch "${0%/*}/disable"

while [ "$(getprop sys.boot_completed)" != 1 ]; do sleep 1; done
rm -rf "${0%/*}/disable"
rm -rf "${0%/*}/service.sh"