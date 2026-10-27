# audiobook-clock

A Xiaomi Mi Smart Clock (X04G, MediaTek MT8167S) turned into a bedside
[Audiobookshelf](https://www.audiobookshelf.org/) player. It runs a small
Buildroot system on the stock 4.14 vendor kernel, plus a Qt 6 app (Nightstand).

| Directory | What it holds |
|---|---|
| `app/` | Nightstand, the Qt 6 / QML app (library, player, sleep timer, clock, Wi-Fi setup) |
| `os/br-external/` | Buildroot external tree: `x04g_stock_defconfig`, the `nightstand` package, the rootfs overlay (init scripts, Wi-Fi and sensor bring-up, the app watchdog) and the stock kernel with its vendor modules |
| `os/scripts/` | Boot image, boot shim and flashing scripts |
| `os/kernel/` | An earlier attempt with a mainline kernel (not used) |
| `tools/` | `clocksh.py` (shell over the USB serial console) and `reflash-from-rescue.sh` |
| `docs/` | `status.md` (current state, recovery steps), hardware research, the app brief |

## Build

Uses Buildroot 2026.08.1, with this repo checked out next to it as `abs/`:

```sh
cd buildroot
make O=../br-stock BR2_EXTERNAL=../os/br-external x04g_stock_defconfig
cd ../br-stock && make            # make nightstand-rebuild && make after app changes
```

## Flash

From a running clock, no buttons needed (see `docs/status.md` for the recovery paths):

```sh
python3 tools/clocksh.py 'sync; reboot2 bootloader'
fastboot -S 128M flash super br-stock/images/rootfs.ext4
fastboot reboot
```

The micro-USB contact on the clock is flaky. If the serial console disappears
while the clock is still up, reseat the cable at both ends.
