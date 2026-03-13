# CYD Jukebox

A physical music controller for your Home Assistant smart home — built on a ~12€ ESP32 display. Stick it on the wall, tap to play.

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)
![ESPHome](https://img.shields.io/badge/built%20with-ESPHome-informational)
![Platform: ESP32](https://img.shields.io/badge/platform-ESP32-green)

---

![CYD Jukebox mounted on a wall](assets/readme-image-1.png)

---

## What is this?

CYD Jukebox turns a [Cheap Yellow Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) into a always-on, wall-mounted music controller for any `media_player` entity in Home Assistant. No app, no phone, no voice commands — just a physical button on your wall.

It's cheap, open, and built entirely on ESPHome. Works with any HA media player out of the box, and pairs especially well with [Music Assistant](https://music-assistant.io/).

---

## Features

- **Album art** — displays the current song's cover art, updated automatically
- **Playback controls** — play/pause, skip, stop
- **Volume control** — tap the volume button to reveal a slider
- **Quick-play buttons** — two configurable shortcuts to instantly start a favourite playlist
- **Playlist browser** — a scrollable page with all your playlists
- **State sync** — reflects changes made from any other device in real time
- **Auto-dim & sleep** — dims after a short idle period; turns off completely when the player is inactive. A tap wakes everything back up
- **Burn-in prevention** — runs pixel training automatically during the night so your display stays healthy

---

![CYD Jukebox UI showing album art and controls](assets/readme-image-2.jpg)

---

## Hardware

### The CYD (Cheap Yellow Display)

The CYD is an ESP32-based 2.8" resistive touchscreen module — everything on a single PCB, no soldering required. It's popular in the ESPHome community for being a complete, capable package at a very low price. (The PCB is yellow. Hence the name.)

**What to buy:** look for the model number `ESP32-2432S028` to make sure you get the right variant.

- Recommended listing: [Tenstar Robot on AliExpress](https://it.aliexpress.com/item/1005006315444628.html) (~12€)
- No 3D printer? Some listings include a basic case for a little extra

### Case

The firmware is designed for **portrait (vertical) orientation**.

There's no official CYD Jukebox case yet — a custom retro-styled one is in the works. In the meantime, this case from Printables works great:
[ESP32-2432S028 USB-C Module Case](https://www.printables.com/model/793451-esp32-2432s028-usb-c-module-case-cheap-yellow-disp)

For mounting, a wall mount is recommended. Double-sided tape works fine. The community is welcome to share their own solutions.

---

## Requirements

- [Home Assistant](https://www.home-assistant.io/) with a configured `media_player` entity
- [ESPHome](https://esphome.io/) — either the [ESPHome Device Builder add-on](https://esphome.io/guides/getting_started_hassio/) (recommended) or the CLI
- [AppDaemon](https://appdaemon.readthedocs.io/) HA add-on — for album art generation

---

## Project Structure

```
cyd-jukebox/
├── cyd-jukebox.yaml              # Main ESPHome config — start here
├── cyd-playlists-page.yaml       # Playlist browser UI (included by main config)
├── cyd-gradient-overlay.yaml     # Gradient overlay widget (included by main config)
├── secrets.yaml.example          # Template for your secrets file
├── fonts/
│   ├── MemoryIcons-Regular.otf   # Pixel icon font (Apache 2.0)
│   └── LICENSE
└── appdeamon/
    ├── cyd-jukebox-album-art-generator.py   # AppDaemon app — fetches & processes album art
    └── apps.py                              # AppDaemon config template
```

---

## Setup

> Full step-by-step guide coming soon.

**Short version:**

1. Copy `secrets.yaml.example` → `secrets.yaml` and fill in your credentials
2. Edit `cyd-jukebox.yaml` — set your `media_player` entity, HA URL, and any playlists
3. Flash the config to your CYD via ESPHome
4. Add the device in Home Assistant and allow it to perform actions
5. Set up the AppDaemon app for album art (see `appdeamon/`)

---

## Roadmap

Things actively being worked on or planned:

- **Media player switching** — select a different speaker from the device itself, with optional queue transfer
- **Better pixel/retro aesthetics** — sharper icons, more retro UI elements
- **Custom case design** — a purpose-built retro case to be published on Printables
- **Landscape orientation** support
- **Dynamic media browsing** — replace the static playlist page with live browsing

---

## Contributing

PRs, issues, suggestions, and case designs are all welcome. If you build one, show it off.

---

## Credits & License

CYD Jukebox is MIT licensed — see [LICENSE](LICENSE).

Uses [MemoryIcons](https://pictogrammers.com) by Pictogrammers, licensed under [Apache License 2.0](fonts/LICENSE).
