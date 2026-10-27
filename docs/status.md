# Status (2026-10-27)

## On the clock now
Flashed to eMMC, nothing else touched (preloader, lk, tee, nvram, nvdata as stock):

| partition | content |
|---|---|
| boot | shim + stock 4.14 kernel, `abs.root=super` (normal power-up) |
| recovery | shim, `abs.shell`: rescue shell on the USB serial console (has `reboot2`) |
| super | buildroot root filesystem (ext4, read-only, RAM overlay on top) |
| userdata | `/data` (ext4): Wi-Fi config, app settings, logs, ssh host keys, Wi-Fi NVRAM copy |

LineageOS is gone from `super`; the full backup in `backup/` restores it via BROM.

## Verified on a clean boot (no buttons, no manual steps)
- Boots to the Nightstand app (Wi-Fi setup screen), framebuffer checked from the Mac
- Wi-Fi up with the factory calibration and real MAC `9c:9d:7e:af:2b:7a`; scan lists the home networks
- `/data` persistent; dropbear (key login), ntpd, haveged running
- Reflashing without buttons: `reboot2 bootloader` from the console, then fastboot

## Fixed overnight
| symptom | cause | fix |
|---|---|---|
| boot hung, no console | `udhcpc -t 0` retries forever in the foreground | `-t 3`, Wi-Fi brought up in the background |
| `/data` was RAM only | `/dev` empty when S01data ran | `mdev -s` in inittab sysinit |
| wpa_supplicant died | `/dev/null` was a regular file (redirect before mdev) | same |
| app stuck, black screen | `getrandom()` blocks: no entropy source on 4.14 | haveged |
| UI sideways, cropped | Qt 6 linuxfb ignores `rotation=` | app option `--rotate 270` turns the stage |
| fake "NVRAM WARNING" network, random MAC | driver gets no calibration | copy from `nvdata`, pass via `WR-BUF:NVRAM` like Android's HAL |
| dark screen | night mode (1/4) on a 160/255 base; clock thinks it's 02:00 in 2020 (no NTP yet) | base 255 on every app start (night = 63); RTC written after NTP sync |
| clock synced late | ntpd started before Wi-Fi had DNS | udhcpc hook restarts ntpd on `bound` (flashed) |
| Qt locale warning | no UTF-8 locale | generate `C.UTF-8` |

## Confirmed in use
- Wi-Fi (WLAN-Birnbach, DHCP 192.168.1.180, ssh `root@abs-clock`), touch, speaker, screen brightness, buttons (MicMute toggles the screen)
- NTP synced and the RTC holds the real time, so it is right from boot on

## Sensors and watchdog (added 2026-10-27 evening)
- **Light/proximity sensor** EM3071X (I2C 0-0024), read via `/sys/bus/platform/drivers/als_ps/{als,ps}`.
  The light part is enabled through the MediaTek sensor framework (`4,1` > `alsactive`); the framework's
  proximity enable fails, so the app sets bit 0x80 in the chip's `config` register.
  Readings behind the glass: light ~1 dark, 2 lamp, 6 ceiling light, 800-3000 flashlight;
  proximity 0-1 idle, 0x3a-0x82 hand, <0x28 flashlight.
- **Auto-brightness** (Settings → Screen brightness): log curve, lamp ~50 %, ceiling ~70 %, daylight full.
- **Night mode "When dark"**: dark for 30 s → night; light again for 5 s → day.
- **Hand near a dark screen** shows the dim clock (proximity is only polled while the screen is off:
  each read logs a kernel line).
- **App watchdog** `nightstand-watch` (inittab): heartbeat `/run/nightstand/alive` stuck 60 s → kill app,
  stuck again → reboot. Tested with SIGSTOP: restarted after ~60 s. Kernel hangs: MediaTek `wdk` + panic=5.

## Still open
- **USB:** the micro-USB contact at the clock is flaky (data dropped while power stayed);
  replugging both ends and moving the cable brought it back.

## Recovery cheat sheet
- Normal reflash: `python3 tools/clocksh.py 'reboot2 bootloader'`, then `fastboot -S 128M flash super out/final/rootfs.ext4`.
- System hangs: hold Vol+ and Vol− while plugging in → rescue shell, then `reboot2 bootloader`.
- Worse: hold Vol+ while plugging in → BROM, mtkclient (`docs/hardware-research.md`).
