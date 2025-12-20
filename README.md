# MomentStack
A tool that dynamically adds information to paper business cards.

## Overview
MomentStack pairs a tiny NFC-enabled device with a hosted map page so that a paper business card can carry living content. An M5StickC Plus2 equipped with the RFID 2 Unit writes NFC tags containing a sharable URL. When the tag is scanned, the hosted map opens with the exact coordinates, zoom level, and caption you curated for the recipient.

## Key Capabilities
- NFC writer firmware for M5StickC Plus2 + RFID 2 Unit that encodes Leaflet-based map URLs into Mifare Classic or Ultralight tags.
- Hosted web app (`docs/`) that renders the location, popup text, and shows the edit overlay automatically when a `d=` device parameter is present in the URL.
- Embedded HTTP API on the device (`/api/location`) so the web UI can read/write the active coordinates over the local network.
- QR-guided setup flow: press Button A to start the config server, which displays a QR code directly on the device screen for easy access from your phone.

## Repository Layout
- `Arduino/RFID2_URL_Writer_StickCPlus2/`: firmware source for the NFC writer, including a lightly modified NDEF stack and MFRC522 I2C driver.
- `docs/`: static site (Leaflet + vanilla JS) served via GitHub Pages or any static host. This is what the NFC URL points to.

## Requirements
- M5StickC Plus2 with the M5Stack RFID 2 Unit (WS1850S/MFRC522 I2C) attached to Port A.
- Arduino IDE with **M5Stack board package @ 2.1.3** (verified) or PlatformIO capable of compiling ESP32 firmware.
- Wi-Fi network credentials the device can join (typically your phone's tethering hotspot so the handset and device share a LAN).
- Formatted Mifare Classic or Ultralight NFC tags.

## Firmware Setup
1. Open `Arduino/RFID2_URL_Writer_StickCPlus2/RFID2_URL_Writer_StickCPlus2.ino` in Arduino IDE.
2. Install the required board definitions and libraries (`M5StickCPlus2`). The sketch already bundles the custom NFC helpers.
3. Update `ssid` and `password` near the top of the sketch to match the network you will use during configuration.
4. Optionally change `kConfigPageUrl` if you host the map UI somewhere other than `https://ksasao.github.io/MomentStack/`.
5. Build and flash the sketch to the M5StickC Plus2.

## Configuring Location Data
1. Power the device. It boots into NFC writer mode and shows date/time/battery info.
2. Press Button A (release) to enter **Config Mode**. The firmware connects to Wi-Fi, starts an HTTP server on port 80, syncs time via NTP, and displays a QR code on the screen pointing to `https://ksasao.github.io/MomentStack/?p=@35.681684,139.786917,17z&t=Hello&d=<device-ip>`.
3. Using a phone on the same network, scan the QR code from the device display. The browser loads the hosted map UI with the `d` parameter set to the device's IP address, which unlocks the edit overlay and tells the page how to contact the device API.
4. Drag the map or use the geolocation button to select the spot, change the popup text, then hit **update**. The page sends a GET request to `http://<device-ip>/api/location` with query parameters, saves the values to NVS, and redirects to the new shareable URL.
5. The device automatically restarts within 3 seconds after applying changes and returns to writer mode.

## Writing NFC Tags
1. With the device back in writer mode, place a formatted tag on the RFID 2 Unit.
2. The firmware retrieves the persisted location/text, appends the current date and time, and builds a URL such as `https://ksasao.github.io/MomentStack/?p=@35.681684,139.786917,17z&t=2025-12-20%2014:30%0AHello`, writing it as a single URI record via the on-board MFRC522.
3. Success/failure feedback is shown on screen and through a short tone. The device powers down automatically after 30 seconds of inactivity to save battery.
4. Scanning the tag on any smartphone opens the hosted map with your curated content including the timestamp.

## Web App Reference (`docs/`)
- **Base URL:** `https://ksasao.github.io/MomentStack/` (can be self-hosted).
- **Parameters:**
    - `p=@<lat>,<lng>,<zoom>z` (required) sets the map center and zoom.
    - `t=<text>` supplies popup content. Newlines (`\n`) are converted to `<br>` for readability. HTML tags are escaped to prevent XSS.
    - `d=<device-ip-or-url>` enables the edit overlay/current-location button and points the page at the device that will handle API requests.
- The map is rendered with Leaflet 1.9 and OpenStreetMap tiles; zoom controls appear at the bottom-left.

## Device API
- `GET /api/location` → `{ lat, lng, zoom, text, pos }` representing the values stored in NVS.
- `GET /api/location?lat=<lat>&lng=<lng>&zoom=<zoom>&text=<text>&return=<url>` updates the stored location and text, then redirects the browser. The `return` parameter (optional) specifies the absolute URL for redirection; the `d` parameter is automatically stripped from the return URL.
- `POST /api/location` accepts form-urlencoded fields `lat`, `lng`, `zoom`, `text`, and optional `return` (absolute URL) used for redirecting the browser back to the share link.
- `OPTIONS /api/location` responds with permissive CORS headers so the hosted UI can talk to the device from a different origin while both reside on the same LAN.

## Additional Resources
- Detailed Japanese walkthrough: https://qiita.com/ksasao/items/fb02ea463a9a5fd3823f
- Leaflet docs: https://leafletjs.com/

Feel free to adapt the firmware, host the web UI on your own domain, or extend the URL schema with richer context while keeping the NFC payload lightweight.