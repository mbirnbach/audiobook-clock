#!/bin/sh
# Flash the appliance onto the clock (LK fastboot mode).
# usage: flash-clock.sh <out dir>
#
# Layout:
#   boot      shim + stock kernel, abs.root=super   (normal power-up)
#   recovery  shim + stock kernel, abs.shell         (rescue shell on USB serial)
#   super     root filesystem, ext4, mounted read-only
#   userdata  /data, formatted to ext4 on first boot (S01data)
# Everything else (preloader, lk, tee, nvram, ...) is left alone.
#
# To get into fastboot: `reboot2 bootloader` on the clock, or
# `adb reboot bootloader` from Android.
set -e
OUT=$1
for f in boot.img recovery.img rootfs.ext4; do
	[ -f "$OUT/$f" ] || { echo "missing $OUT/$f"; exit 1; }
done
fastboot devices | grep -q . || { echo "no fastboot device"; exit 1; }

fastboot flash recovery "$OUT/recovery.img"
fastboot -S 128M flash super "$OUT/rootfs.ext4"   # LK takes sparse chunks
fastboot flash boot "$OUT/boot.img"
fastboot reboot
