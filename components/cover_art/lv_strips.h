#pragma once

// Every LVGL call of this component lives here, so moving to LVGL 9 (renamed
// image API, 24-bit lv_color_t) only touches lv_strips.h/.cpp.

#include <cstddef>
#include <cstdint>

#include <lvgl.h>

namespace esphome {
namespace cover_art {

static constexpr uint8_t MAX_STRIPS = 16;
// An indexed strip starts with its 256-colour palette (lv_color32_t each)
static constexpr size_t PALETTE_BYTES = 256 * sizeof(lv_color32_t);

struct StripLayout {
  uint16_t width{0};   // pixels per row
  uint16_t rows{0};    // rows per strip
  uint8_t count{0};    // number of strips
  bool indexed{false};  // 8-bit fixed palette instead of RGB565
  uint8_t zoom{1};     // 2 = drawn at double size

  uint16_t height() const { return this->rows * this->count; }
  size_t pixel_bytes() const { return size_t(this->width) * this->rows * (this->indexed ? 1 : 2); }
  size_t strip_bytes() const { return this->pixel_bytes() + (this->indexed ? PALETTE_BYTES : 0); }
  size_t total_bytes() const { return this->strip_bytes() * this->count; }
};

// RGB565 in LVGL's byte order (ESPHome builds LVGL 8 with LV_COLOR_16_SWAP)
inline uint16_t pack_rgb565(uint8_t r5, uint8_t g6, uint8_t b5) {
  lv_color_t c = lv_color_make(r5 << 3, g6 << 2, b5 << 3);
  return c.full;
}

// Write the fixed RGB332 palette (index = rrrgggbb) to the start of an indexed strip
void write_palette_332(uint8_t *strip);

// Create one image per strip inside parent, stacked top to bottom
void attach_strips(lv_obj_t *parent, const StripLayout &layout, uint8_t *const *bufs, lv_img_dsc_t *dscs);

// Redraw the strips after their pixels changed
void invalidate_strips(lv_obj_t *parent);

}  // namespace cover_art
}  // namespace esphome
