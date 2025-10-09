# Donkey
A minimal Linux container runtime built from scratch using namespaces, cgroups, and pivot_root. 
Educational project to understand how Docker works under the hood.

**Daemon service is not implemented yet so if you exit the shell then the session of the crate will be terminated.**

## Features
- Namespace isolation (PID, Mount, UTS, IPC)
- Overlay filesystem (copy-on-write layers)
- Multiple rootfs support (OpenWRT, Alpine, Ubuntu)
- Manual container lifecycle management

## Requirements
- Linux kernel with namespace support
- Root privileges (sudo)
- tar, gzip for image extraction

## Build & Setup
Build the binary with Makefile and please make a folder for crate files at `/var/lib/donkey`

```bash
mkdir -p /var/lib/donkey /var/lib/donkey/crate /var/lib/donkey/image

make donkey
```

Download [openwrt-general-rootfs-18.06.4-x86-64](https://downloads.openwrt.org/releases/18.06.4/targets/x86/64/openwrt-18.06.4-x86-64-generic-rootfs.tar.gz) or any latest rootfs image of your fav distro.

I've tested three images
- OpenWRT
- Alpine
- Ubuntu (Unable to use host network interface)

```bash

# Add image
sudo ./donkey image openwrt-general-rootfs-18.06.4-x86-64.tar.gz

# Create crate
sudo ./donkey create -name owrt openwrt

# Run the crate
sudo ./donkey run owrt

# Remove crate
sudo ./donkey rm owrt

# List crates
sudo ./donkey ps
```

P.S: I haven't isolate the network part yet so the crates will use host's network interface.