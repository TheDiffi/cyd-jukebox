# CYD Jukebox — Guide Notes (Interview in Progress)

## Elevator Pitch
A simple, ubiquitous, in-the-world music player controller for any Home Assistant media player entity — built on the cheapest possible hardware. Buy a Cheap Yellow Display (~12€), 3D print a case, flash the firmware, and you're ready to go.

## Hardware

### The CYD (Cheap Yellow Display)
- ESP32-equipped 2.8" resistive touchscreen display module — a complete PCB with everything on board
- Very popular in the ESPHome community for being a complete, extremely cheap package
- The PCB is yellow — hence the name
- Reference: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display
- **Buy:** look for model number **ESP32-2432S028** to verify compatibility
- Recommended seller: Tenstar Robot on AliExpress — https://it.aliexpress.com/item/1005006315444628.html
- Alternatively: some sellers offer CYD + case bundles for slightly more

### 3D Printed Case
- Currently recommended print: https://www.printables.com/model/793451-esp32-2432s028-usb-c-module-case-cheap-yellow-disp
- **IMPORTANT:** the firmware is designed for **vertical (portrait) orientation** — horizontal is not currently supported
- Mounting: wall mount recommended (author uses double-sided tape)
- No official case design yet — future plan to create a custom one that fits the retro aesthetic and publish it on Printables
- Community suggestions for mounting solutions welcome
- No 3D printer? Look for CYD listings that include a case

## Software Dependencies

### Required
- **Home Assistant** — the project works with any `media_player` entity in HA
- **ESPHome** — used to build and flash the firmware. Options:
  - ESPHome Device Builder (HA add-on, recommended): https://esphome.io/guides/getting_started_hassio/
  - ESPHome CLI (Python, local)
- **AppDaemon** (HA add-on) — required for album art (details TBD in setup section)

### Recommended (not required)
- **Music Assistant** — the author's preferred media player, pairs very well with this project

## Setup Overview (High-Level — details TBD)
1. Get the project files
2. Edit config (Wi-Fi, HA API details, media player entity)
3. Flash ESPHome to the CYD
4. Add device to Home Assistant and allow it to perform HA actions ("Allow the device to perform Home Assistant actions")
5. Album art setup:
   - Install AppDaemon HA add-on
   - Add the AppDaemon script (file TBD — to be added to repo)
   - Restart HA
6. (Optional) Find playlist URLs/IDs and add them to the playlists page

> Note: Getting album art working is the trickiest part of setup.

## UI & Features

### Aesthetic
- Pixelated / retro style (first working version — plenty of room to improve)

### Screens
- **Main playback screen:** album art background with gradient overlay, song title, artist name
- **Playlist browser:** user-defined list of playlists (scrollable)
- **Idle/sleep screen:** dark screen with a play icon — tap to wake

### Controls
- Play / Pause
- Skip to next track
- Volume control (slider, toggled by volume button)
- 2 quick-play buttons (instantly start a predefined playlist)
- Browse button (opens playlist page)
- Power/stop button

### Behaviour
- Reflects current player state — changes made on other devices are mirrored
- **Auto-dim:** display dims after a short period of no interaction
- **Auto-sleep:** display turns off when the media player entity is idle. A tap wakes both the display and the media player
- **Album art:** displays the current song's album artwork
- **Burn-in prevention (pixel training):** runs nightly at 02:05, 03:05, 04:05 for 30 minutes each — https://esphome.io/cookbook/lvgl/#prevent-burn-in-of-lcd

## Future Plans

### In Progress / Todo
- **Media player switching:** a button opens a list of available media players; selecting one switches control to that player. Open question: should the current queue be transferred to the new player (like Music Assistant's "send queue to...") or kept independent?
- **Improve the pixel/retro look:**
  - Icons are pixel art but appear blurry due to upscaling/anti-aliasing — needs investigation
  - Borders and UI elements could look more retro
- **Custom case design** that matches the retro aesthetic — to be published on Printables

### Further Future
- Horizontal (landscape) orientation UI
- Make playlist setup easier, or replace the static playlist page with dynamic music browsing

## Target Audience & Skill Level
- No programming skills required
- Intended for anyone slightly technical who can follow a tutorial
- Trickiest part: album art setup (AppDaemon script)

## Community
- Contributions welcome: PRs, feedback, suggestions, case designs, anything
- Bug reports and issues via GitHub

## Credits & Licensing
- Project by Diffi — all original work
- **MemoryIcons font** by Pictogrammers, licensed under Apache License 2.0 — https://pictogrammers.com
