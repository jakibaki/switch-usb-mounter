# WIP Switch usb mounter

This can mount your switches sd-card on your computer over usb.

This is **very much** a work in progress, please don't post it on gbatemp yet, I'll make a post myself once it's ready. I'm just 'releasing' this no so no duplications of efforts happen.

To use you need Python3, Fuse (winfsp works on windows, osxfuse on macOS) and pyusb.

On Windows you need to download and run Zadig from https://zadig.akeo.ie/, run the homebrew on your switch and plug it in and then select and install the libusbK driver for the libnx-device.

To use just start the homebrew on the switch, plug it in and then run `python3 mount.py MOUNTPOINT` with `MOUNTPOINT` being a path on *nix and a drive letter (like `K:`) on windows.

To compile you need libnx with scires usb-fix-pr compiled in.  
(NRO that may or may not be outdated: https://transfer.sh/fJtuw/switch-usb-mounter.nro)

## What works/works not

Reading mostely works fine but not at great speed (around 3 Megabytes/s for me), writing kind of works but is really slow (less than 1 Megabyte/s) and overwriting files doesn't seem to work (need to delete them first).

Editing files works with some texteditors (vim) but not with others (vs code), probably because I'm not implementing getattr properly in some way.