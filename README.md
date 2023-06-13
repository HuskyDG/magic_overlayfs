# Magisk Overlayfs

On Android 10+, system partitions might no longer be able to remount as read-write. For devices use dynnamic partition, it is nearly impossible to modify system partiton as there is no space left. This module solves these problem by using OverlayFS. So what is OverlayFS? On [Wikipedia](https://en.m.wikipedia.org/wiki/OverlayFS):

> OverlayFS is a union mount filesystem implementation for Linux. It combines multiple different underlying mount points into one, resulting in single directory structure that contains underlying files and sub-directories from all sources. Common applications overlay a read/write partition over a read-only partition, such as with LiveCDs and IoT devices with limited flash memory write cycles.

Benefits of using overlayfs for system partitions:

- Make most parts of system partition (`/system`, `/vendor`, `/product`, `/system_ext`, `/odm`, `/odm_dlkm`, `/vendor_dlkm`, ...) become read-write.
- `/data` storage is used for `upperdir` of OverlayFS mount. However, on some kernel, f2fs is not supported by OverlayFS and cannot be used directly. The workaround is to create an ext4 loop image then mount it.
- All modifications to overlayfs partition will not be made directly, but will be stored in upperdir, so it is easy to revert. Just need to remove/disable module so your system will return to untouched stage.
- Support Magisk version 23.0+ and latest version of KernelSU

> If you are interested in OverlayFS, you can read documentation at <https://docs.kernel.org/filesystems/overlayfs.html>

## Build

There is two way:

### GitHub Actions
- Fork this repo and run github actions

### Linux/WSL

1. Clone this repo to your device
```
git clone http://github.com/HuskyDG/Magisk_OverlayFS && cd Magisk_OverlayFS
```
2. Setup Android NDK in repository directory
```
wget https://dl.google.com/android/repository/android-ndk-r23b-linux.zip
unzip android-ndk-r23b-linux.zip
```
3. Run `bash build.sh`

## KernelSU problem

- The KernelSU module is similar to Magisk in that it allows users to modify the system partition while maintaining system integrity. It does this through the implementation of overlayfs. However, it's important to note that KernelSU makes changes to the system partition by using read-only overlayfs, which also mounts on top of magic_overlayfs and prevent system from being remounted as read-write. If you want to remount your system partitions as read-write, you simply need to first unmount the KernelSU overlayfs using this command:

```bash
nsenter -t 1 -m sh
overlayfs_system --unmount-ksu
```

or set `DO_UNMOUNT_KSU=true` in `/data/adb/modules(_update)/magisk_overlayfs/mode.sh`

- After that you will be able to remount system as read-write

## Change OverlayFS mode

- OverlayFS is mounted as read-only by default

- Configure overlayfs mode in `/data/adb/modules(_update)/magisk_overlayfs/mode.sh` to change mode of OverlayFS

> Read-write mode of overlayfs will cause baseband on some devices stop working

```
# 0 - read-only but can still remount as read-write
# 1 - read-write default
# 2 - read-only locked (cannot remount as read-write)

export OVERLAY_MODE=2
```

- OverlayFS upper loop device will be setup at `/dev/block/overlayfs_loop`
- On Magisk, OverlayFS upper loop are mounted at `$(magisk --path)/overlayfs_mnt`. You can make modifications through this path to make changes to overlayfs mounted in system.

## Modify system files with OverlayFS

- If you are lazy to remount, please modify `mode.sh` and set it to `OVERLAY_MODE=1` so overlayfs will be always read-write every boot.

- You can quickly remount all overlayfs to read-write by this command in terminal:
```bash
su -mm -c magic_remount_rw
```

- After that you can restore all system partitons back to read-only mode by this commamd in terminal:
```bash
su -mm -c magic_remount_ro
```


## Overlayfs-based Magisk module

- If you want to use overlayfs mount for your module, add these line to the end of `customize.sh`:

```bash
OVERLAY_IMAGE_EXTRA=0     # number of kb need to be added to overlay.img
OVERLAY_IMAGE_SHRINK=true # shrink overlay.img or not?
INCLUDE_MAGIC_MOUNT=false # enable legacy Magisk mount

# Only use OverlayFS if Magisk_OverlayFS is installed
if [ -f "/data/adb/modules/magisk_overlayfs/util_functions.sh" ] && \
    /data/adb/modules/magisk_overlayfs/overlayfs_system --test; then
  ui_print "- Add support for overlayfs"
  . /data/adb/modules/magisk_overlayfs/util_functions.sh
  support_overlayfs && rm -rf "$MODPATH"/system
fi
```

## Bugreport

- Please include `/cache/overlayfs.log`

## Reset overlayfs

- Remove `/data/adb/overlay` and reinstall module

## Without Magisk

- Simple configuration to test:

```bash
# - Create a writeable directory in ext4 (f2fs) /data
# which will be used for upperdir
# - On some Kernel, f2fs is not supported by OverlayFS
# and cannot be used directly
WRITEABLE=/data/overlayfs

mkdir -p "$WRITEABLE"

# - Export list of modules if you want to load mounts by overlayfs
# - If you have /vendor /product /system_ext as seperate partitons
# - Please move it out of "system" folder, overwise **BOOM**
export OVERLAYLIST=/data/adb/modules/module_a:/data/adb/modules/module_b

# - If there is Magisk, export this in post-fs-data.sh (before magic mount):
export MAGISKTMP="$(magisk --path)"

# - Load overlayfs
./overlayfs_system "$WRITEABLE"
```

## Source code

- <http://github.com/HuskyDG/Magisk_OverlayFS>
