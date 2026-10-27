#!/bin/sh
# Combine buildroot's rootfs.cpio.gz with the kernel modules into one ramdisk.
# usage: mkramdisk.sh <linux src> <kbuild dir> <rootfs.cpio.gz> <out.cpio.gz>
set -e
SRC=$1; KB=$2; ROOTFS=$3; OUT=$4
TMP=$(mktemp -d)
make -s -C "$SRC" O="$KB" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
	INSTALL_MOD_PATH="$TMP" INSTALL_MOD_STRIP=1 modules_install
rm -f "$TMP"/lib/modules/*/build "$TMP"/lib/modules/*/source
(cd "$TMP" && find lib | cpio -o -H newc -R 0:0 --quiet | gzip -9) > "$TMP/modules.cpio.gz"
cat "$ROOTFS" "$TMP/modules.cpio.gz" > "$OUT"
rm -rf "$TMP"
ls -l "$OUT"
