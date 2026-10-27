# Xiaomi Mi Smart Clock (X04G) – hardware & feasibility notes

Collected 2026-10-26 from the running device (LineageOS/PHH GSI 13 on the stock
Xiaomi vendor + 4.14 kernel) and from public research.

## Device

| Item | Value | Source |
|---|---|---|
| SoC | MediaTek MT8167S, 4× Cortex-A35 @ 1.5 GHz | `/proc/cpuinfo`, DT |
| GPU | PowerVR GE8300 (`rgx.fw.22.40.54.30`) | vendor firmware |
| RAM | 1 GB DDR3 | `/proc/meminfo` |
| eMMC | 4 GB Kioxia (3,959,422,976 bytes user area, 2× 2 MiB boot areas, RPMB) | `/proc/partitions` |
| Display | 800×480 MIPI-DSI, Zhunyi Z40046 / Fitipower JD9161Z controller, reset GPIO66, backlight PWM + enable GPIO43 | stock DT, lscorcia DTS |
| Touch | Chipsemi CHSC6440 (stock name `tlsc6x`) @ i2c2 0x2e, FT6236-compatible | stock DT, lscorcia DTS |
| Keys | Vol+ (SoC keypad `mtk-kpd`), Vol− + Mic-mute (MT6392 PMIC keys) | `getevent` |
| Speaker | SoC internal codec → LINEOUT (voice amp) → external class-D amp, enable = GPIO51 | stock DT pinctrl `audexampon`, `tinymix` |
| Mics | ES7243 ADC @ i2c1 0x13 (TDM) | stock DT |
| Light/prox | EM3071x @ i2c0 0x24 | stock DT |
| Wi-Fi/BT | MT8167 integrated CONNSYS (2.4 GHz b/g/n, BT), vendor `wmt_drv` + `wlan_drv_gen2` stack | `lsmod` |
| PMIC | MT6392 | DT |
| UART | TP30 = TX, TP29 = RX, 1.8 V, 921600 baud (back of board) | pmOS wiki, Informatic |

## Boot chain & partitions

BootROM → preloader (eMMC boot0) → LK (`lk`/`lk2`) → Android boot image v2 in `boot`.

- Bootloader is unlocked (`seccfg`), verified boot state **orange** – LK boots any boot image.
- Stock boot image: header v2, page 2048, kernel @ 0x40008000, ramdisk @ 0x55000000,
  tags @ 0x54000000, 32-bit zImage with appended DTB.
- LK chooses 32/64-bit kernel mode from `bootopt=` in the boot image cmdline
  (stock: `bootopt=64S3,32N2,32N2` → 32-bit kernel; use `64S3,32N2,64N2` for arm64).
- Partition table: see `backup/info/` and GPT in the raw dump. Relevant ones:
  `boot` (16 MiB), `recovery` (16 MiB), `super` (1.5 GiB), `cache` (128 MiB), `userdata` (1.9 GiB).
  Never touch: `preloader` (boot0), `lk`, `lk2`, `seccfg`, `proinfo`, `nvram`, `nvdata`, `persist`, `tee1/2`.

## Recovery / unbrick path

- Full raw dump in `backup/raw/` (boot0, boot1, entire user area).
- **BootROM mode:** hold **Vol+** while plugging in USB → `0e8d:0003`.
  `mtkclient` works on this device (it is how the bootloader was unlocked in the first place)
  and can rewrite any partition or the full eMMC from BROM, independent of what's on flash.
- Vol+ & Vol− on power-up → recovery ("No command"); LK fastboot (`0e8d:201c`) via `adb reboot bootloader`.

**Verified 2026-10-26** with mtkclient v2.1.4.1 (`tools/venv`): BROM → kamakiri payload →
DA (xflash) with eMMC access; `printgpt` output in `backup/info/mtkclient_printgpt.log`.
Target config 0xe5: SBC + DAA enabled, SLA disabled, no root cert → kamakiri bypass works.

Procedure on macOS:
1. Unplug the clock.
2. `cd tools && sudo ./venv/bin/mtk <cmd>` (root is required on macOS, otherwise the
   kamakiri step fails with `Access denied`). Start it *before* plugging in, or it
   asks you to reconnect and exits.
3. Hold Vol+, plug in USB, release when it prints "Device detected".
4. Restore examples: `mtk w boot backup/partitions/boot.bin`,
   or the full user area: `mtk wf backup/raw/mmcblk0.img`.
5. Afterwards, a normal re-plug boots from flash again.

## Dev loop (no flashing)

`adb reboot bootloader`, then `fastboot boot <boot.img>` boots an image from RAM; the
flash is untouched, and a re-plug returns to whatever is installed. Verified with the stock boot image.
**Do not run `fastboot getvar all`**: LK returns garbage at the end and its fastboot
then hangs until power-cycled. Single `getvar <name>` queries are fine
(`version-bootloader: mico_x04g-451d69f-20210619014509-20211214131701`).

## Mainline Linux status (lscorcia/linux, branch `x04g-7.3`)

Luca Leonardo Scorcia maintains a mainline tree for this exact device
(https://github.com/lscorcia/linux), rebased through Linux 7.3, with
display patches being upstreamed. Board DTS: `arch/arm64/boot/dts/mediatek/mt8167-xiaomi-x04g.dts`.

| Function | Mainline status |
|---|---|
| CPU/PMIC/eMMC/USB (gadget, RNDIS) | working |
| Display (DSI + JD9161Z panel driver, PWM backlight) | in tree (patches on list) |
| Touch (edt-ft5x06 / ft6236) | in tree |
| Keys (mtk-kpd + mt6392 pmic keys) | in tree |
| Speaker (MT8167 AFE + codec + ext amp) | in tree, `[WIP]` |
| Mics (ES7243) | in tree, `[WIP]` |
| ALS (em3071x IIO) | in tree |
| GPU (PowerVR) | firmware available; not needed – Qt can render in software at 800×480 |
| **Wi-Fi / BT** | **WIP** – port of vendor CONNSYS driver on branches `consys`, `x04g-7.1-rc6-btwifi` (April 2026), not in `x04g-7.3`; status unverified |

Firmware blobs needed for Wi-Fi/BT/GPU are on the device in `/vendor/firmware`
and mirrored at https://github.com/lscorcia/firmware-xiaomi-x04g.

## Vendor stack (fallback)

`/vendor/lib/modules/{wmt_drv,wmt_chrdev_wifi,wlan_drv_gen2,bt_drv}.ko` + firmware
`ROMv2_lm_patch_1_{0,1}_hdr.bin`, `WIFI_RAM_CODE_8167`, `WMT_SOC.cfg`, plus Android
userspace helpers `wmt_loader`/`wmt_launcher` (bionic). Kernel is the stock 32-bit 4.14.

## References

- https://github.com/lscorcia/linux (x04g branches)
- https://github.com/lscorcia/firmware-xiaomi-x04g
- https://github.com/Informatic/xiaomi-x04g-research (unlock, BROM, hardware)
- https://wiki.postmarketos.org/wiki/Xiaomi_Mi_Smart_Clock_(xiaomi-x04g)
- https://github.com/bkerler/mtkclient
