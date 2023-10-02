# OverlayFS Mode
# 0 - read-only but can still remount as read-write
# 1 - read-write default
# 2 - read-only locked (cannot remount as read-write)
# You can set to 2 after modify system partititons to avoid detection
export OVERLAY_MODE=0

# Set to true to mount overlayfs on subdirectories of /system /vendor /system_ext /product,...
# instead of on /system /vendor /system_ext /product,...
export OVERLAY_LEGACY_MOUNT=true