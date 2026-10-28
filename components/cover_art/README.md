# cover_art

An ESPHome component that shows a full-size (240×240) JPEG cover in LVGL on an ESP32 without PSRAM.

A 240×240 RGB565 picture needs 115 KB. On the CYD, the largest free block of RAM is only about 45 KB, and ESPHome's `online_image` also wants the whole JPEG in RAM plus about 17 KB for its decoder. This component avoids both problems:

- **ROM decoder:** the ESP32 has ChaN's TJpgDec in its mask ROM. It needs about 3 KB of working memory and no flash, and it reads the file as a stream, so the JPEG is never stored.
- **Strips:** the picture lives in small strips of 240×15 pixels (7.2 KB each). They can sit anywhere in RAM, and LVGL shows them as one image inside a container you define in YAML.
- **Allocated at boot:** the strips are allocated before WiFi starts, while RAM is still unfragmented, and they are never freed.
- **Background task:** a task on core 0 downloads and decodes. The main loop only redraws once a cover is done, so the UI never stalls. A new request cancels the one in flight.
- **256 → 240:** HA's camera proxy delivers 256×256 covers made of 16×16 blocks. Each block is shrunk to 15×15, so every row of blocks fills exactly one strip. Other sizes are scaled with nearest neighbour, using TJpgDec's 1/2–1/8 pre-scaling first.
- **Dithering:** a 4×4 ordered dither when reducing to RGB565 (or to the palette) avoids banding on dark covers.
- **Visible rows only:** set `height` to skip rows that other widgets cover. Decoding stops after the last visible row.
- **Pixel art:** with `pixel_size: N` the cover is decoded to (size/N)² pixels and each is drawn as a flat N×N block. TJpgDec's built-in 1/2–1/8 scaling does most of the shrinking and averages the source rather than skipping pixels. Dithering works per block, so blocks stay one colour. `set_pixel_size()` can change it at runtime; it applies from the next fetch.

## Configuration

```yaml
external_components:
  - source: github://TheDiffi/cyd-jukebox@master
    components: [cover_art]

cover_art:
  id: cover
  container: lv_cover_box   # an LVGL obj, 240x240, placed below everything that should draw on top
  url: "http://homeassistant.local:8123/api/camera_proxy/camera.my_cover?width=240&height=240"
  authorization: !secret ha_token   # "Bearer <long-lived token>"
  size: 240        # output width/height, multiple of 15
  height: 195      # rows that are actually visible (default: size)
  mode: auto       # auto | rgb565 | palette | half
  fit: scale       # scale: whole picture; crop: centre at 1:1
  pixel_size: 1    # draw the cover as N x N blocks (1 = full resolution)
  dither: true
  contrast: 1.0
  saturation: 1.0
```

`mode: auto` tries the modes in this order and keeps the first one that leaves at least 96 KB free before WiFi starts:

| Mode    | Picture                          | RAM for 240×195 |
| ------- | -------------------------------- | --------------- |
| rgb565  | full colour                      | 93.6 KB         |
| palette | 256 fixed colours (RGB332), dithered | 51.9 KB     |
| half    | 120 px, drawn at 2× zoom         | 21.6 KB         |

## Actions

- `cover_art.fetch: cover`: download the configured URL. Accepts an optional `url:` (templatable) for a one-off source.
- `cover_art.test_pattern: cover`: colour bars plus a white/red line at the top and bottom of every strip. Seams show up as uneven lines.
- `cover_art.clear: cover`: black.

## Limits

- The original ESP32 only. Other chips have different ROM decoders.
- LVGL 8 (ESPHome ≤ 2026.3). All LVGL calls are in `lv_strips.cpp`, so moving to LVGL 9 only touches that file.
- Baseline JPEGs only. TJpgDec can't decode progressive files.
- While a cover is decoding, a part of the screen that happens to redraw (e.g. the song title) briefly shows the half-finished cover behind it. The whole cover appears at once when decoding finishes.
- Plain HTTP. HTTPS is untested, and a TLS handshake needs more RAM than the CYD has left.
