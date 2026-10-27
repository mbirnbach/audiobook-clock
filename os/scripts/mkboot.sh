#!/bin/sh
# Pack an Android v2 boot image for the X04G's MediaTek LK (64-bit kernel).
# usage: mkboot.sh <kbuild dir> <ramdisk.cpio.gz> <out.img> [extra cmdline]
#
# LK decompresses Image.gz itself and takes the DTB appended to it; the DTB is
# also put in the v2 dtb section, like the stock image. Addresses:
#   kernel  0x40080000
#   ramdisk 0x48000000  (LK rejects 0x55000000 for ramdisks > ~16 MB: overlaps LK;
#                        0x43000000-0x43e00000 is secmon/OP-TEE)
#   tags/dtb 0x54000000 (as stock)
# A v0 image with the DTB as "second" at 0x44000000 (Lenovo-tablet style) got
# less far: only CPU0 ever ran.
set -e
KB=$1; RD=$2; OUT=$3; EXTRA=$4
DTB=$KB/arch/arm64/boot/dts/mediatek/mt8167-xiaomi-x04g.dtb
TMP=$(mktemp -d)
cat "$KB/arch/arm64/boot/Image.gz" "$DTB" > "$TMP/Image.gz-dtb"

CMDLINE="bootopt=64S3,32N2,64N2 console=tty0 console=ttyS0,921600n1 earlycon=uart8250,mmio32,0x11005000 fbcon=rotate:3 loglevel=7 $EXTRA"

PYTHONPATH="$(dirname "$0")/pylib" mkbootimg \
	--header_version 2 \
	--kernel "$TMP/Image.gz-dtb" \
	--ramdisk "$RD" \
	--dtb "$DTB" \
	--base 0x40000000 \
	--kernel_offset 0x00080000 \
	--ramdisk_offset 0x08000000 \
	--tags_offset 0x14000000 \
	--dtb_offset 0x14000000 \
	--pagesize 2048 \
	--cmdline "$CMDLINE" \
	-o "$OUT"
rm -rf "$TMP"
ls -l "$OUT"
