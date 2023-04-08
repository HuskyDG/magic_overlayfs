# Magisk Overlayfs

- Make system partition (`/system`, `/vendor`, `/product`, `/system_ext`) become read-write. Important: you can only modify content in subdirectories of partitions. It's known that cover entire partitions with overlayfs will cause some problem.
- Use `/data` as upperdir for overlayfs to store modifications. All modifications to overlayfs partition will not be made directly, but will be stored in upperdir, so it is easy to revert.

## Build

There is two way:
- Fork this repo and run github actions
- Run `bash build.sh` (On Linux/WSL)

## Bugreport

- Please include `/cache/overlayfs.log`

## Reset overlayfs

- Remove `/data/adb/overlay` and reinstall module

## Without Magisk

- Possible to test:

```bash
mkdir -p /data/overlayfs
./overlayfs_system /data/overlayfs
```
