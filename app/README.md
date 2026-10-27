# Nightstand – Audiobookshelf player for the Mi Smart Clock

Qt 6 / Qt Quick app for an 800×480 touchscreen, built from the "Nightstand Player" mockup.
Targets Qt 6.11 (buildroot 2026.08.1); develops fine against newer Qt on macOS.

## Build and run (desktop)

```bash
cmake -S app -B app/build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build app/build
app/build/nightstand --demo            # sample library, simulated audio
app/build/nightstand                   # real server: setup screen on first run
```

Options: `--demo`, `--speed <factor>` (demo playback speed), `--fps` (frame counter, also
logged; use on the device to judge animations), `--fake-wifi` (simulated
wpa_supplicant for the Wi-Fi screens), `--gpu` (default scene graph instead of the software renderer), `--shot <png> [--screen <name>] [--set k=v,...]` (render a
screen to a PNG and exit; used to check the UI against the mockup).

On the desktop, arrow keys ↑/↓ act as the volume keys, `M` as the mute key and ←/→ browse
covers, as in the mockup.

## Bedtime behaviour

- **Sleep timer:** fades the volume out over the last 30 s, then pauses and turns the screen
  off. During the fade the screen shows "Still listening?"; a tap anywhere, a tap on the dark
  screen, or a volume key restarts it as a 5-minute timer.
- **Rewind after sleep:** the first resume after the timer goes back further (Settings →
  Playback, default 1 min) than a normal resume (default 5 s).
- **Speed:** 0.75×–2× from the player's top bar, pitch-corrected where the Qt Multimedia
  backend supports it. "Time left" is shown at the current speed.
- **Night mode** (Settings → Display; scheduled 22:00–07:00 by default): backlight at a quarter,
  a light dim layer, warm standby clock.
- **Burn-in:** the standby and screen-off clocks move to a new spot (±12 px) every minute.
- **A–Z:** button at bottom left of the library; a 7×4 grid of large letter tiles jumps the
  cover flow (or the series/author list). "Now" goes back to the books in progress.

Prepared, to be finished on the device:

- **Hold a volume key to skip** (Settings → Playback, off by default): a short press changes
  volume on release, holding skips and repeats every 0.9 s. Depends on how the gpio-keys
  driver reports auto-repeat and release; check the key events with `evtest` first.
- **Performance:** `--fps`, and Settings → Display → Animations → Reduced (flat cover flow,
  no cross-fades or shadows). Caching icons as layers renders them blank with the software
  renderer, so icons are still drawn as shapes; pre-rendering them is the next step if the
  frame counter says so.

## Languages

English and German, switchable live in Settings → Display → Language (default follows the
system language). Strings use `qsTr()`/`tr()`; translations are in `i18n/nightstand_de.ts`.
After changing UI text, run `cmake --build build --target update_translations` and translate
the new entries (Qt Linguist, or edit the XML). The phone setup page follows the phone's
language. The build needs Qt Linguist tools on the host (buildroot: host-qt6tools with
linguist).

## Sizing for the 85 × 50 mm panel

800 px across 85 mm is about 9.4 px per mm. All sizes come from `qml/Theme.qml`:

| Token | px | mm | Use |
|---|---|---|---|
| `touch` | 72 | 7.7 | minimum touch target (buttons, list rows, tabs, segments) |
| `touchLarge` | 96 | 10 | main controls (play is 104) |
| `textSmall` | 19 | 2.0 | smallest text anywhere (meta, captions) |
| `textBody` | 22 | 2.3 | labels, list rows, buttons |
| `textTitle` / `textHeading` | 26 / 32 | | item and screen titles |
| `margin` | 24 | 2.5 | edge inset |
| `screenRadius` | 56 | ~6 | rounded panel corners (from a product photo) |

`CornerMask` blacks out the corners in desktop previews and screenshots so clipping shows up
early. Lists add `cornerInset` bottom padding so their last row can scroll clear of the corners.

## Setup: pairing and Wi-Fi

The clock is set up from a phone so nothing needs typing on the 800×480 screen:

1. **Wi-Fi** (only when the clock isn't connected yet). Pick a network on screen and type the
   password with the on-screen keyboard, or tap **Use my phone instead**. That starts an open
   hotspot `Nightstand-XXXX` (via the OS helper below) and shows two QR codes: one joins the
   hotspot, one opens the setup page.
2. **Pair.** The clock shows a 6-digit code and a QR code for `http://<clock-ip>/?code=…`. The
   page served by the clock (`web/index.html`) lists Audiobookshelf servers it found on the LAN
   (port 13378 scan), takes username and password, and the clock signs itself in. It can also
   change the clock's Wi-Fi. **Enter on this screen instead** keeps the keyboard flow.

The code is required for every change, is replaced after a successful pairing or 5 wrong
tries, and the page is only served while the setup screen is open. The password goes to the
clock once for the login call and is never stored; the clock keeps only the ABS tokens.
The setup page is plain HTTP on the LAN, like the ABS connection itself.

Settings → Server → **Pair with phone** re-opens pairing later (e.g. to switch accounts).

### OS contract for the hotspot

`scripts/nightstand-ap start <ssid>` / `stop`, installed as `/usr/libexec/nightstand/nightstand-ap`
(or on `PATH`). If it's missing, the phone option is hidden and Wi-Fi is set on screen. The
script is a hostapd + dnsmasq starting point and is untested: it depends on AP mode in the
MT8167 Wi-Fi driver. Station-mode Wi-Fi uses `wpa_cli` against a running wpa_supplicant with
`update_config=1` so joined networks persist.

## Layout

| Path | What |
|---|---|
| `src/AbsClient` | ABS REST: `/status`, `/login` (access + refresh tokens on ABS ≥ 2.26, legacy token otherwise), `/auth/refresh` on 401 |
| `src/Library` | Libraries, minified items, `/api/me` progress and bookmarks, lazy item detail (chapters), series/author grouping |
| `src/PlaybackController` | Play sessions (`/api/items/:id/play`), multi-track books, resume with rewind, chapter skip, seek, sleep timer (minutes / end of chapter), `/api/session/:id/sync` every 15 s and on pause, `/close` on switch/quit |
| `src/audio/AudioBackend.h` | Backend seam. `QtAudioBackend` (Qt Multimedia → GStreamer/ALSA on the device), `SimulatedBackend` (demo). A libmpv backend can be added here |
| `src/CoverProvider`, `src/Palette` | Covers fetched as resized JPEG via the authenticated client (disk cache, 6 MB in-memory byte cache); per-book colours derived from the cover in OKLCH like the mockup; background glow rendered once per book at quarter resolution |
| `src/DeviceControl` | Idle tracking, hardware keys, backlight (`/sys/class/backlight`), Wi-Fi scan/join via `wpa_cli`, setup hotspot hooks |
| `src/PairingServer`, `web/index.html` | Pairing code, minimal HTTP server (QTcpServer, no extra Qt module), QR codes (vendored Nayuki qrcodegen, MIT), LAN server discovery |
| `qml/` | Screens: setup (with on-screen keyboard), library (cover flow, series, authors), detail, now playing, standby clock, settings, side panels, volume OSD, screen-off/tap-to-wake |

## Target notes (for the buildroot package)

- Needs host Qt Linguist tools (`lrelease`) for the translations.
- Needs `wpa_supplicant` with `wpa_cli`; for the phone hotspot also `hostapd` and `dnsmasq`.
- Needs `qt6base` (gui, network, linuxfb or eglfs, jpeg, png), `qt6declarative`, `qt6multimedia` with
  the **gstreamer** plugin and ALSA (avoids the FFmpeg plugin's footprint), plus GStreamer plugins
  for the server's audio formats (mp3/m4b/aac/flac/opus: gst1-plugins-good/-bad/-ugly or gst1-libav),
  and souphttpsrc for HTTP streaming.
- Rendering is forced to the software scene graph in `main.cpp`; no shaders or effects are used.
  Cover Flow's 3D tilt uses `Rotation` with a Y axis, which the software renderer draws as a
  projective QPainter transform. Check it on the device; if it's too slow, set `angle: 0`.
- Run with `QT_QPA_PLATFORM=linuxfb` (add `:rotation=90` if the panel is natively portrait) and
  `QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS` / `QT_QPA_EVDEV_KEYBOARD_PARAMETERS` pointing at the
  touch and gpio-keys devices. The app maps `Qt::Key_VolumeUp`, `Key_VolumeDown` and
  `Key_MicMute`; verify the evdev keymap emits these for `KEY_MICMUTE`.
- Settings live in `QSettings` (`~/.config/abs-clock/nightstand.conf`), HTTP cache in
  `~/.cache/abs-clock/nightstand/http` – point `HOME`/`XDG_*` at writable storage.
- Volume is software volume in the player; the ALSA mixer is left at a fixed level.

## Tested against a real server

Audiobookshelf 2.37.1 (local auth), 435-book library, paired from a phone: library, covers and
cover colours, series grouping, item detail, streaming a multi-file book (Qt Multimedia on
macOS), resume, chapter skip, seeking within and across files, and progress sync (position
survives an app restart). Not yet exercised live: bookmark add/delete, access-token refresh
after expiry, and the session `/close` on quit (the request may not leave before the process
exits; pause syncs first).

## Not done yet

- Setup hotspot (`nightstand-ap`) is untested and depends on the Wi-Fi driver supporting AP mode.
- Wi-Fi joining is tested only against a simulated wpa_supplicant (`--fake-wifi`).
- Offline progress queue: syncs that fail while the server is unreachable are dropped (the
  next successful sync carries the position).
