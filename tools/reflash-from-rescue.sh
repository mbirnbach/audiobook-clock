#!/bin/sh
# Wait for the clock's rescue shell (recovery partition: hold Vol+ and Vol−
# while plugging in), reboot it into LK fastboot, flash <out dir>, and wait
# for the new system's console.
# usage: tools/reflash-from-rescue.sh out/final
set -e
OUT=$1
cd "$(dirname "$0")/.."
echo "waiting for the rescue shell (abs.shell)..."
until python3 tools/clocksh.py -t 5 'grep -q abs.shell /proc/cmdline' >/dev/null 2>&1; do
	sleep 2
done
echo "rescue shell up, rebooting to fastboot"
python3 tools/clocksh.py -t 5 'sync; reboot2 bootloader' >/dev/null 2>&1 || true
until fastboot devices | grep -q .; do sleep 1; done
sh os/scripts/flash-clock.sh "$OUT"
echo "flashed, waiting for the console"
python3 tools/clocksh.py --wait -t 180
python3 tools/clocksh.py -t 20 'uptime; cat /proc/cmdline | grep -o "abs[^ ]*"; ps | grep -E "nightstand|wpa_s|udhcpc|wmt-init|dropbear" | grep -v grep'
