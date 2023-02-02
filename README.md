# Magisk Overlayfs (WIP)

- Only support Magisk Delta for overlayfs compatible with modules.
- Make system partition (`/system`, `/vendor`, `/product`, `/system_ext`) become read-write.
- Use `/data` as upperdir

## Build

There is two way:
- Fork this repo and run github actions
- Run `bash build.sh` (On Linux/WSL)

## Without Magisk

- Possible to test:

```bash
mkdir -p /data/overlayfs
./overlayfs_system /data/overlayfs
```