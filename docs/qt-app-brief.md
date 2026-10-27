# Qt app – brief for the app session

Context: we are building an Audiobookshelf (ABS) player appliance on a Xiaomi Mi Smart Clock
(X04G, MediaTek MT8167S, 4× Cortex-A35, 1 GB RAM, 800×480 touchscreen, mono speaker).
The OS is a custom buildroot image developed in a separate session (see
`docs/hardware-research.md`). This session builds the Qt app independently; the two are
merged later by adding the app as a buildroot package.

## What to build now (on the Mac, in an 800×480 window)

- ABS API client: login/token, libraries, items, chapters, streaming URLs, progress sync.
- UI from the user's mockup (once provided).
- Playback logic: resume position, chapter skip, seek, sleep timer, progress reporting.

## Target constraints (must hold so the merge is painless)

| Constraint | Requirement |
|---|---|
| Qt version | Qt 6 as shipped by buildroot **2026.08.1** – develop against that version, not the newest Qt. |
| Rendering | No GPU driver: software rendering, 800×480 landscape. Qt Widgets, or Qt Quick with the software renderer. Avoid shaders, blur, heavy animations. |
| Display | Full screen via `linuxfb` or `eglfs`/KMS platform plugin; no window manager. |
| Input | Touch via evdev. Hardware keys arrive as key events: `KEY_VOLUMEUP`, `KEY_VOLUMEDOWN`, `KEY_MICMUTE` (map these in the app). No power button. |
| Audio | Keep playback behind a small interface so the backend can be swapped. Qt Multimedia's FFmpeg backend is heavy for 1 GB RAM; GStreamer or libmpv on ALSA are alternatives. Backend choice is still open. |
| Memory | Aim well below ~300 MB; stream with modest buffers, don't cache whole books in RAM. |
| Network | Wi-Fi only, talks to the user's local ABS server over HTTP. |

## Integration (later)

When the OS boots with display, touch, audio and Wi-Fi working, the app is added as a
buildroot package in this repo and tested on the clock. Keep the build CMake-based with
no desktop-only dependencies.
