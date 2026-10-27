#!/bin/sh
# Pack a boot image with Xiaomi's stock kernel (4.14, 32-bit) and our ramdisk.
# usage: mkboot-stock.sh <ramdisk.cpio.gz> <out.img> [extra cmdline]
#
# kernel and dtb come unchanged from the stock boot partition
# (board/x04g-stock/kernel, extracted from backup/partitions/boot.bin).
# Addresses as stock. The ramdisk has to stay at 0x55000000 (LK's own
# reservation) and below ~16 MB: anywhere else LK passes a bogus
# initrd range ("INITRD: 0x00001000+0x48001000 is not a memory region"),
# and bigger ones overlap LK.
set -e
RD=$1; OUT=$2; EXTRA=$3
K=$(dirname "$0")/../br-external/board/x04g-stock/kernel

CMDLINE="bootopt=64S3,32N2,32N2 loglevel=4 $EXTRA"

PYTHONPATH="$(dirname "$0")/pylib" mkbootimg \
	--header_version 2 \
	--kernel "$K/zImage-dtb" \
	--ramdisk "$RD" \
	--dtb "$K/mt8167.dtb" \
	--base 0x40000000 \
	--kernel_offset 0x00008000 \
	--ramdisk_offset 0x15000000 \
	--tags_offset 0x14000000 \
	--dtb_offset 0x14000000 \
	--os_version 10.0.0 \
	--os_patch_level 2021-06 \
	--pagesize 2048 \
	--cmdline "$CMDLINE" \
	-o "$OUT"
python3 "$(dirname "$0")/fix-bootimg-header.py" "$OUT"
ls -l "$OUT"
