#!/bin/sh
# Build the small boot shim ramdisk from a buildroot target tree.
# usage: mkshim.sh <buildroot output dir> <out.cpio.gz>
set -e
T=$1/target; OUT=$2
SHIM=$(dirname "$0")/../br-external/board/x04g-stock/shim
W=$(mktemp -d)
mkdir -p $W/bin $W/sbin $W/lib $W/dev $W/proc $W/sys $W/tmp
cp -a $T/bin/busybox $W/bin/
for l in ld-linux-armhf.so.3 libc.so.6 libresolv.so.2; do cp -L $T/lib/$l $W/lib/; done
for a in sh mount umount mkdir mknod ls cat grep sed sleep echo setsid head ln switch_root mdev; do
	ln -s /bin/busybox $W/bin/$a
done
ln -s /bin/busybox $W/sbin/mdev
ln -s /bin/busybox $W/sbin/switch_root
cp $SHIM/init $W/init
cp -a $T/usr/sbin/reboot2 $W/bin/   # rescue shell: `reboot2 bootloader` for fastboot
(cd $W && find . | cpio -o -H newc -R 0:0 --quiet | gzip -9) > "$OUT"
rm -rf $W
ls -l "$OUT"
