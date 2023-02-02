# Magisk Overlayfs

> This is still Work-in-process

- Emulate your system partitions become read-write-able by using OverlayFS
- Make system partition (`/system`, `/vendor`, `/product`, `/system_ext`) become read-write.
- Use `/data` as upperdir for overlayfs
- All modifications to overlayfs partition will not be made directly, but will be stored in upperdir, so it is easy to revert.

## Build

There is two way:
- Fork this repo and run github actions
- Run `bash build.sh` (On Linux/WSL)

## Bugreport

- Please include `/cache/overlayfs.log`

## Without Magisk

- Possible to test:

```bash
mkdir -p /data/overlayfs
./overlayfs_system /data/overlayfs
```
