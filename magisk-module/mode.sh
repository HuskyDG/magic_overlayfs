# 0 - read-only but can still remount as read-write
# 1 - read-write default
# 2 - read-only locked (cannot remount as read-write)

export OVERLAY_MODE=0

# If you are using KernelSU, set this to true to unmount KernelSU overlayfs
export DO_UNMOUNT_KSU=false