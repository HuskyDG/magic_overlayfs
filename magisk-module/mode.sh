# OverlayFS Mode
# 0 - read-only but can still remount as read-write
# 1 - read-write default
# 2 - read-only locked (cannot remount as read-write)
# You can set to 2 after modify system partititons to avoid detection
export OVERLAY_MODE=0

# Set to true to enable legacy mode that mount overlayfs on subdirectories instead of root partititons
export OVERLAY_LEGACY_MOUNT=false

# If you are using KernelSU, set this to true to unmount KernelSU overlayfs
export DO_UNMOUNT_KSU=false