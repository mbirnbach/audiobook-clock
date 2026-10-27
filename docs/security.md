# Security notes (2026-10-27)

A source-only review of commit `1cc1e93`. Nothing was run, nothing is fixed yet. Fine for a single clock
on a home network; revisit before anyone else runs this on a network they don't control.

## Worth fixing

1. **Server change keeps the old tokens (medium).** `AbsClient::checkServer` saves the new server URL
   before login (`app/src/AbsClient.cpp`, reply handler). If login on the new server fails, the old
   access/refresh tokens stay and go to the new host with the next request (`request()`,
   `refreshAccessToken()`, `streamUrl()`). Reachable via Settings → Pair with phone (with the code) or
   by picking a fake LAN server in discovery and backing out.
   Fix: stage the URL, and commit it together with the new tokens only when login succeeds.
2. **`GET /api/info` needs no pairing code (low).** It shows the ABS username, server URL and nearby
   SSIDs to any LAN host while pairing is open (`PairingServer::handle`). Gate it on the code and send
   the code from `refresh()` in `app/web/index.html`.

## To check on the device

3. **Pairing code has no rate limit.** 900,000 codes; rotating after 5 failures doesn't slow guessing,
   and the pair screen stays open indefinitely (boot when signed out, after session expiry, from
   Settings). How long a brute force takes depends on the clock's request rate. Fix: per-client
   backoff, a time limit on the pairing window.
4. **No Host/Origin/Content-Type check on the pairing server.** A web page in a LAN browser could send
   `text/plain` POSTs or DNS-rebind to it. Require `Content-Type: application/json` and a Host equal to
   the clock's address.
5. **Untrusted text rendered as QML AutoText.** Book titles/authors/chapters (`LibraryScreen`,
   `DetailScreen`, `PlayerScreen`, `SidePanel`, `ClockScreen`, `Cover`) and Wi-Fi SSIDs (`WifiList`,
   `WifiJoin`, `wifiMessage`) may interpret markup such as `<img src=…>`. Fix:
   `textFormat: Text.PlainText` on those Text elements (and on `PairingView.qml` busy/error text).
6. **Wi-Fi password not filtered in the app.** `DeviceControl::connectWifi` passes it to
   `wpa_cli set_network psk "…"`. Safe if the image's wpa_supplicant is ≥ 2.6 (rejects control
   characters); check with `wpa_supplicant -v`. Rejecting non-printable characters app-side is cheap.

## Smaller hardening

- Don't log the pairing code (`PairingServer::setActive`).
- Clear the HTTP disk cache on sign-out or server change; set a `QImageReader` allocation limit for covers.
- `nightstand-ap` would start an **open** hotspot. It can't run today (no hostapd/dnsmasq in
  `x04g_stock_defconfig`), but the "Use my phone instead" button still shows and drops the Wi-Fi when
  tapped. Before enabling it: WPA2 with a random passphrase in the QR code, and only show the button
  when the tools exist.
- The old test image (`x04g_test_defconfig`) runs dropbear `-B` with an empty root password.
- `inittab` gives a root shell on the USB console; acceptable with an unlocked bootloader anyway.
- Prefer HTTPS for the ABS server; plain HTTP exposes tokens on the LAN.

## Looked at and fine

SSH key-only, no committed secrets, root hooks don't put network input into commands, `wpa_cli`
called without a shell and with a hex SSID, pairing page uses `textContent`, `wmt-init` buffers
bounded, build packages local only.
